
#ifndef __LIBMX_H__
#define __LIBMX_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifndef NOXML
#include <expat.h>
#endif

#ifndef NOXML

typedef struct lmx_attrib_s {
	char *name;
	char *value;
	struct lmx_attrib_s *next;
} lmx_attrib_t;

typedef struct lmx_s {
#define LMX_TYPE_TAG   0
#define LMX_TYPE_CDATA 1
	int lmx_type;
	union {
		struct {
			char *name;
			lmx_attrib_t *attribs;
			struct lmx_s *children;
			struct lmx_s *children_tail;
		} tagdata;
#define lmx_name data.tagdata.name
#define lmx_attribs data.tagdata.attribs
#define lmx_children data.tagdata.children
#define lmx_children_tail data.tagdata.children_tail
		struct {
			char *cdata;
			int cdatalen; /* does not include NULL terminator */
		} cdatadata;
#define lmx_cdata data.cdatadata.cdata
#define lmx_cdatalen data.cdatadata.cdatalen
	} data;
	/* top-level nodes never have a parent or a next */
	struct lmx_s *lmx_parent;
	struct lmx_s *lmx_next;
} lmx_t;

lmx_t *lmx_new(const char *name);
lmx_t *lmx_free(lmx_t *lmx); /* returns parent (if any) */

const char *lmx_get_name(lmx_t *lmx);
lmx_t *lmx_add(lmx_t *parent, const char *name);
lmx_t *lmx_add_lmx(lmx_t *dest, lmx_t *src);
lmx_t *lmx_get_tag(lmx_t *parent, const char *tagname);
lmx_t *lmx_get_parent(lmx_t *lmx);
int lmx_add_cdata(lmx_t *lmx, const char *cdata);
int lmx_add_cdata_bound(lmx_t *lmx, const char *cdata, int cdatalen);
int lmx_add_cdata_bound_encoded(lmx_t *lmx, const char *cdata, int cdatalen);
const char *lmx_get_cdata(lmx_t *lmx);
const char *lmx_get_cdata_oftag(lmx_t *lmx, const char *tagname);
int lmx_add_attrib(lmx_t *lmx, const char *name, const char *value);
const char *lmx_get_attrib(lmx_t *lmx, const char *name);
int lmx_add_expatattribs(lmx_t *lmx, const char **attribs);
char *lmx_get_string(lmx_t *lmx);
lmx_t *lmx_get_firstchild(lmx_t *lmx); /* may return LMX_TYPE_CDATA */
lmx_t *lmx_get_nextsibling(lmx_t *lmx); /* may return LMX_TYPE_CDATA */
lmx_t *lmx_get_firsttagchild(lmx_t *lmx);
lmx_t *lmx_get_nexttagsibling(lmx_t *lmx);

#endif /* ndef NOXML */

#endif /* __LIBMX_H__ */

