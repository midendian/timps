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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif


#include <naf/nafmodule.h>

#ifdef NAF_USEIPV4

#include <naf/nafconfig.h>
#include <naf/nafbufutils.h>
#include <naf/nafrpc.h>

#include "../module.h" /* for naf_module__registerresident() */

#include "net.h"
#include "ipv4.h"

#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <string.h>

#define NAF_IPV4_DEBUG_DEFAULT 0
static int naf_ipv4__debug = NAF_IPV4_DEBUG_DEFAULT;
static struct nafmodule *naf_ipv4__module = NULL;

static struct nafnet_rt *naf_ipv4__routetable = NULL;

struct naf_ipv4_prot {
	naf_u8_t nip_protocol;
	struct nafmodule *nip_mod;
	naf_ipv4_protfunc_t nip_handler;
	struct naf_ipv4_prot *nip__next;
};
static struct naf_ipv4_prot *naf_ipv4__protlist = NULL;


static void
naf_ipv4_route__free(struct nafnet_rt *rt)
{
	naf_free(naf_ipv4__module, rt);
	return;
}

static struct nafnet_rt *
naf_ipv4_route__alloc(void)
{
	struct nafnet_rt *rt;

	if (!(rt = naf_malloc(naf_ipv4__module, sizeof(struct nafnet_rt))))
		return NULL;
	memset(rt, 0, sizeof(struct nafnet_rt));

	return rt;
}

static struct nafnet_rt *
naf_ipv4_route_find(naf_u32_t dest, naf_u32_t gw, naf_u32_t mask, naf_u16_t metric)
{
	struct nafnet_rt *rt;

	for (rt = naf_ipv4__routetable; rt; rt = rt->rt__next) {
		if ((rt->rt_dest == dest) &&
		    (rt->rt_gw == gw) &&
		    (rt->rt_mask == mask) &&
		    (rt->rt_metric == metric))
			return rt;
	}
	return NULL;
}

struct nafnet_rt *
naf_ipv4_getroute(naf_u32_t addr)
{
	/* XXX XXX XXX do this right, heh. */
	return naf_ipv4__routetable;
}

static int
naf_ipv4_route_rem(naf_u32_t dest, naf_u32_t gw, naf_u32_t mask, naf_u16_t metric)
{
	struct nafnet_rt *rtc, **rtp;

	for (rtp = &naf_ipv4__routetable; (rtc = *rtp); ) {
		if ((rtc->rt_dest == dest) &&
		    (rtc->rt_gw == gw) &&
		    (rtc->rt_mask == mask) &&
		    (rtc->rt_metric == metric)) {
			*rtp = rtc->rt__next;
			naf_ipv4_route__free(rtc);
			return 0;
		}
		rtp = &rtc->rt__next;
	}
	return -1;
}

static int
naf_ipv4_route_add(naf_u32_t dest, naf_u32_t gw, naf_u32_t mask, naf_u16_t flags, naf_u16_t metric, struct nafnet_if *iff)
{
	struct nafnet_rt *rt;

	rt = naf_ipv4_route_find(dest, gw, mask, metric);
	if (rt) {
		if (naf_ipv4__debug)
			dvprintf(naf_ipv4__module, "route already exists (%08lx/%08lx via %08lx metric %d\n",dest, mask, gw, metric);
		return -1;
	}
	rt = naf_ipv4_route__alloc();
	if (!rt)
		return -1;
	rt->rt_dest = dest;
	rt->rt_gw = gw;
	rt->rt_mask = mask;
	rt->rt_flags = flags;
	rt->rt_metric = metric;
	if (iff)
		rt->rt_if = iff;
	else {
		/* XXX choose an interface properly */
		rt->rt_if = naf_net_if_find(NULL);
	}

	/* XXX should keep this ordered */
	rt->rt__next = naf_ipv4__routetable;
	naf_ipv4__routetable = rt;

	return 0;
}

void naf_ipv4_event_ifup(struct nafnet_if *iff)
{
	struct nafnet_ifaddr *ifa;

	/* Add a network route. */
	for (ifa = iff->if_addrs;
	     ifa;
	     ifa = ifa->ifa__next) {

		if (ifa->ifa_type != NAFNET_AF_INET)
			continue;

		/*
		 * XXX This only works for non-point-to-point links.  For ptp
		 * links, the route should get set to the other end, not the
		 * network.  (Proper mask for a ptp link is all-ones, but
		 * currently if you try to do that, you'll end up with a static
		 * route yourself, which is useless.)
		 */
		naf_ipv4_route_add(ifa->ifa_ipv4.ifaa_ipv4_addr &
				      ifa->ifa_ipv4.ifaa_ipv4_mask,
				   0x00000000 /* no gateway */,
				   ifa->ifa_ipv4.ifaa_ipv4_mask,
				   0, 0, iff);
	}
	return;
}

