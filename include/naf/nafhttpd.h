
#ifndef __NAFHTTPD_H__
#define __NAFHTTPD_H__

#include <naf/naftypes.h>
#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <libmx.h>

#define NAF_HTTPD_PAGEFLAGS_NONE        0x0000
#define NAF_HTTPD_PAGEFLAGS_NOHEADERS   0x0001 /* handler will send headers */
#define NAF_HTTPD_PAGEFLAGS_NOAUTOCLOSE 0x0002 /* handler will maintain conn */
typedef naf_u16_t naf_httpd_pageflags_t;

typedef int (*naf_httpd_pagehandler_t)(struct nafmodule *theirmod, const char *fn, naf_httpd_pageflags_t pageflags, struct nafconn *httpconn);

int naf_httpd_page_register(struct nafmodule *theirmod, const char *fn, const char *contenttype, naf_httpd_pageflags_t flags, naf_httpd_pagehandler_t handler);
int naf_httpd_page_unregister(struct nafmodule *theirmod, const char *fn);
int naf_httpd_sendlmx(struct nafmodule *theirmod, struct nafconn *conn, lmx_t *lmx);

#endif /* __NAFHTTPD_H__ */

