
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h> /* snprintf */

#ifndef NOXML

#include <expat.h>
#include <libmx.h>

static lmx_t *lmx__new(int type)
{
	lmx_t *lmx;

	if (!(lmx = (lmx_t *)malloc(sizeof(lmx_t))))
		return NULL;
	memset(lmx, 0, sizeof(lmx_t));

	lmx->lmx_type = type;

	return lmx;
}

static lmx_t *lmx__newtag(const char *name)
{
	lmx_t *lmx;

	if (!(lmx = lmx__new(LMX_TYPE_TAG)))
		return NULL;

	if (!(lmx->lmx_name = strdup(name))) {
		free(lmx);
		return NULL;
	}

	return lmx;
}

static lmx_t *lmx__newcdata(const char *cdata, int cdatalen)
{
	lmx_t *lmx;

	if (!(lmx = lmx__new(LMX_TYPE_CDATA)))
		return NULL;

	/*
	 * We store a \0 terminator in the buffer for convenience's sake,
	 * however, it is not included in lmx->cdatalen.
	 */
	lmx->lmx_cdatalen = cdatalen;
	if (!(lmx->lmx_cdata = malloc(lmx->lmx_cdatalen + 1))) {
		free(lmx);
		return NULL;
	}
	memcpy(lmx->lmx_cdata, cdata, lmx->lmx_cdatalen);
	lmx->lmx_cdata[lmx->lmx_cdatalen] = '\0';

	return lmx;
}

static lmx_attrib_t *lmxattrib__findbyname(lmx_attrib_t *head, const char *name)
{
	lmx_attrib_t *lmxa;

	for (lmxa = head; lmxa; lmxa = lmxa->next) {
		if (strcasecmp(lmxa->name, name) == 0)
			return lmxa;
	}

	return NULL;
}

static lmx_attrib_t *lmxattrib__new(const char *name, const char *value)
{
	lmx_attrib_t *lmxa;

	if (!(lmxa = (lmx_attrib_t *)malloc(sizeof(lmx_attrib_t))))
		return NULL;

	if (!(lmxa->name = strdup(name))) {
		free(lmxa);
		return NULL;
	}

	if (value) {
		if (!(lmxa->value = strdup(value))) {
			free(lmxa->name);
			free(lmxa);
			return NULL;
		}
	} else
		lmxa->value = NULL;

	return lmxa;
}

static void lmxattrib__free(lmx_attrib_t *lmxa)
{

	free(lmxa->value);
	free(lmxa->name);
	free(lmxa);

	return;
}

static void lmxattrib__freeall(lmx_attrib_t *head)
{
	lmx_attrib_t *lmxa;

	for (lmxa = head; lmxa; ) {
		lmx_attrib_t *tmp;

		tmp = lmxa->next;
		lmxattrib__free(lmxa);
		lmxa = tmp;
	}

	return;
}

static void lmx__freeall(lmx_t *head);

static void lmx__free(lmx_t *lmx)
{

	if (lmx->lmx_type == LMX_TYPE_TAG) {

		lmxattrib__freeall(lmx->lmx_attribs);
		lmx__freeall(lmx->lmx_children);
		free(lmx->lmx_name);

	} else if (lmx->lmx_type == LMX_TYPE_CDATA) {

		free(lmx->lmx_cdata);

	}
	free(lmx);

	return;
}

static void lmx__freeall(lmx_t *head)
{
	lmx_t *lmx;

	for (lmx = head; lmx; ) {
		lmx_t *tmp;

		tmp = lmx->lmx_next;
		lmx__free(lmx);
		lmx = tmp;
	}

	return;
}

lmx_t *lmx_new(const char *name) /* exported */
{
	lmx_t *lmx;

	if (!name)
		return NULL;

	if (!(lmx = lmx__newtag(name)))
		return NULL;

	return lmx;
}

