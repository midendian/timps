/*
 * naf - Networked Application Framework
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * naf is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * naf is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * Simple SOAP-over-HTTP wrapper for NAF RPC.  Additionally, modules can
 * register methods to be called for filenames on GET requests.
 *
 * XXX not really HTTP compliant, and SOAP compliance is highly questionable
 * XXX needs lots of security auditing...
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifndef HAVE_EXPAT_H
#define NOXML 1
#endif

#ifndef NOXML

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h> /* snprintf -- why is it in stdio.h? */
#endif

#include <naf/nafmodule.h>
#include <naf/nafrpc.h>
#include <naf/nafhttpd.h>

#include <libmx.h> /* Sadly, the entire point is XML. */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h> /* snprintf -- why is it in stdio.h? */
#endif

#include "module.h" /* for naf_module__registerresident() only */

static struct nafmodule *ourmodule = NULL;
static int httpd_debug = 0;

#define SOAPBUFLEN 1024
#define SOAPMONBUFLEN 1

#define SOAPMAXREQPAYLOADLEN 2048

#define SOAPCONN_STATE_FRESH        0
#define SOAPCONN_STATE_INHEADERS    1
#define SOAPCONN_STATE_INREQPAYLOAD 2
#define SOAPCONN_STATE_PROCESSING   3

/* 
 * In the PROCESSING state, we wouldn't generally need to be reading from
 * the client.  However, if the client closes their connection between the
 * time we stop reading the headers (or request payload) and the time we
 * write the reply, the only way to detect the EOF is to have a pending
 * read.
 *
 * So we add a small buffer that we never really use.
 */
static int reqmonitorbuf(struct nafmodule *mod, struct nafconn *conn)
{
	char *nbuf;

	if (!(nbuf = naf_malloc_type(mod, NAF_MEM_TYPE_NETBUF, SOAPMONBUFLEN)))
		return -1;
	if (naf_conn_reqread(conn, nbuf, SOAPMONBUFLEN, 0) == -1) {
		naf_free(mod, nbuf);
		return -1;
	}

	return 0;
}

static int reqsoapbuf(struct nafmodule *mod, struct nafconn *conn, char *inbuf)
{
	char *nbuf = NULL;

	if (!inbuf) {
		if (!(nbuf = naf_malloc_type(mod, NAF_MEM_TYPE_NETBUF, SOAPBUFLEN)))
			return -1;
	}
	if (naf_conn_reqread(conn, inbuf ? inbuf : nbuf, SOAPBUFLEN, 0) == -1) {
		naf_free(mod, nbuf);
		return -1;
	}

	return 0;
}

static void connkill(struct nafmodule *mod, struct nafconn *conn)
{
	return;
}

static int takeconn(struct nafmodule *mod, struct nafconn *conn)
{

	conn->type |= NAF_CONN_TYPE_SOAP | NAF_CONN_TYPE_CLIENT;
	conn->type &= ~NAF_CONN_TYPE_DETECTING;

	conn->state = SOAPCONN_STATE_FRESH;

	if ((naf_conn_setdelim(mod, conn, "\r\n", 2) == -1) ||
			(reqsoapbuf(mod, conn, NULL) == -1)) {
		return -1;
	}

	return 0;
}

static char *nextnonwhite(char *s)
{

	while (s && ((*s == '\t') || (*s == ' ')))
		s++;

	return s;
}

static char *http_getnextarg(struct nafmodule *mod, char **cmd)
{
	char *ret = NULL, *end;

	if (!cmd || !*cmd)
		return NULL;

	if ((end = index(*cmd, ' '))) {
		int len;

		len = end - *cmd;

		/* XXX need checking here to make sure evil doesn't fuck with us */
		if (!(ret = (char *)naf_malloc(mod, len+1)))
			goto out;
		memcpy(ret, *cmd, len);
		ret[len] = '\0';

	} else
		ret = naf_strdup(mod, *cmd);

out:
	if (end)
		end = nextnonwhite(end);
	*cmd = end ? end : NULL;
	return ret;
}

