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

#ifndef __NET_H__
#define __NET_H__

#include <naf/nafmodule.h>

#ifndef __PLUGINREGONLY

#include <naf/nafbufutils.h>

struct nafnet_ifaddr {
	naf_u16_t ifa_type;
#define NAFNET_AF_INET 0
	union {
		struct {
			naf_u32_t ifaa_ipv4_addr;
			naf_u32_t ifaa_ipv4_mask;
		} ifa_addr_ipv4;
	} ifa_addr;
#define ifa_ipv4 ifa_addr.ifa_addr_ipv4
#define ifa_ipv4_addr ifa_ipv4.ifaa_ipv4_addr
#define ifa_ipv4_mask ifa_ipv4.ifaa_ipv4_mask
	struct nafnet_ifaddr *ifa__next;
};

struct nafnet_if;
typedef int (*naf_net_if_outputfunc_t)(struct nafmodule *caller, struct nafnet_if *iff, naf_sbuf_t *sb);

struct nafnet_if {
#define NAFNET_MAXIFNAMELEN 16
	char if_name[NAFNET_MAXIFNAMELEN];
	naf_u32_t if_flags;
#define NAFNET_IFFLAG_NONE 0x00000000
#define NAFNET_IFFLAG_UP   0x00000001
	naf_u16_t if_mtu;
#define NAFNET_MTU_DEFAULT 1500
	struct nafnet_ifaddr *if_addrs;
	struct nafmodule *if_owner;
	naf_net_if_outputfunc_t if_outputf;
	struct nafnet_if *if__next;
};

struct nafnet_if *naf_net_if_add(struct nafmodule *mod, const char *name, naf_net_if_outputfunc_t outf);
int naf_net_if_rem(struct nafmodule *mod, struct nafnet_if *iff);
struct nafnet_if *naf_net_if_find(const char *ifname);

#endif /* ndef __PLUGINREGONLY */

int naf_net__register(void);

#endif /* __NET_H__ */

