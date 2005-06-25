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

#ifndef __IPV4_H__
#define __IPV4_H__

#include <naf/nafmodule.h>

#ifndef __PLUGINREGONLY

#include <naf/nafbufutils.h>
#include "net.h"

struct nafnet_rt {
	naf_u32_t rt_dest;
	naf_u32_t rt_gw;
	naf_u32_t rt_mask;
	naf_u16_t rt_flags;
	naf_u16_t rt_metric;
	struct nafnet_if *rt_if;
	struct nafnet_rt *rt__next;
};

struct nafnet_ip {
	naf_u32_t ip_flags;
#define NAF_NET_IPFLAG_NONE 0x00000000
#define NAF_NET_IPFLAG_MOREFRAG 0x00000001
#define NAF_NET_IPFLAG_DONTFRAG 0x00000002
	naf_u16_t ip_fields;
	/* set when corresponding field is set to valid value */
#define NAF_NET_IPFIELD_NONE 0x0000
#define NAF_NET_IPFIELD_TOS 0x0001
#define NAF_NET_IPFIELD_ID  0x0002
#define NAF_NET_IPFIELD_FRAGOFF 0x0004
#define NAF_NET_IPFIELD_TTL 0x0008
#define NAF_NET_IPFIELD_PROTOCOL 0x0010
#define NAF_NET_IPFIELD_SADDR 0x0020
#define NAF_NET_IPFIELD_DADDR 0x0040
	naf_u16_t ip_options;
	/* set when corresponding option is/was set */
#define NAF_NET_IPOPT_NONE 0x0000
#define NAF_NET_IPOPT_SECURITY 0x0001
#define NAF_NET_IPOPT_LSRR 0x0002
#define NAF_NET_IPOPT_SSRR 0x0004
#define NAF_NET_IPOPT_RR 0x0008
#define NAF_NET_IPOPT_SID 0x0010
#define NAF_NET_IPOPT_TIMESTAMP 0x0020
	naf_u8_t ip_tos;
	naf_u16_t ip_id;
	naf_u16_t ip_fragoff;
	naf_u8_t ip_ttl;
	naf_u8_t ip_protocol;
	naf_u32_t ip_saddr;
	naf_u32_t ip_daddr;
	naf_sbuf_t ip_data;
	struct nafnet_rt *ip_rt;
};

/* From BSD. */
struct nafnet_ipraw {
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int	ip_hl:4,		/* header length */
		ip_v:4;			/* version */
#endif
#if BYTE_ORDER == BIG_ENDIAN
	u_int	ip_v:4,			/* version */
		ip_hl:4;		/* header length */
#endif
	u_char	ip_tos;			/* type of service */
	u_short	ip_len;			/* total length */
	u_short	ip_id;			/* identification */
	u_short	ip_off;			/* fragment offset field */
#define	IP_RF 0x8000			/* reserved fragment flag */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
	u_char	ip_ttl;			/* time to live */
	u_char	ip_p;			/* protocol */
	u_short	ip_sum;			/* checksum */
	struct	in_addr ip_src,ip_dst;	/* source and dest address */
};

struct nafnet_ip *naf_ipv4_ip_new(struct nafmodule *mod);
void naf_ipv4_ip_free(struct nafmodule *mod, struct nafnet_ip *ip);

/* called by interface driver when IP packet received */
int naf_ipv4_input(struct nafmodule *mod, struct nafnet_if *recvif, naf_u8_t *buf, naf_u16_t buflen);
typedef int (*naf_ipv4_protfunc_t)(struct nafmodule *yourmod, struct nafnet_ip *ip);

int naf_ipv4_prot_register(struct nafmodule *mod, naf_u8_t prot, naf_ipv4_protfunc_t func);
int naf_ipv4_prot_unregister(struct nafmodule *mod, naf_u8_t prot, naf_ipv4_protfunc_t func);

/* called by IP users when they want to send a packet */
int naf_ipv4_output(struct nafmodule *mod, struct nafnet_ip *ip);

/* called by net.c */
void naf_ipv4_event_ifup(struct nafnet_if *iff);
void naf_ipv4_event_ifdown(struct nafnet_if *iff);

#endif /* ndef __PLUGINREGONLY */

int naf_ipv4__register(void);

#endif /* __IPV4_H__ */