static int handlefirstreqline(struct nafmodule *mod, struct nafconn *conn, char *buf)
{
	char *cur, *req, *file = NULL, *prot = NULL;
	int ret = -1;

	if ((strncmp(buf, "GET ", 4) != 0) &&
			(strncmp(buf, "POST ", 5) != 0) &&
			(httpd_debug > 0)) {
		dvprintf(mod, "unrecognized request: \"%s\"\n", buf);
		return -1;
	}

	cur = buf;
	req = http_getnextarg(mod, &cur);
	file = http_getnextarg(mod, &cur);
	prot = http_getnextarg(mod, &cur);

	if (httpd_debug > 0)
		dvprintf(mod, "request: req = '%s', file = '%s', prot = '%s'\n", req, file, prot);

	if ((strcasecmp(req, "GET") == 0) && file && strlen(file)) {

		if (naf_conn_tag_add(mod, conn, "conn.httprequest.get", 'S', (void *)file) == 0) {
			ret = 0;
			file = NULL;
		}

	} else if ((strcasecmp(req, "POST") == 0) && file && strlen(file)) {

		if (naf_conn_tag_add(mod, conn, "conn.httprequest.post", 'S', (void *)file) == 0) {
			ret = 0;
			file = NULL;
		}

	}

	naf_free(mod, prot);
	naf_free(mod, file);
	naf_free(mod, req);

	return ret;
}

static int handleheaderline(struct nafmodule *mod, struct nafconn *conn, char *buf)
{
	char *name, *value = NULL;

	name = buf;
	if (!(value = index(name, ':')))
		return -1;
	*(value++) = '\0';
	value = nextnonwhite(value);

	if (httpd_debug > 0)
		dvprintf(mod, "header: name = '%s', value = '%s'\n", name, value);

	if (!name || !strlen(name))
		return 0;

	if (strcasecmp(name, "Content-Length") == 0) {
		int clen;

		if (!value || ((clen = atoi(value)) < 0)) {
			dprintf(mod, "invalid Content-Length header\n");
			return -1;
		}

		if (clen > SOAPMAXREQPAYLOADLEN) {
			dvprintf(mod, "rejecting request with huge Content-Length (wanted %d)\n", clen);
			return -1;
		}

		naf_conn_tag_add(mod, conn, "conn.httpheader.content-length", 'I', (void *)clen);

	} /* XXX check other headers? */

	return 0;
}

struct httpdpage {
	struct nafmodule *hp_owner;
	char *hp_fn;
	char *hp_contenttype;
	naf_httpd_pageflags_t hp_flags;
	naf_httpd_pagehandler_t hp_handler;
	struct httpdpage *hp_next;
};
static struct httpdpage *naf_httpd__pages = NULL;

static struct httpdpage *naf_httpd_page__new(struct nafmodule *mod, const char *fn, const char *contenttype, naf_httpd_pageflags_t flags)
{
	struct httpdpage *hp;

	if (!(hp = naf_malloc(mod, sizeof(struct httpdpage))))
		return NULL;
	if (!(hp->hp_fn = naf_strdup(mod, fn))) {
		naf_free(mod, hp);
		return NULL;
	}
	hp->hp_owner = NULL;
	hp->hp_contenttype = NULL; /* defaults to text/html */
	if (contenttype && !(hp->hp_contenttype = naf_strdup(mod, contenttype))) {
		naf_free(mod, hp->hp_fn);
		naf_free(mod, hp);
		return NULL;
	}
	hp->hp_flags = flags;
	hp->hp_handler = NULL;

	return hp;
}

static void naf_httpd_page__free(struct nafmodule *mod, struct httpdpage *hp)
{

	if (hp->hp_contenttype)
		naf_free(mod, hp->hp_contenttype);
	naf_free(mod, hp->hp_fn);
	naf_free(mod, hp);

	return;
}

static struct httpdpage *naf_httpd_page__find_hard(struct nafmodule *mod, const char *fn)
{
	struct httpdpage *hp;

	for (hp = naf_httpd__pages; hp; hp = hp->hp_next) {
		if (strcasecmp(fn, hp->hp_fn) == 0)
			return hp;
	}

	return NULL;
}