const char *lmx_get_name(lmx_t *lmx)
{

	if (!lmx || (lmx->lmx_type != LMX_TYPE_TAG))
		return NULL;

	return lmx->lmx_name;
}

lmx_t *lmx_free(lmx_t *lmx) /* exported */
{
	lmx_t *parent;

	if (!lmx)
		return NULL;

	parent = lmx->lmx_parent;
	lmx__free(lmx);

	return parent;
}

static lmx_t *lmx__findbyname(lmx_t *head, const char *name)
{
	lmx_t *lmx;

	for (lmx = head; lmx; lmx = lmx->lmx_next) {
		if (lmx->lmx_type == LMX_TYPE_TAG) {
			if (strcasecmp(lmx->lmx_name, name) == 0)
				return lmx;
		}
	}

	return NULL;
}

lmx_t *lmx_get_tag(lmx_t *parent, const char *tagname) /* exported */
{
	lmx_t *lmx;

	if (!parent || !tagname || (parent->lmx_type != LMX_TYPE_TAG))
		return NULL;

	return lmx__findbyname(parent->lmx_children, tagname);
}

lmx_t *lmx_get_parent(lmx_t *lmx) /* exported */
{

	if (!lmx)
		return NULL;

	return lmx->lmx_parent;
}

static void lmx__append(lmx_t *parent, lmx_t *x)
{

	x->lmx_parent = parent;
	x->lmx_next = NULL;

	if (parent->lmx_children_tail) {
		parent->lmx_children_tail->lmx_next = x;
		parent->lmx_children_tail = parent->lmx_children_tail->lmx_next;
	} else
		parent->lmx_children = parent->lmx_children_tail = x;

	return;
}

lmx_t *lmx_add(lmx_t *parent, const char *name) /* exported */
{
	lmx_t *lmx;

	if (!parent || !name || (parent->lmx_type != LMX_TYPE_TAG))
		return NULL;

	if (!(lmx = lmx__newtag(name)))
		return NULL;

	lmx__append(parent, lmx);

	return lmx;
}

/* XXX this might be wrong in regard to cdata */
lmx_t *lmx_add_lmx(lmx_t *dest, lmx_t *src) /* exported */
{
	lmx_t *x = NULL;

	if (src->lmx_type == LMX_TYPE_TAG) {
		lmx_t *y;
		lmx_attrib_t *xa;

		if (!(x = lmx_add(dest, src->lmx_name)))
			return NULL;

		for (xa = src->lmx_attribs; xa; xa = xa->next)
			lmx_add_attrib(x, xa->name, xa->value);

		for (y = src->lmx_children; y; y = y->lmx_next)
			lmx_add_lmx(x, y);

	} else if (src->lmx_type == LMX_TYPE_CDATA) {

		lmx_add_cdata_bound(dest, src->lmx_cdata, src->lmx_cdatalen);

	}

	return x;
}

int lmx_add_cdata(lmx_t *lmx, const char *cdata) /* exported */
{
	return lmx_add_cdata_bound(lmx, cdata, strlen(cdata));
}

int lmx_add_cdata_bound(lmx_t *lmx, const char *cdata, int cdatalen) /* exported */
{
	lmx_t *x;
	char *buf;
	int buflen;

	if (!lmx || !cdata || (cdatalen <= 0) || (lmx->lmx_type != LMX_TYPE_TAG))
		return -1;

	if (!(x = lmx__newcdata(cdata, cdatalen)))
		return -1;

	lmx__append(lmx, x);

#if 0
	buflen = cdatalen + lmx->cdatalen + 1;

	if (!(buf = (char *)malloc(buflen)))
		return -1;

	if (lmx->cdatalen)
		memcpy(buf, lmx->cdata, lmx->cdatalen);
	memcpy(buf + lmx->cdatalen, cdata, cdatalen);
	buf[buflen-1] = '\0';

	free(lmx->cdata);
	lmx->cdata = buf;
	lmx->cdatalen = buflen - 1; /* do not count terminator */
#endif

	return 0;
}