void
naf_ipv4_event_ifdown(struct nafnet_if *iff)
{
	struct nafnet_ifaddr *ifa;

	/* Add a network route. */
	for (ifa = iff->if_addrs;
	     ifa;
	     ifa = ifa->ifa__next) {

		if (ifa->ifa_type != NAFNET_AF_INET)
			continue;

		naf_ipv4_route_rem(ifa->ifa_ipv4.ifaa_ipv4_addr &
				      ifa->ifa_ipv4.ifaa_ipv4_mask,
				   0x00000000 /* no gateway */,
				   ifa->ifa_ipv4.ifaa_ipv4_mask,
				   0);
	}
	return;
}

static void
naf_ipv4_route__flush(void)
{
	struct nafnet_rt *rt;

	for (rt = naf_ipv4__routetable; rt; ) {
		struct nafnet_rt *rtt;

		rtt = rt->rt__next;
		naf_ipv4_route__free(rt);
		rt = rtt;
	}
	return;
}


static void
naf_ipv4_prot__free(struct naf_ipv4_prot *nip)
{
	naf_free(naf_ipv4__module, nip);
	return;
}

static struct naf_ipv4_prot *
naf_ipv4_prot__alloc(void)
{
	struct naf_ipv4_prot *nip;

	if (!(nip = naf_malloc(naf_ipv4__module, sizeof(struct naf_ipv4_prot))))
		return NULL;
	memset(nip, 0, sizeof(struct naf_ipv4_prot));

	return nip;
}

int
naf_ipv4_prot_register(struct nafmodule *mod, naf_u8_t prot, naf_ipv4_protfunc_t func)
{
	struct naf_ipv4_prot *nip;

	if (!mod || !func)
		return -1;

	if (!(nip = naf_ipv4_prot__alloc()))
		return -1;
	nip->nip_protocol = prot;
	nip->nip_mod = mod;
	nip->nip_handler = func;

	nip->nip__next = naf_ipv4__protlist;
	naf_ipv4__protlist = nip;

	return 0;
}

int
naf_ipv4_prot_unregister(struct nafmodule *mod, naf_u8_t prot, naf_ipv4_protfunc_t func)
{
	struct naf_ipv4_prot *nipc, **nipp;

	for (nipp = &naf_ipv4__protlist; (nipc = *nipp); ) {
		if ((nipc->nip_mod == mod) &&
		    (nipc->nip_protocol == prot) &&
		    (nipc->nip_handler == func)) {
			*nipp = nipc->nip__next;
			naf_ipv4_prot__free(nipc);
			return 0;
		}
		nipp = &nipc->nip__next;
	}

	return -1;
}

static void
naf_ipv4_prot__unregisterall(void)
{
	struct naf_ipv4_prot *nip;

	for (nip = naf_ipv4__protlist; nip; ) {
		struct naf_ipv4_prot *nipt;

		nipt = nip->nip__next;
		naf_ipv4_prot__free(nip);
		nip = nipt;
	}
	return;
}


#define MINIPHDRLEN 20

static int
getiphdr(naf_sbuf_t *sb, struct nafnet_ipraw *iph)
{
	naf_u8_t b;

	if (naf_sbuf_bytesremaining(sb) < MINIPHDRLEN)
		return -1;

	b = naf_sbuf_get8(sb);
	iph->ip_v = (b >> 4) & 0xf;
	iph->ip_hl = b & 0xf;
	iph->ip_tos = naf_sbuf_get8(sb);
	iph->ip_len = naf_sbuf_get16(sb);
	iph->ip_id = naf_sbuf_get16(sb);
	iph->ip_off = naf_sbuf_get16(sb);
	iph->ip_ttl = naf_sbuf_get8(sb);
	iph->ip_p = naf_sbuf_get8(sb);
	iph->ip_sum = naf_sbuf_get16(sb);
		naf_sbuf_setpos(sb, naf_sbuf_getpos(sb) - 2);
		naf_sbuf_put16(sb, 0x0000); /* for calculating csum later */
	iph->ip_src.s_addr = naf_sbuf_get32(sb);
	iph->ip_dst.s_addr = naf_sbuf_get32(sb);

	return 0;
}

/*
 * Returns ones-complement of the ones-complement word-wise sum of the IP
 * header (which is assumed to have the csum field set to zero).  Value is in
 * network byte order.  (I guess uses 32bit math for performance, heh.)
 */