int naf_httpd_page_register(struct nafmodule *theirmod, const char *fn, const char *contenttype, naf_httpd_pageflags_t flags, naf_httpd_pagehandler_t handler)
{
	struct httpdpage *hp;

	if (!theirmod || !fn || !handler)
		return -1; /* invalid args */
	if (naf_httpd_page__find_hard(ourmodule, fn))
		return -1; /* already present */
	if (!(hp = naf_httpd_page__new(ourmodule, fn, contenttype, flags)))
		return -1;
	hp->hp_owner = theirmod;
	hp->hp_handler = handler;

	hp->hp_next = naf_httpd__pages;
	naf_httpd__pages = hp;

	return 0;
}

int naf_httpd_page_unregister(struct nafmodule *theirmod, const char *fn)
{
	struct httpdpage *hp, **hpprev;

	for (hpprev = &naf_httpd__pages; (hp = *hpprev); ) {

		if (strcasecmp(hp->hp_fn, fn) == 0) {
			if (hp->hp_owner != theirmod)
				return -1; /* not theirs */
			*hpprev = hp->hp_next;
			naf_httpd_page__free(ourmodule, hp);
			return 0;
		}
		hpprev = &hp->hp_next;
	}

	return -1; /* not registered */
}

int naf_httpd_sendlmx(struct nafmodule *theirmod, struct nafconn *conn, lmx_t *lmx)
{
	char *str, *str2;

	if (!lmx)
		return -1;
	if (!(str = lmx_get_string(lmx)))
		return -1;
	if (!(str2 = naf_strdup_type(ourmodule, NAF_MEM_TYPE_NETBUF, str))) {
		free(str);
		return -1;
	}
	free(str);

	if (naf_conn_reqwrite(conn, str2, strlen(str2)) == -1) {
		naf_free(ourmodule, str2);
		return -1;
	}

	return 0;
}

#define GENERIC404 { \
	"HTTP/1.0 404 Not Found\r\n" \
	"Connection: Close\r\n" \
	"Content-Type: text/html; charset=iso-8859-1\r\n" \
	"\r\n" \
	"<html>" \
	"    <head>" \
	"        <title>404 Not Found</title>" \
	"    </head>" \
	"    <body>" \
	"        <h1>404 Not Found</h1>" \
	"    </body>" \
	"</html>" /* mozilla requires 404's to have bodies! */ \
}

static int send404(struct nafmodule *mod, struct nafconn *conn)
{
	static const char err404[] = GENERIC404;
	char *reply;

	if (!(reply = naf_strdup_type(mod, NAF_MEM_TYPE_NETBUF, err404)))
		return -1;

	if (naf_conn_reqwrite(conn, reply, strlen(reply)) == -1) {
		naf_free(mod, reply);
		return -1;
	}

	naf_conn_schedulekill(conn);

	return 0;
}

static int dorequest_get(struct nafmodule *mod, struct nafconn *conn, const char *file)
{
	struct httpdpage *hp;

	/* XXX if ends with /, fall back to looking for index.html, etc */
	hp = naf_httpd_page__find_hard(mod, file);
	if (!hp) {
		if (httpd_debug > 0)
			dvprintf(mod, "no registered handler for page '%s'\n", file);
		return send404(mod, conn);
	}

	if (!(hp->hp_flags & NAF_HTTPD_PAGEFLAGS_NOHEADERS)) {
		char twoohoh[255];
		char *hdr;

		snprintf(twoohoh, sizeof(twoohoh), 
				"HTTP/1.0 200\r\n"
				"Connection: Close\r\n"
				"Content-Type: %s\r\n"
				"\r\n",
				hp->hp_contenttype ? hp->hp_contenttype : "text/html; charset=iso-8859-1");
		if (!(hdr = naf_strdup_type(mod, NAF_MEM_TYPE_NETBUF, twoohoh)))
			return -1;

		if (naf_conn_reqwrite(conn, hdr, strlen(hdr)) == -1) {
			naf_free(mod, hdr);
			return -1;
		}
	}
	hp->hp_handler(hp->hp_owner, hp->hp_fn, hp->hp_flags, conn);

	if (!(hp->hp_flags & NAF_HTTPD_PAGEFLAGS_NOAUTOCLOSE))
		naf_conn_schedulekill(conn);

	return 0;
}

struct handlenafrpcinfo {
	struct nafmodule *mod;
	XML_Parser parser;
	lmx_t *x;
};