int lmx_add_attrib(lmx_t *lmx, const char *name, const char *value) /* exported */
{
	lmx_attrib_t *lmxa;

	if (!lmx || !name || (lmx->lmx_type != LMX_TYPE_TAG)) /* value optional (?) */
		return -1;

	if (!(lmxa = lmxattrib__new(name, value)))
		return -1;

	lmxa->next = lmx->lmx_attribs;
	lmx->lmx_attribs = lmxa;

	return 0;
}

/*
 * The concept of this method is technically flawed, but practically quite
 * useful.  It will return only the first CDATA segment in the tag.  This
 * works fine 95% of the time, as CDATA isn't usually mixed with tags.
 *
 * Also, it's already flawed in that it returns a string without a length.
 */
const char *lmx_get_cdata(lmx_t *lmx) /* exported */
{
	lmx_t *x;

	if (!lmx || (lmx->lmx_type != LMX_TYPE_TAG))
		return NULL;

	for (x = lmx->lmx_children; x; x = x->lmx_next) {
		if (x->lmx_type == LMX_TYPE_CDATA)
			return x->lmx_cdata;
	}

	return NULL;
}

const char *lmx_get_cdata_oftag(lmx_t *parent, const char *tagname) /* exported */
{
	lmx_t *lmx;

	if (!parent || !tagname || (parent->lmx_type != LMX_TYPE_TAG))
		return NULL;

	if (!(lmx = lmx__findbyname(parent->lmx_children, tagname)))
		return NULL;

	return lmx_get_cdata(lmx);
}

const char *lmx_get_attrib(lmx_t *lmx, const char *name) /* exported */
{
	lmx_attrib_t *lmxa;

	if (!lmx || !name || (lmx->lmx_type != LMX_TYPE_TAG))
		return NULL;

	if (!(lmxa = lmxattrib__findbyname(lmx->lmx_attribs, name)))
		return NULL;

	return lmxa->value;
}

int lmx_add_expatattribs(lmx_t *lmx, const char **atts)
{
	int i;

	if (!lmx || !atts || (lmx->lmx_type != LMX_TYPE_TAG))
		return -1;

	for (i = 0; atts[i] != '\0'; i += 2)
		lmx_add_attrib(lmx, atts[i], atts[i+1]);

	return 0;
}

static const struct {
	const char c;
	const char *enc;
} cdatastr__convs[] = {
	{'&',  "&amp;"},
	{'\'', "&apos;"},
	{'\"', "&quot;"},
	{'<',  "&lt;"},
	{'>',  "&gt;"},
	{'\0', ""}
};

static int cdatastr__strlen(const char *instr)
{
	const char *p;
	int n;

	for (n = 0, p = instr; *p; p++) {
		int i;

		for (i = 0; cdatastr__convs[i].c != '\0'; i++) {
			if (cdatastr__convs[i].c == *p) {
				n += strlen(cdatastr__convs[i].enc);
				break;
			}
		}

		if (cdatastr__convs[i].c == '\0')
			n++;
	}

	return n;
}

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

static int cdatastr__decodedstrlen(const char *instr, int instrlen)
{
	const char *p;
	int n;
	int originstrlen = instrlen;

	for (n = 0, p = instr; *p && (instrlen > 0); p++, instrlen--, n++) {
		int i;

		for (i = 0; cdatastr__convs[i].c != '\0'; i++) {
			if (strncmp(cdatastr__convs[i].enc,
						p,
						MIN(strlen(cdatastr__convs[i].enc), instrlen)) == 0) {
				p += strlen(cdatastr__convs[i].enc) - 1;
				instrlen -= strlen(cdatastr__convs[i].enc) - 1;
				break;
			}
		}
	}

	return n;
}

