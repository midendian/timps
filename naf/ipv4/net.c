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

/* XXX should be able to compile this without IPv4, since we may have other
 * protocols in the future
 */
#ifdef NAF_USEIPV4

#include <naf/nafconfig.h>
#include <naf/nafrpc.h>

#include "../module.h" /* for naf_module__registerresident() */

#include "net.h"
#include "ipv4.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#define NAF_NET_DEBUG_DEFAULT 0
static int naf_net__debug = NAF_NET_DEBUG_DEFAULT;
static struct nafmodule *naf_net__module = NULL;
static struct nafnet_if *nafnet__iflist = NULL;


static int naf_net_if_addr_remallbytype(struct nafnet_if *iff, naf_u16_t type);
static int naf_net_if_addr_add_ipv4(struct nafnet_if *iff, naf_u32_t addr, naf_u32_t mask);


static const char *
mkpname(const char *p0, ...)
{
	static char buf[128];
	va_list v;
	char *cp;
	int i;

	i = snprintf(buf, sizeof(buf), "interface[%s]", p0);
	va_start(v, p0);
	for ((cp = va_arg(v, char *)); cp; (cp = va_arg(v, char *)))
		i += snprintf(buf+i, sizeof(buf)-i, "[%s]", cp);
	va_end(v);

	return buf;
}

static int
naf_net_if__configure(struct nafnet_if *iff)
{
	const char *pn;
	char *p;

	naf_net_if_addr_remallbytype(iff, NAFNET_AF_INET);
#ifdef NAFNET_USE_IPV4
	/* XXX should be in ipv4.c */ {
		struct in_addr ina, inam;

		memset(&ina, 0, sizeof(struct in_addr));
		memset(&inam, 0, sizeof(struct in_addr));

		pn = mkpname(iff->if_name, "ipv4", "address", NULL);
		p = naf_config_getmodparmstr(naf_net__module, pn);
		if (p)
			inet_aton(p, &ina);

		pn = mkpname(iff->if_name, "ipv4", "mask", NULL);
		p = naf_config_getmodparmstr(naf_net__module, pn);
		if (p)
			inet_aton(p, &inam);

		/* make up an old-style (class-based) mask if not specified */
		if ((ina.s_addr != 0) && (inam.s_addr == 0)) {
			if (ina.s_addr & 0x80000000) {
				if (ina.s_addr & 0x40000000)
					inam.s_addr = 0xffffff00;
				else
					inam.s_addr = 0xffff0000;
			} else
				inam.s_addr = 0xff000000;
		}

		/* XXX do checking here? (make sure in isn't net or bcast) */
		if ((ina.s_addr == 0) || (inam.s_addr == 0) ||
		    (naf_net_if_addr_add_ipv4(iff, ina.s_addr, inam.s_addr) == -1)) {
			tvprintf(naf_net__module, "failed to add address %s to %s\n", inet_ntoa(ina), iff->if_name);
		}
	}
#endif /* def NAFNET_USE_IPV4 */

	{
		int nmtu;

		/* XXX interface can't set its desired default MTU */
		iff->if_mtu = NAFNET_MTU_DEFAULT;

		pn = mkpname(iff->if_name, "mtu", NULL);
		p = naf_config_getmodparmstr(naf_net__module, pn);
		if (p) {
			if ((nmtu = atoi(p)) == -1)
				tvprintf(naf_net__module, "invalid MTU configured for %s\n", iff->if_name);
			else
				iff->if_mtu = (naf_u16_t)nmtu;
		}
	}

	{
		int nstate = -1;

		pn = mkpname(iff->if_name, "state", NULL);
		p = naf_config_getmodparmstr(naf_net__module, pn);
		if (p) {
			if (strcasecmp(p, "up") == 0)
				nstate = 1;
			else if (strcasecmp(p, "down") == 0)
				nstate = 0;
			else
				tvprintf(naf_net__module, "unknown state configured for %s\n", iff->if_name);
		}
		if ((nstate == 1 && !(iff->if_flags & NAFNET_IFFLAG_UP))) {
			tvprintf(naf_net__module, "bringing up interface %s\n", iff->if_name);
			iff->if_flags |= NAFNET_IFFLAG_UP;
			naf_ipv4_event_ifup(iff); /* XXX need better API (this way will not update when addresses change without up/down */
		} else if ((nstate == 0) && (iff->if_flags & NAFNET_IFFLAG_UP)) {
			tvprintf(naf_net__module, "taking down interface %s\n", iff->if_name);
			iff->if_flags &= ~NAFNET_IFFLAG_UP;
			naf_ipv4_event_ifdown(iff); /* XXX need better API */
		} else
			; /* no change or invalid */
	}

	return 0;
}