static void handlenafrpc_startelement(void *userdata, const char *name, const char **attribs)
{
	struct handlenafrpcinfo *info = (struct handlenafrpcinfo *)userdata;

	if (info->x) {
		lmx_t *nx;

		nx = lmx_add(info->x, name);
		lmx_add_expatattribs(nx, attribs);
		info->x = nx;

		return;
	}

	if (strcasecmp(name, "SOAP-ENV:Envelope") != 0) {
		if (httpd_debug > 0)
			dvprintf(info->mod, "(start) unknown outer tag '%s'\n", name);
		return;
	}

	info->x = lmx_new(name);
	lmx_add_expatattribs(info->x, attribs);

	return;
}

static naf_rpc_req_t *createreq(struct nafmodule *mod, const char *soapname)
{
	const char *p;
	int modulelen;
	char *module, *meth;
	naf_rpc_req_t *req;

	if (!(p = index(soapname, '.')))
		return NULL;
	modulelen = p - soapname;

	if (!(module = naf_malloc(mod, modulelen + 1)))
		return NULL;
	memcpy(module, soapname, modulelen);
	module[modulelen] = '\0';

	soapname += modulelen + 1;
	if (!(meth = naf_strdup(mod, soapname))) {
		naf_free(mod, module);
		return NULL;
	}

	req = naf_rpc_request_new(mod, module, meth);

	naf_free(mod, module);
	naf_free(mod, meth);

	return req;
}

/*
 * <SOAP-ENV:Envelope
 * 		xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"
 * 		xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
 * 		xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
 * 		xmlns:xsd="http://www.w3.org/1999/XMLSchema"
 * 		SOAP-ENV:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
 * 	<SOAP-ENV:Body>
 * 		<core.getuserinfo>
 * 			<user xsi:type="xsd:int">102009</user>
 * 			<service xsi:type="xsd:string">rim</service>
 * 		</core.getuserinfo>
 * 	</SOAP-ENV:Body>
 * </SOAP-ENV:Envelope>
 */
static void handlenafrpc_endelement(void *userdata, const char *name)
{
	struct handlenafrpcinfo *info = (struct handlenafrpcinfo *)userdata;
	lmx_t *px;

	if ((px = lmx_get_parent(info->x))) {
		info->x = px; /* pop off stack */
		return;
	}

	if (strcasecmp(name, "SOAP-ENV:Envelope") != 0) {
		if (httpd_debug > 0)
			dvprintf(info->mod, "(end) unknown outer tag '%s'\n", name);
	}

	/* info.x is picked up and returned to the caller later */
	return;
}

static void handlenafrpc_chardata(void *userdata, const char *s, int slen)
{
	struct handlenafrpcinfo *info = (struct handlenafrpcinfo *)userdata;

	if (info->x)
		lmx_add_cdata_bound_encoded(info->x, s, slen);

	return;
}

static lmx_t *parsesoaprequest(struct nafmodule *mod, unsigned char *data, int datalen)
{
	struct handlenafrpcinfo info;

	info.mod = mod;
	info.x = NULL;

	info.parser = XML_ParserCreate(NULL);
	XML_SetUserData(info.parser, (void *)&info);
	XML_SetElementHandler(info.parser, handlenafrpc_startelement, handlenafrpc_endelement);
	XML_SetCharacterDataHandler(info.parser, handlenafrpc_chardata);

	XML_Parse(info.parser, data, datalen, 0);

	XML_ParserFree(info.parser);

	return info.x;
}

#define DEFAULTHEADERS \
	"Content-Type: text/xml; charset=\"utf-8\"\r\n" \
	"Connection: close\r\n"

static int sendsoapbody(struct nafmodule *mod, struct nafconn *conn, const char *firstline, lmx_t *soapbody)
{
	char *sb = NULL;
	char *buf;
	int buflen;

	if (soapbody && !(sb = lmx_get_string(soapbody)))
		return -1;

	buflen = strlen(firstline) + 2 + strlen(DEFAULTHEADERS) +
			128 + 4 + (sb ? strlen(sb) : 0);
	if (!(buf = naf_malloc_type(mod, NAF_MEM_TYPE_NETBUF, buflen + 1))) {
		naf_free(mod, sb);
		return -1;
	}

	snprintf(buf, buflen + 1,
			"%s\r\n%sContent-Length: %d\r\n\r\n%s",
			firstline,
			DEFAULTHEADERS,
			sb ? strlen(sb) : 0,
			sb ? sb : "");

	naf_free(mod, sb);

	if (naf_conn_reqwrite(conn, buf, buflen) == -1) {
		naf_free(mod, buf);
		return -1;
	}

	return 0;
}