static int cdatastr__decode(char *buf, int buflen, const char *instr, int instrlen)
{
	const char *p;
	int n;

	for (n = 0, p = instr; *p && buflen && (instrlen > 0); ) {
		int i;

		for (i = 0; cdatastr__convs[i].c != '\0'; i++) {
			if (strncmp(cdatastr__convs[i].enc, p, strlen(cdatastr__convs[i].enc)) == 0) {

				*buf = cdatastr__convs[i].c;
				p += strlen(cdatastr__convs[i].enc);
				instrlen -= strlen(cdatastr__convs[i].enc);
				buf++; buflen--; n++;
				break;
			}
		}

		if (cdatastr__convs[i].c == '\0') {
			*buf = *(p++);
			buf++; buflen--; n++;
		}
	}

	*buf = '\0';
	/* *p may != '\0' if buffer too small */

	return n;
}

static int cdatastr__putstr(char *buf, int buflen, const char *instr)
{
	const char *p;
	int n;

	for (n = 0, p = instr; *p && buflen; p++) {
		int i;

		for (i = 0; cdatastr__convs[i].c != '\0'; i++) {
			if (cdatastr__convs[i].c == *p) {
				int z;

				z = strlen(cdatastr__convs[i].enc);
				if (buflen < z)
					z = buflen;
				memcpy(buf, cdatastr__convs[i].enc, z);
				buf += z; buflen -= z; n += z;
				break;
			}
		}

		if (cdatastr__convs[i].c == '\0') {
			*buf = *p;
			buf++; buflen--; n++;
		}
	}

	*buf = '\0';
	/* *p may != '\0' if buffer too small */

	return n;
}

int lmx_add_cdata_bound_encoded(lmx_t *lmx, const char *cdata, int cdatalen) /* exported */
{
	char *buf;
	int buflen;
	int ret;

	buflen = cdatastr__decodedstrlen(cdata, cdatalen);
	if (!(buf = (char *)malloc(buflen+1)))
		return -1;
	cdatastr__decode(buf, buflen, cdata, cdatalen);

	/* stored in decoded form */
	ret = lmx_add_cdata_bound(lmx, buf, buflen);

	free(buf);

	return ret;
}

static int lmxattrib__getrenderedlen(lmx_attrib_t *head)
{
	lmx_attrib_t *lmxa;
	int n;

	for (n = 0, lmxa = head; lmxa; lmxa = lmxa->next) {
		n += 1 + strlen(lmxa->name) + 1 + 1; /* space name=' */
		if (lmxa->value)
			n += strlen(lmxa->value);
		n += 1; /* ' */
	}

	return n;
}

static int lmxattrib__render(lmx_attrib_t *head, char *buf, int buflen)
{
	lmx_attrib_t *lmxa;
	int m;

	for (m = 0, lmxa = head; lmxa; lmxa = lmxa->next) {
		int n;

		n = snprintf(buf, buflen, " %s='%s'",
				lmxa->name,
				lmxa->value ? lmxa->value : "");
		buf += n; buflen -= n;
		m += n;
	}

	return m;
}

static int lmx__getrenderedlen(lmx_t *lmx)
{
	int n = 0;

	if (!lmx)
		return 0;

	if (lmx->lmx_type == LMX_TYPE_TAG) {
		lmx_t *x;

		n += 1 + strlen(lmx->lmx_name) + 1; /* <tag> */
		n += lmxattrib__getrenderedlen(lmx->lmx_attribs);

		/* <tag/> short-hand case */
		if (!lmx->lmx_children) {
			n += 1 /* / */;
			return n;
		}

		for (x = lmx->lmx_children; x; x = x->lmx_next)
			n += lmx__getrenderedlen(x);

		n += 1 + 1 + strlen(lmx->lmx_name) + 1; /* </tag> */

	} else if (lmx->lmx_type == LMX_TYPE_CDATA) {

		n += cdatastr__strlen(lmx->lmx_cdata);

	}

	return n;
}