static void
naf_net_if__reconfigureall(void)
{
	struct nafnet_if *ifc;

	for (ifc = nafnet__iflist; ifc; ifc = ifc->if__next)
		naf_net_if__configure(ifc);

	return;
}

struct nafnet_if *
naf_net_if_find(const char *ifname)
{
	struct nafnet_if *ifc;

	if (!ifname)
		return nafnet__iflist; /* XXX bad */

	for (ifc = nafnet__iflist; ifc; ifc = ifc->if__next) {
		if (strcmp(ifname, ifc->if_name) == 0)
			return ifc;
	}
	return NULL;
}

struct nafnet_if *
naf_net_if__alloc(struct nafmodule *mod, const char *name)
{
	struct nafnet_if *iff;

	if (!(iff = naf_malloc(naf_net__module, sizeof(struct nafnet_if))))
		return NULL;
	memset(iff, 0, sizeof(struct nafnet_if));

	strncpy(iff->if_name, name, NAFNET_MAXIFNAMELEN);
	iff->if_flags = NAFNET_IFFLAG_NONE; /* start 'down' */
	iff->if_mtu = NAFNET_MTU_DEFAULT;
	iff->if_addrs = NULL;
	iff->if_owner = mod;

	return iff;
}

static struct nafnet_ifaddr *
naf_net_ifaddr__alloc(void)
{
	struct nafnet_ifaddr *ifa;

	if (!(ifa = naf_malloc(naf_net__module, sizeof(struct nafnet_ifaddr))))
		return NULL;
	memset(ifa, 0, sizeof(struct nafnet_ifaddr));

	return ifa;
}

static int
naf_net_if_addr_add_ipv4(struct nafnet_if *iff, naf_u32_t addr, naf_u32_t mask)
{
#ifdef NAFNET_USE_IPV4
	struct nafnet_ifaddr *ifa;

	if (!(ifa = naf_net_ifaddr__alloc()))
		return -1;

	ifa->ifa_type = NAFNET_AF_INET;
	ifa->ifa_ipv4.ifaa_ipv4_addr = addr;
	ifa->ifa_ipv4.ifaa_ipv4_mask = mask;

	ifa->ifa__next = iff->if_addrs;
	iff->if_addrs = ifa;
#endif /* def NAFNET_USE_IPV4 */
	return 0;
}

static void
naf_net_ifaddr__free(struct nafnet_ifaddr *ifa)
{
	naf_free(naf_net__module, ifa);
	return;
}

static void
naf_net_ifaddr__freeall(struct nafnet_ifaddr *ifa)
{
	while (ifa) {
		struct nafnet_ifaddr *ifat;

		ifat = ifa->ifa__next;
		naf_net_ifaddr__free(ifa);
		ifa = ifat;
	}
	return;
}

static int
naf_net_if_addr_remallbytype(struct nafnet_if *iff, naf_u16_t type)
{
	struct nafnet_ifaddr *ifac, **ifap;

	for (ifap = &iff->if_addrs; (ifac = *ifap); ) {
		if (ifac->ifa_type == type) {
			*ifap = ifac->ifa__next;
			naf_net_ifaddr__free(ifac);
		} else
			ifap = &ifac->ifa__next;
	}

	return 0;
}

static void
naf_net_if__free(struct nafnet_if *iff)
{
	naf_net_ifaddr__freeall(iff->if_addrs);
	naf_free(naf_net__module, iff);
	return;
}

struct nafnet_if *
naf_net__allocif(struct nafmodule *mod, const char *name)
{
	char nname[NAFNET_MAXIFNAMELEN];
	int n;

	if (!strchr(name, '%')) {
		if (naf_net_if_find(name))
			return NULL;
		return naf_net_if__alloc(mod, name);
	}

	n = 0;
	do {
		snprintf(nname, sizeof(nname), name, n++);
	} while ((n < 10) && naf_net_if_find(nname));
	if (n == 10)
		return NULL;

	return naf_net_if__alloc(mod, nname);
}