static naf_u16_t
naf_ipv4__hdrcsum(naf_u8_t *buf, naf_u16_t len)
{
	naf_u32_t sum;
	naf_u16_t *p;

	for (sum = 0, p = (naf_u16_t *)buf; len > 1; len -= 2, p++)
		sum += *p;
	if (len) {
		union {
			naf_u8_t byte;
			naf_u16_t wyde;
		} odd;
		odd.wyde = 0;
		odd.byte = *(naf_u8_t *)p;
		sum += odd.wyde;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum = htons(sum & 0xffff);

	return ~sum;
}

/*
 * sb should be positioned at start of options and only extend to the end of
 * the IP header (ie, the end of the options).
 */
static int
naf_ipv4__parseoptions(naf_sbuf_t *sb, struct nafnet_ip *ip)
{

	while (naf_sbuf_bytesremaining(sb)) {
		naf_u8_t type;

		type = naf_sbuf_get8(sb);
		if (type == 0) /* EOO */
			break;
		else if (type == 1) /* No Op */
			;
		else if (type == 130) { /* security */

			ip->ip_options |= NAF_NET_IPOPT_SECURITY;
			naf_sbuf_advance(sb, 11); /* fixed length */

		} else if (type == 131) { /* Loose Source and Record Route */
			naf_u8_t len;

			ip->ip_options |= NAF_NET_IPOPT_LSRR;
			len = naf_sbuf_get8(sb);
			naf_sbuf_advance(sb, len - 2); /* XXX */

		} else if (type == 137) { /* Strict Source and Record Route */
			naf_u8_t len;

			ip->ip_options |= NAF_NET_IPOPT_SSRR;
			len = naf_sbuf_get8(sb);
			naf_sbuf_advance(sb, len - 2); /* XXX */

		} else if (type == 7) { /* Record Route */
			naf_u8_t len;

			ip->ip_options |= NAF_NET_IPOPT_RR;
			len = naf_sbuf_get8(sb);
			naf_sbuf_advance(sb, len - 2); /* XXX */

		} else if (type == 136) { /* Stream ID */

			ip->ip_options |= NAF_NET_IPOPT_SID;
			naf_sbuf_advance(sb, 4);

		} else if (type == 68) { /* Timestamp */
			naf_u8_t len;

			ip->ip_options |= NAF_NET_IPOPT_TIMESTAMP;
			len = naf_sbuf_get8(sb);
			naf_sbuf_advance(sb, len - 2); /* XXX */

		} else {
			if (naf_ipv4__debug)
				dvprintf(naf_ipv4__module, "unknown IP option 0x%02x from 0x%08lx\n", type, ip->ip_saddr);
			break; /* can't continue */
		}

	}

	return 0;
}

static int
naf_ipv4_input__callhandler(struct nafnet_ip *ip)
{
	struct naf_ipv4_prot *nip;

	for (nip = naf_ipv4__protlist; nip; nip = nip->nip__next) {
		if (nip->nip_protocol == ip->ip_protocol) {
			if (nip->nip_handler(nip->nip_mod, ip) == 0)
				return 0;
			naf_sbuf_rewind(&ip->ip_data);
		}
	}
	return -1;
}

struct nafnet_ip *
naf_ipv4_ip_new(struct nafmodule *mod)
{
	struct nafnet_ip *ip;

	if (!(ip = naf_malloc(mod, sizeof(struct nafnet_ip))))
		return NULL;
	memset(ip, 0, sizeof(struct nafnet_ip));
	naf_sbuf_init(mod, &ip->ip_data, NULL, 0); /* autogrow */

	return ip;
}

void
naf_ipv4_ip_free(struct nafmodule *mod, struct nafnet_ip *ip)
{

	if (!mod || !ip)
		return;

	naf_sbuf_free(mod, &ip->ip_data);
	naf_free(mod, ip);

	return;
}

int
naf_ipv4_input(struct nafmodule *inmod, struct nafnet_if *recvif, naf_u8_t *buf, naf_u16_t buflen)
{
	struct nafnet_ipraw iph;
	naf_sbuf_t sb;
	struct nafnet_ip ip;
	naf_u16_t hdrlen;

	if (!inmod || !buf)
		return -1;

	naf_sbuf_init(naf_ipv4__module, &sb, buf, buflen);
	if (getiphdr(&sb, &iph) == -1)
		goto out;
	hdrlen = iph.ip_hl * 4;
	if ((hdrlen > buflen) || (iph.ip_len > buflen))
		goto out;
	sb.sbuf_buflen = hdrlen; /* trim down this sbuf */

	if (naf_ipv4__debug > 1) {
		dvprintf(naf_ipv4__module, "received IP packet: ver = %d, ihl = %d, tot_len = %d, ttl = %d, saddr = 0x%08lx, daddr = 0x%08lx, protocol %d, csum 0x%04x\n",
							iph.ip_v, iph.ip_hl,
							iph.ip_len, iph.ip_ttl,
							iph.ip_src.s_addr,
							iph.ip_dst.s_addr,
							iph.ip_p,
							iph.ip_sum);
	}

	if (iph.ip_v != 4)
		goto out;
	if (iph.ip_ttl == 0)
		goto out;
	if (naf_ipv4__hdrcsum(buf, hdrlen) != iph.ip_sum) {
		if (naf_ipv4__debug)
			dvprintf(naf_ipv4__module, "bad IPv4 checksum from 0x%08lx\n", iph.ip_src.s_addr);
		goto out;
	}

	/* XXX make sure daddr is a valid address for recvif (uni/multi/broad) */

	memset(&ip, 0, sizeof(struct nafnet_ip));
	naf_sbuf_init(naf_ipv4__module, &ip.ip_data, buf + hdrlen,
							iph.ip_len - hdrlen);
	ip.ip_tos = iph.ip_tos; ip.ip_id = iph.ip_id; ip.ip_fragoff = iph.ip_off;
	ip.ip_ttl = iph.ip_ttl; ip.ip_protocol = iph.ip_p;
	ip.ip_saddr = iph.ip_src.s_addr; ip.ip_daddr = iph.ip_dst.s_addr;
	ip.ip_fields = NAF_NET_IPFIELD_TOS| NAF_NET_IPFIELD_ID |
		       NAF_NET_IPFIELD_FRAGOFF | NAF_NET_IPFIELD_TTL |
		       NAF_NET_IPFIELD_PROTOCOL | NAF_NET_IPFIELD_SADDR |
		       NAF_NET_IPFIELD_DADDR;
	if (ip.ip_fragoff & 0x4000)
		ip.ip_flags |= NAF_NET_IPFLAG_DONTFRAG;
	if (ip.ip_fragoff & 0x2000)
		ip.ip_flags |= NAF_NET_IPFLAG_MOREFRAG;
	ip.ip_fragoff &= ~0xe000; /* fragoff is actually only 13bits */
	if (naf_ipv4__parseoptions(&sb, &ip) == -1)
		goto outip;

	naf_ipv4_input__callhandler(&ip);

outip:
	naf_sbuf_free(naf_ipv4__module, &(ip.ip_data));
out:
	naf_sbuf_free(naf_ipv4__module, &sb);

	return 0;
}

#define NAF_NET_TTL_DEFAULT 255

int
naf_ipv4_output(struct nafmodule *mod, struct nafnet_ip *ip)
{
	naf_sbuf_t ips;
	naf_u16_t totlen;
	int ckpos;

	if (!mod || !ip)
		return -1;
	if (!(ip->ip_fields & NAF_NET_IPFIELD_DADDR) ||
	    !(ip->ip_fields & NAF_NET_IPFIELD_PROTOCOL))
		return -1;

	if (!ip->ip_rt)
		ip->ip_rt = naf_ipv4_getroute(ip->ip_daddr);
	if (!ip->ip_rt || !ip->ip_rt->rt_if)
		return -1;
	if (!(ip->ip_fields & NAF_NET_IPFIELD_SADDR)) {
		/* XXX this isn't technically correct. */
		ip->ip_saddr = ip->ip_rt->rt_if->if_addrs->ifa_ipv4.ifaa_ipv4_addr;
	}

	if (!(ip->ip_fields & NAF_NET_IPFIELD_TTL))
		ip->ip_ttl = NAF_NET_TTL_DEFAULT;
	if (!(ip->ip_fields & NAF_NET_IPFIELD_TOS))
		ip->ip_tos = 0;
	if (!(ip->ip_fields & NAF_NET_IPFIELD_ID))
		ip->ip_id = 0; /* eh. */

	ip->ip_fragoff = 0; /* never set by upper layers */
	if (ip->ip_flags & NAF_NET_IPFLAG_DONTFRAG)
		ip->ip_fragoff |= 0x4000;

	/*
	 * We could use naf_sbuf_prepend() on the current sbuf, but that's
	 * currently implemented in a really inefficient way.  This is probably
	 * better, but it's not like there's a whole lot of efficiency going
	 * on around here anyway.
	 */
	totlen = MINIPHDRLEN + ip->ip_data.sbuf_buflen;
	if (naf_sbuf_init(naf_ipv4__module, &ips, NULL, totlen) == -1)
		return -1;

	naf_sbuf_put8(&ips, (0x04 << 4) | 5); /* ihl = 5 (20bytes) */
	naf_sbuf_put8(&ips, ip->ip_tos);
	naf_sbuf_put16(&ips, totlen);
	naf_sbuf_put16(&ips, ip->ip_id);
	naf_sbuf_put16(&ips, ip->ip_fragoff);
	naf_sbuf_put8(&ips, ip->ip_ttl);
	naf_sbuf_put8(&ips, ip->ip_protocol);
		ckpos = naf_sbuf_getpos(&ips);
	naf_sbuf_put16(&ips, 0x0000); /* set later */
	naf_sbuf_put32(&ips, ip->ip_saddr);
	naf_sbuf_put32(&ips, ip->ip_daddr);

	/* XXX should probably honor requests for IP options and put here */

	naf_sbuf_rewind(&ip->ip_data);
	naf_sbuf_putraw(&ips, naf_sbuf_getposptr(&ip->ip_data), naf_sbuf_bytesremaining(&ip->ip_data));

	naf_sbuf_setpos(&ips, ckpos);
	naf_sbuf_put16(&ips, naf_ipv4__hdrcsum(ips.sbuf_buf, MINIPHDRLEN));
	naf_sbuf_rewind(&ips);

	if (!ip->ip_rt->rt_if->if_outputf ||
	    (ip->ip_rt->rt_if->if_outputf(naf_ipv4__module, ip->ip_rt->rt_if, &ips) == -1)) {
		naf_sbuf_free(naf_ipv4__module, &ips);
		return -1;
	}
	naf_ipv4_ip_free(naf_ipv4__module, ip);

	return 0;
}


/*
 * ipv4->route()
 *   IN:
 *
 *   OUT:
 *      array routes {
 *          array destination {
 *              scalar destination;
 *              scalar gateway;
 *              scalar mask;
 *              scalar flags;
 *              scalar metric;
 *		string ifname;
 *          }
 *      }
 *
 */
static void
__rpc_ipv4_route(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t **rts;
	struct nafnet_rt *rt;

	if (!(rts = naf_rpc_addarg_array(mod, &req->returnargs, "routes"))) {
		req->status = NAF_RPC_STATUS_UNKNOWNFAILURE;
		return;
	}

	for (rt = naf_ipv4__routetable; rt; rt = rt->rt__next) {
		naf_rpc_arg_t **outer;
		struct in_addr in;

		in.s_addr = rt->rt_dest;
		if ((outer = naf_rpc_addarg_array(mod, rts, inet_ntoa(in)))) {

			/* XXX When RPC actually gets a nice front-end, the
			 * scalar versions of these would be better.  But
			 * strings are easier to read with nafconsole.
			 */
			in.s_addr = rt->rt_dest;
			naf_rpc_addarg_string(mod, outer, "destination", inet_ntoa(in));
			in.s_addr = rt->rt_gw;
			naf_rpc_addarg_string(mod, outer, "gateway", inet_ntoa(in));
			in.s_addr = rt->rt_mask;
			naf_rpc_addarg_string(mod, outer, "mask", inet_ntoa(in));
			naf_rpc_addarg_scalar(mod, outer, "flags", rt->rt_flags);
			naf_rpc_addarg_scalar(mod, outer, "metric", rt->rt_metric);
			if (rt->rt_if)
				naf_rpc_addarg_string(mod, outer, "ifname", rt->rt_if->if_name);
		}
	}

	req->status = NAF_RPC_STATUS_SUCCESS;
	return;
}

static void
signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {

		NAFCONFIG_UPDATEINTMODPARMDEF(mod, "debug",
					      naf_ipv4__debug,
					      NAF_IPV4_DEBUG_DEFAULT);
	}

	return;
}

static int
modinit(struct nafmodule *mod)
{

	naf_ipv4__module = mod;
	naf_ipv4__routetable = NULL;
	naf_ipv4__protlist = NULL;

	naf_rpc_register_method(mod, "route", __rpc_ipv4_route, "View routing table");

	return 0;
}

static int
modshutdown(struct nafmodule *mod)
{

	naf_ipv4_prot__unregisterall();
	naf_ipv4_route__flush();
	naf_ipv4__module = NULL;

	naf_rpc_unregister_method(mod, "route");

	return 0;
}

static int
modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "ipv4");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->signal = signalhandler;

	return 0;
}

int
naf_ipv4__register(void)
{
	return naf_module__registerresident("ipv4", modfirst, NAF_MODULE_PRI_SECONDPASS);
}

#else /* NAF_USEIPV4 */

int
naf_ipv4__register(void)
{
	return 0;
}

#endif /* NAF_USEIPV4 */