static int lmx__render(lmx_t *lmx, char *buf, int buflen)
{
	int m = 0;
	int n = 0;

	if (!lmx)
		return 0;

	if (lmx->lmx_type == LMX_TYPE_TAG) {
		lmx_t *x;

		n = snprintf(buf, buflen, "<%s", lmx->lmx_name);
		buf += n; buflen -= n; m += n;

		n = lmxattrib__render(lmx->lmx_attribs, buf, buflen);
		buf += n; buflen -= n; m += n;

		if (!lmx->lmx_children) { /* <tag/> short-hand case */
			n = snprintf(buf, buflen, "/>");
			buf += n; buflen -= n; m += n;

			return m;
		}

		n = snprintf(buf, buflen, ">");
		buf += n; buflen -= n; m += n;

		for (x = lmx->lmx_children; x; x = x->lmx_next) {
			n = lmx__render(x, buf, buflen);
			buf += n; buflen -= n; m += n;
		}

		n = snprintf(buf, buflen, "</%s>", lmx->lmx_name);
		buf += n; buflen -= n; m += n;

	} else if (lmx->lmx_type == LMX_TYPE_CDATA) {

		n = cdatastr__putstr(buf, buflen, lmx->lmx_cdata);
		buf += n; buflen -= n; m += n;

	}

	return m;
}

char *lmx_get_string(lmx_t *lmx) /* exported */
{
	char *buf;
	int buflen;

	if (!lmx)
		return NULL;

	buflen = lmx__getrenderedlen(lmx) + 1;
	if (!(buf = (char *)malloc(buflen)))
		return NULL;

	lmx__render(lmx, buf, buflen);

	return buf;
}

lmx_t *lmx_get_firstchild(lmx_t *lmx)
{

	if (!lmx || (lmx->lmx_type != LMX_TYPE_TAG))
		return NULL;

	return lmx->lmx_children;
}

lmx_t *lmx_get_nextsibling(lmx_t *lmx)
{

	if (!lmx)
		return NULL;

	return lmx->lmx_next;
}

lmx_t *lmx_get_firsttagchild(lmx_t *lmx)
{
	lmx_t *x;

	if (!lmx || (lmx->lmx_type != LMX_TYPE_TAG))
		return NULL;

	for (x = lmx->lmx_children; x; x = x->lmx_next) {
		if (x->lmx_type == LMX_TYPE_TAG)
			return x;
	}

	return NULL;
}

lmx_t *lmx_get_nexttagsibling(lmx_t *lmx)
{
	lmx_t *x;

	if (!lmx)
		return NULL;

	for (x = lmx->lmx_next; x; x = x->lmx_next) {
		if (x->lmx_type == LMX_TYPE_TAG)
			return x;
	}

	return NULL;
}

static void lmx_dump_real(lmx_t *x, int indent)
{

	fprintf(stderr, "%*stype = %s%s%s\n", indent * 8, " ",
			(x->lmx_type == LMX_TYPE_TAG) ? "tag" : "",
			(x->lmx_type == LMX_TYPE_CDATA) ? "cdata" : "",
			((x->lmx_type != LMX_TYPE_TAG) &&
			 (x->lmx_type != LMX_TYPE_CDATA)) ? "unknown" : "");
	indent++;

	if (x->lmx_type == LMX_TYPE_TAG) {
		lmx_t *y;

		fprintf(stderr, "%*sname = '%s'\n", indent * 8, " ",
				x->lmx_name);

		for (y = x->lmx_children; y; y = y->lmx_next)
			lmx_dump_real(y, indent);

	} else if (x->lmx_type == LMX_TYPE_CDATA) {

		fprintf(stderr, "%*scdata (%d) = '%s'\n", indent * 8, " ",
				x->lmx_cdatalen, x->lmx_cdata);

	}

	return;
}

void lmx_dump(lmx_t *lmx)
{

	lmx_dump_real(lmx, 0);

	return;
}

#endif /* ndef NOXML */