static int addargs(struct nafmodule *mod, lmx_t *x, naf_rpc_arg_t *head)
{
	naf_rpc_arg_t *arg;
	int count;

	for (arg = head, count = 0; arg; arg = arg->next, count++) {
		lmx_t *y;

		/*
		 * XXX Numerous NAF RPC methods return array names that
		 * can start with numbers.  These numbers then get translated
		 * into tag names here, but having XML tag names start with
		 * numbers is technically illegal and makes many XML parsers
		 * barf.  For example, the one used by SOAP::Lite.  Should
		 * find a way around this.  
		 */
		if (!(y = lmx_add(x, arg->name)))
			continue;

		if (arg->type == NAF_RPC_ARGTYPE_ARRAY) {
			int n;
			char tmpstr[64];

			n = addargs(mod, y, arg->data.children);
			snprintf(tmpstr, sizeof(tmpstr), "xsd:ur-type[%d]", n);
			lmx_add_attrib(y, "SOAP-ENC:arrayType", tmpstr);
			lmx_add_attrib(y, "xsi:type", "xsd:Array");

		} else if (arg->type == NAF_RPC_ARGTYPE_SCALAR) {
			char tmpstr[128];

			snprintf(tmpstr, sizeof(tmpstr), "%lu", arg->data.scalar);
			lmx_add_cdata(y, tmpstr);
			lmx_add_attrib(y, "xsi:type", "xsd:int");

		} else if (arg->type == NAF_RPC_ARGTYPE_BOOL) {

			lmx_add_cdata(y, arg->data.boolean ? "1" : "0");

			lmx_add_attrib(y, "xsi:type", "xsd:boolean");

		} else if (arg->type == NAF_RPC_ARGTYPE_STRING) {

			lmx_add_cdata(y, arg->data.string);
			lmx_add_attrib(y, "xsi:type", "xsd:string");

		} /* XXX NAF_RPC_ARGTYPE_GENERIC */
	}

	return count;
}

static int sendsoapreturnargs(struct nafmodule *mod, struct nafconn *conn, naf_rpc_arg_t *returnhead)
{
	lmx_t *env, *body, *root;
	int ret = -1;

	if (!(env = lmx_new("SOAP-ENV:Envelope")))
		goto out;
	lmx_add_attrib(env, "xmlns:xsi", "http://www.w3.org/1999/XMLSchema-instance");
	lmx_add_attrib(env, "xmlns:SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/");
	lmx_add_attrib(env, "xmlns:SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/");
	lmx_add_attrib(env, "xmlns:xsd", "http://www.w3.org/1999/XMLSchema");
	lmx_add_attrib(env, "SOAP-ENV:encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/");

	if (!(body = lmx_add(env, "SOAP-ENV:Body")))
		goto out;
	if (!(root = lmx_add(body, "NAFRPCReturnValues")))
		goto out;
	addargs(mod, root, returnhead);

	if (sendsoapbody(mod, conn, "HTTP/1.1 200 OK", env) == 0)
		ret = 0;

out:
	lmx_free(env);
	return ret;
}

static int sendsoaperror(struct nafmodule *mod, struct nafconn *conn, int rpcstatus)
{
	lmx_t *env, *body, *fault, *x;
	int ret = -1;

	if (!(env = lmx_new("SOAP-ENV:Envelope")))
		goto out;
	lmx_add_attrib(env, "xmlns:SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/");

	if (!(body = lmx_add(env, "SOAP-ENV:Body")))
		goto out;
	if (!(fault = lmx_add(body, "SOAP-ENV:Fault")))
		goto out;
	if ((x = lmx_add(fault, "faultcode")))
		lmx_add_cdata(x, "SOAP-ENV:Server");
	if ((x = lmx_add(fault, "faultstring"))) {

		if (rpcstatus == NAF_RPC_STATUS_INVALIDARGS)
			lmx_add_cdata(x, "Invalid arguments");
		else if (rpcstatus == NAF_RPC_STATUS_UNKNOWNFAILURE)
			lmx_add_cdata(x, "Unknown failure");
		else if (rpcstatus == NAF_RPC_STATUS_UNKNOWNTARGET)
			lmx_add_cdata(x, "Unknown target");
		else if (rpcstatus == NAF_RPC_STATUS_UNKNOWNMETHOD)
			lmx_add_cdata(x, "Unknown method");
		else if (rpcstatus == NAF_RPC_STATUS_PENDING)
			lmx_add_cdata(x, "Unable to process request immediately");
		else
			lmx_add_cdata(x, "Unrecognized status");
	}

	if (sendsoapbody(mod, conn, "HTTP/1.1 500 Internal Server Error", env) == 0)
		ret = 0;

out:
	lmx_free(env);
	return ret;
}

