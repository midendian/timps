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

#ifndef __CKCACHE_H__
#define __CKCACHE_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <naf/nafmodule.h>
#include <naf/naftypes.h>

int toscar_ckcache_add(struct nafmodule *mod, naf_u8_t *ck, naf_u16_t cklen, const char *ip, const char *sn, naf_u16_t servtype);
int toscar_ckcache_rem(struct nafmodule *mod, naf_u8_t *ck, naf_u16_t cklen, char **ipret, char **snret, naf_u16_t *servtyperet);
void toscar_ckcache_timer(struct nafmodule *mod, time_t now);


#endif /* ndef __CKCACHE_H__ */

