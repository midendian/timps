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
#include <naf/nafconfig.h>
#include <naf/nafbufutils.h>

#include "../module.h" /* for naf_module__registerresident() */

#include "net.h"
#include "ipv4.h"

#define NAF_ICMPV4_DEBUG_DEFAULT 0
static int naf_icmpv4__debug = NAF_ICMPV4_DEBUG_DEFAULT;
static struct nafmodule *naf_icmpv4__module = NULL;

#define NAF_NET_IPPROTO_ICMP 1


struct nafnet_icmp {
	naf_u8_t ic_type;
	naf_u8_t ic_code;
	naf_u16_t ic_cksum;
	naf_u16_t ic_id;
	naf_u16_t ic_seqnum;
};

static
naf_u16_t naf_icmpv4__csum(naf_sbuf_t *sb)
{
	naf_u32_t origpos;
	naf_u32_t sum;

	origpos = naf_sbuf_getpos(sb);
	naf_sbuf_rewind(sb);

	for (sum = 0; naf_sbuf_bytesremaining(sb) > 1; )
		sum += naf_sbuf_get16(sb); /* add in network order */
	if (naf_sbuf_bytesremaining(sb) > 0) {
		union {
			naf_u8_t byte;
			naf_u16_t wyde;
		} odd;
		odd.wyde = 0;
		odd.byte = naf_sbuf_get8(sb);
		sum += odd.wyde;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	/* we added in network order, so the result is in network order */

	naf_sbuf_setpos(sb, origpos);
	return ~sum;
}

#define ICMP_TYPE_ECHOREPLY       0
#define ICMP_TYPE_DESTUNREACHABLE 3
#define ICMP_TYPE_ECHO            8

static int
sendechoreply(struct nafmodule *mod, struct nafnet_ip *oldip, struct nafnet_icmp *oldic)
{
	struct nafnet_ip *ip;
	int ckpos;

	if (!(ip = naf_ipv4_ip_new(mod)))
		return -1;
	ip->ip_protocol = NAF_NET_IPPROTO_ICMP;
	ip->ip_saddr = oldip->ip_daddr;
	ip->ip_daddr = oldip->ip_saddr;
	ip->ip_fields = NAF_NET_IPFIELD_PROTOCOL |
					NAF_NET_IPFIELD_SADDR |
					NAF_NET_IPFIELD_DADDR;

	naf_sbuf_put8(&ip->ip_data, ICMP_TYPE_ECHOREPLY);
	naf_sbuf_put8(&ip->ip_data, oldic->ic_code);
	ckpos = naf_sbuf_getpos(&ip->ip_data);
	naf_sbuf_put16(&ip->ip_data, 0x0000); /* later */
	naf_sbuf_put16(&ip->ip_data, oldic->ic_id);
	naf_sbuf_put16(&ip->ip_data, oldic->ic_seqnum);
	if (naf_sbuf_bytesremaining(&oldip->ip_data) > 0) {
		naf_u16_t datalen;
		naf_u8_t *data;

		datalen = naf_sbuf_bytesremaining(&oldip->ip_data);
		data = naf_sbuf_getposptr(&oldip->ip_data);
		naf_sbuf_putraw(&ip->ip_data, data, datalen);
	}
	ip->ip_data.sbuf_buflen = naf_sbuf_getpos(&ip->ip_data); /* trim */
	naf_sbuf_setpos(&ip->ip_data, ckpos);
	naf_sbuf_put16(&ip->ip_data, naf_icmpv4__csum(&ip->ip_data));

	if (naf_ipv4_output(mod, ip) == -1)
		naf_ipv4_ip_free(mod, ip);

	return 0;
}

static int
naf_icmpv4_input(struct nafmodule *mod, struct nafnet_ip *ip)
{
	struct nafnet_icmp ic;
	naf_u16_t nsum;
	int ret;

	ic.ic_type = naf_sbuf_get8(&ip->ip_data);
	ic.ic_code = naf_sbuf_get8(&ip->ip_data);
	ic.ic_cksum = naf_sbuf_get16(&ip->ip_data);
		naf_sbuf_setpos(&ip->ip_data, naf_sbuf_getpos(&ip->ip_data) - 2);
		naf_sbuf_put16(&ip->ip_data, 0); /* for recalculation */
	ic.ic_id = naf_sbuf_get16(&ip->ip_data);
	ic.ic_seqnum = naf_sbuf_get16(&ip->ip_data);

	nsum = naf_icmpv4__csum(&ip->ip_data);
	if (naf_icmpv4__csum(&ip->ip_data) != ic.ic_cksum)
		return -1;

	ret = 0;
	if (ic.ic_type == ICMP_TYPE_ECHOREPLY) {

		if (naf_icmpv4__debug)
			dvprintf(mod, "ICMP echo reply from 0x%08lx\n", ip->ip_saddr);

	} else if (ic.ic_type == ICMP_TYPE_DESTUNREACHABLE) {

		if (naf_icmpv4__debug)
			dvprintf(mod, "ICMP Destination Unreachable from 0x%08lx (code %d)\n", ip->ip_saddr, ic.ic_code);

	} else if (ic.ic_type == ICMP_TYPE_ECHO) {

		if (naf_icmpv4__debug)
			dvprintf(mod, "ICMP Echo Request from 0x%08lx\n", ip->ip_saddr);

		sendechoreply(mod, ip, &ic);

	} else {

		dvprintf(mod, "unknown ICMP type %d from 0x%08lx\n", ic.ic_type, ip->ip_saddr);
		ret = -1;
	}

	return ret;
}


static void
signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {

		NAFCONFIG_UPDATEINTMODPARMDEF(mod, "debug",
					      naf_icmpv4__debug,
					      NAF_ICMPV4_DEBUG_DEFAULT);
	}

	return;
}

static int
modinit(struct nafmodule *mod)
{

	naf_icmpv4__module = mod;

	naf_ipv4_prot_register(mod, NAF_NET_IPPROTO_ICMP, naf_icmpv4_input);

	return 0;
}

static int
modshutdown(struct nafmodule *mod)
{

	naf_ipv4_prot_unregister(mod, NAF_NET_IPPROTO_ICMP, naf_icmpv4_input);

	naf_icmpv4__module = NULL;

	return 0;
}

static int
modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "icmpv4");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->signal = signalhandler;

	return 0;
}

int
naf_icmpv4__register(void)
{
	return naf_module__registerresident("icmpv4", modfirst, NAF_MODULE_PRI_SECONDPASS);
}

