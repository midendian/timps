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