static int handlenafrpc(struct nafmodule *mod, struct nafconn *conn, unsigned char *data, int datalen)
{
	lmx_t *xreq;
	lmx_t *envbody, *func, *parm;
	naf_rpc_req_t *req = NULL;


	xreq = parsesoaprequest(mod, data, datalen);

	if (!(envbody = lmx_get_tag(xreq, "SOAP-ENV:Body")))
		goto errout;
	if (!(func = lmx_get_firsttagchild(envbody)))
		goto errout;

	if (!(req = createreq(mod, lmx_get_name(func))))
		goto errout;
	for (parm = lmx_get_firsttagchild(func);
			parm;
			parm = lmx_get_nexttagsibling(parm)) {
		const char *parmname, *parmtype;

		if (!(parmname = lmx_get_name(parm)))
			continue;
		if (!(parmtype = lmx_get_attrib(parm, "xsi:type")))
			continue;

		if (strcmp(parmtype, "xsd:string") == 0) {
			const char *data;

			if (!(data = lmx_get_cdata(parm)))
				continue;
			naf_rpc_addarg_string(mod, &req->inargs, parmname, data);

		} else if (strcmp(parmtype, "xsd:int") == 0) {
			const char *data;

			if (!(data = lmx_get_cdata(parm)))
				continue;
			naf_rpc_addarg_scalar(mod, &req->inargs, parmname, (naf_rpcu32_t)atoi(data));

		} else if (strcmp(parmtype, "xsd:boolean") == 0) {
			const char *data;

			if (!(data = lmx_get_cdata(parm)))
				continue;
			naf_rpc_addarg_bool(mod, &req->inargs, parmname, (naf_rpcu8_t)(atoi(data) == 0));

		} else {

			dvprintf(mod, "unknown datatype '%s'\n", parmtype);
		}

	}

	if (naf_rpc_request_issue(mod, req) == -1)
		req->status = NAF_RPC_STATUS_UNKNOWNFAILURE; /* good enough. */

	if (req->status == NAF_RPC_STATUS_SUCCESS) {
		if (sendsoapreturnargs(mod, conn, req->returnargs) == -1)
			goto errout;
	} else {
		if (sendsoaperror(mod, conn, req->status) == -1)
			goto errout;
	}

	naf_conn_schedulekill(conn);

	lmx_free(xreq);
	naf_rpc_request_free(mod, req);
	return 0;

errout:
	/* XXX return error */
	lmx_free(xreq);
	if (req)
		naf_rpc_request_free(mod, req);
	return -1;
}

static int dorequest_post(struct nafmodule *mod, struct nafconn *conn, const char *file, unsigned char *reqpayload, int reqpayloadlen)
{

	if (strcasecmp(file, "/nafrpc") == 0)
		return handlenafrpc(mod, conn, reqpayload, reqpayloadlen);

	return send404(mod, conn);
}

static int dorequest(struct nafmodule *mod, struct nafconn *conn, unsigned char *reqpayload, int reqpayloadlen)
{
	char *file = NULL;


	naf_conn_tag_fetch(mod, conn, "conn.httprequest.get", NULL, (void **)&file);
	if (file)
		return dorequest_get(mod, conn, file);


	naf_conn_tag_fetch(mod, conn, "conn.httprequest.post", NULL, (void **)&file);
	if (file)
		return dorequest_post(mod, conn, file, reqpayload, reqpayloadlen);

	return -1;
}

