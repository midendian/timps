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

#ifndef __FLAP_H__
#define __FLAP_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <naf/naftypes.h>
#include <naf/nafbufutils.h>

int toscar_flap_prepareconn(struct nafmodule *mod, struct nafconn *conn);
int toscar_flap_handleread(struct nafmodule *mod, struct nafconn *conn);
int toscar_flap_handlewrite(struct nafmodule *mod, struct nafconn *conn);

struct nafconn *toscar__findconn(struct nafmodule *mod, const char *sn, naf_u32_t conntypemask);

int toscar_flap_sendconnclose(struct nafmodule *mod, struct nafconn *conn, naf_u16_t reason, const char *reasonurl);
int toscar_flap_puthdr(naf_sbuf_t *sb, naf_u8_t chan);
int toscar_flap_sendsbuf_consume(struct nafmodule *mod, struct nafconn *conn, naf_sbuf_t *sb);

#endif /* ndef __FLAP_H__ */

