/*
 * timps - Transparent Instant Messaging Proxy Server
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * timps is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * timps is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __SNAC_H__
#define __SNAC_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#include <naf/nafmodule.h>
#include <naf/naftypes.h>
#include <naf/naftlv.h>
#include <naf/nafbufutils.h>

#define HRET_ERROR -1
#define HRET_FORWARD 0
#define HRET_DIGESTED 1

struct toscar_snac {
	naf_u16_t group;
	naf_u16_t subtype;
	naf_u16_t flags;
	naf_u32_t id;
	naf_sbuf_t extinfo;
	naf_sbuf_t payload;
};

int toscar_flap_handlesnac(struct nafmodule *mod, struct nafconn *conn, naf_u8_t *buf, naf_u16_t buflen);
int toscar_newsnacsb(struct nafmodule *mod, naf_sbuf_t *sb, naf_u16_t group, naf_u16_t subtype, naf_u16_t flags, naf_u32_t id);

int toscar_auth_sendauthinforequest(struct nafmodule *mod, struct nafconn *conn, naf_u32_t snacid, naf_tlv_t *tlvh);


struct touserinfo {
	char *sn;
	naf_u16_t evillevel;
	naf_tlv_t *tlvh;
};
void touserinfo_free(struct nafmodule *mod, struct touserinfo *toui);
struct touserinfo *touserinfo_new(struct nafmodule *mod, const char *sn);
struct touserinfo *touserinfo_extract(struct nafmodule *mod, naf_sbuf_t *sb);
int touserinfo_render(struct nafmodule *mod, struct touserinfo *toui, naf_sbuf_t *sb);

#endif /* __SNAC_H__ */