static int connready(struct nafmodule *mod, struct nafconn *conn, naf_u16_t what)
{

	if (what & NAF_CONN_READY_DETECTTO)
		return takeconn(mod, conn);

	if (what & NAF_CONN_READY_WRITE) {
		unsigned char *buf;
		int buflen;

		if (naf_conn_takewrite(conn, &buf, &buflen) < 0) {
			dprintf(mod, "connready: takewrite failed\n");
			return -1;
		}
		naf_free(mod, buf);

		return 0;
	}

	if (what & NAF_CONN_READY_READ) {
		unsigned char *buf;
		int buflen;

		if (naf_conn_takeread(conn, &buf, &buflen) == -1) {
			dprintf(mod, "connready: takeread failed\n");
			return -1;
		}

		if (conn->state == SOAPCONN_STATE_FRESH) {

			if (handlefirstreqline(mod, conn, buf) == -1) {
				naf_free(mod, buf);
				/* XXX probably should issue HTTP error */
				return -1;
			}

			conn->state = SOAPCONN_STATE_INHEADERS;

			if (reqsoapbuf(mod, conn, buf) == -1) {
				naf_free(mod, buf);
				return -1;
			}

			return 0;

		} else if (conn->state == SOAPCONN_STATE_INHEADERS) {

			if (strlen(buf) == 0) { /* headers end with blank line */
				int clen = -1;

				naf_conn_tag_fetch(mod, conn, "conn.httpheader.content-length", NULL, (void **)&clen);

				if (clen > 0) {
					if (clen > SOAPBUFLEN) {
						naf_free(mod, buf);
						if (!(buf = naf_malloc_type(mod, NAF_MEM_TYPE_NETBUF, clen))) {
							return -1; /* XXX HTTP error */
						}
					}
					if (naf_conn_reqread(conn, buf, clen, 0) == -1) {
						naf_free(mod, buf);
						return -1; /* XXX HTTP error */
					}

					conn->state = SOAPCONN_STATE_INREQPAYLOAD;

				} else { /* no body */

					naf_conn_setdelim(mod, conn, NULL, 0);
					naf_free(mod, buf);

					conn->state = SOAPCONN_STATE_PROCESSING;
					if (reqmonitorbuf(mod, conn) == -1)
						return -1;

					return dorequest(mod, conn, NULL, 0);
				}

			} else { /* header line */

				if (handleheaderline(mod, conn, buf) == -1) {
					naf_free(mod, buf);
					return -1; /* XXX HTTP error */
				}

				if (reqsoapbuf(mod, conn, buf) == -1) {
					naf_free(mod, buf);
					return -1;
				}
			}

			return 0;

		} else if (conn->state == SOAPCONN_STATE_INREQPAYLOAD) {
			int ret;

			if (httpd_debug > 0)
				dvprintf(mod, "received %d bytes of HTTP payload\n", buflen);
			conn->state = SOAPCONN_STATE_PROCESSING;
			if (reqmonitorbuf(mod, conn) == -1)
				return -1;

			ret = dorequest(mod, conn, buf, buflen);

			naf_free(mod, buf);

			return ret;

		} else if (conn->state == SOAPCONN_STATE_PROCESSING) {

			dprintf(mod, "received data from client while in processing state\n");
			return -1;

		} else {

			dvprintf(mod, "connection in unknown state %d\n", conn->state);

			return -1;
		}
	}

	return 0;
}

static void freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{

	if (strcmp(tagname, "conn.httpheader.content-length") == 0) {

		/* integer */

	} else if (strcmp(tagname, "conn.httprequest.get") == 0) {
		char *fn = (char *)tagdata;

		naf_free(mod, fn);

	} else if (strcmp(tagname, "conn.httprequest.post") == 0) {
		char *fn = (char *)tagdata;

		naf_free(mod, fn);

	} else {

		dvprintf(mod, "XXX unknown tag '%s'\n", tagname);

	}

	return;
}

static int modfirst(struct nafmodule *mod)
{

	ourmodule = mod;

	naf_module_setname(mod, "httpd");
	mod->takeconn = takeconn;
	mod->connready = connready;
	mod->connkill = connkill;
	mod->freetag = freetag;

	return 0;
}

#endif /* ndef NOXML */

int naf_httpd__register(void)
{
#ifdef NOXML
	return 0;
#else
	return naf_module__registerresident("httpd", modfirst, NAF_MODULE_PRI_THIRDPASS);
#endif
}