struct nafnet_if *
naf_net_if_add(struct nafmodule *mod, const char *name, naf_net_if_outputfunc_t outf)
{
	struct nafnet_if *iff;

	if (!mod || !name || !outf)
		return NULL;

	if (!(iff = naf_net__allocif(mod, name)))
		return NULL;
	iff->if_outputf = outf;

	iff->if__next = nafnet__iflist;
	nafnet__iflist = iff;

	naf_net_if__configure(iff);

	return iff;
}

int
naf_net_if_rem(struct nafmodule *mod, struct nafnet_if *iff)
{
	struct nafnet_if *ifc, **ifp;

	if (!mod || !iff)
		return -1;

	for (ifp = &nafnet__iflist; (ifc = *ifp); ) {
		if (ifc == iff) {
			*ifp = ifc->if__next;
			naf_net_if__free(iff);
			return 0;
		}
		ifp = &ifc->if__next;
	}
	return -1;
}


static void
signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {

		NAFCONFIG_UPDATEINTMODPARMDEF(mod, "debug",
					      naf_net__debug,
					      NAF_NET_DEBUG_DEFAULT);

		naf_net_if__reconfigureall();
	}

	return;
}


static int
__rpc_net_ifconfig__addif(struct nafmodule *mod, naf_rpc_arg_t **parent, struct nafnet_if *iff)
{
	naf_rpc_arg_t **ifr;
	struct nafnet_ifaddr *ifa;

	if (!(ifr = naf_rpc_addarg_array(mod, parent, iff->if_name)))
		return -1;

	naf_rpc_addarg_string(mod, ifr, "name", iff->if_name);
	naf_rpc_addarg_string(mod, ifr, "state", (iff->if_flags & NAFNET_IFFLAG_UP) ? "up" : "down");
	naf_rpc_addarg_scalar(mod, ifr, "flags", iff->if_flags);
	naf_rpc_addarg_scalar(mod, ifr, "mtu", iff->if_mtu);
	for (ifa = iff->if_addrs; ifa; ifa = ifa->ifa__next) {
		naf_rpc_arg_t **ifar;

		if (ifa->ifa_type != NAFNET_AF_INET)
			continue; /* XXX shrug. */

		if ((ifar = naf_rpc_addarg_array(mod, ifr, "ipv4"))) {
			struct in_addr in;

			in.s_addr = ifa->ifa_ipv4_addr;
			naf_rpc_addarg_string(mod, ifar, "address", inet_ntoa(in));

			in.s_addr = ifa->ifa_ipv4_mask;
			naf_rpc_addarg_string(mod, ifar, "mask", inet_ntoa(in));
		}
	}

	return 0;
}

/*
 * net->ifconfig()
 *   IN:
 *      [optional] string ifname;
 *
 *   OUT:
 *      array interfaces {
 *          array ifname {
 *		string ifname;
 *              array protocol {
 *	            [optional] string address;
 *		}
 *          }
 *      }
 *
 */
static void
__rpc_net_ifconfig(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t *ifname;
	naf_rpc_arg_t **ifs;
	struct nafnet_if *iff;

	if ((ifname = naf_rpc_getarg(req->inargs, "ifname"))) {
		if (ifname->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	if (!(ifs = naf_rpc_addarg_array(mod, &req->returnargs, "interfaces"))) {
		req->status = NAF_RPC_STATUS_UNKNOWNFAILURE;
		return;
	}

	if (ifname) {

		if ((iff = naf_net_if_find(ifname->data.string)))
			__rpc_net_ifconfig__addif(mod, ifs, iff);

		req->status = NAF_RPC_STATUS_SUCCESS;
		return;
	}

	for (iff = nafnet__iflist; iff; iff = iff->if__next)
		__rpc_net_ifconfig__addif(mod, ifs, iff);

	req->status = NAF_RPC_STATUS_SUCCESS;
	return;
}

static int
modinit(struct nafmodule *mod)
{

	naf_net__module = mod;

	naf_rpc_register_method(mod, "ifconfig", __rpc_net_ifconfig, "Configure interfaces");

	return 0;
}

static int
modshutdown(struct nafmodule *mod)
{

	naf_rpc_unregister_method(mod, "ifconfig");

	naf_net__module = NULL;

	return 0;
}

static int
modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "net");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->signal = signalhandler;

	return 0;
}

int
naf_net__register(void)
{
	return naf_module__registerresident("net", modfirst, NAF_MODULE_PRI_FIRSTPASS);
}

#else /* NAF_USEIPV4 */

int
naf_net__register(void)
{
	return 0;
}

#endif

