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

#include "../module.h" /* for naf_module__registerresident() */

#include "net.h"
#include "ipv4.h"
#include "linuxtun.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_LINUX_NETDEVICE_H
#include <linux/netdevice.h>
#endif

/* XXX should probably have a --enable=linuxtun or something to configure */
#ifdef HAVE_LINUX_IF_TUN_H

#include <linux/if_tun.h>


#define NAF_LINUXTUN_DEBUG_DEFAULT 0
static int naf_linuxtun__debug = NAF_LINUXTUN_DEBUG_DEFAULT;
static struct nafmodule *naf_linuxtun__module = NULL;
static struct nafnet_if *naf_linuxtun__if = NULL;
static struct nafconn *naf_linuxtun__ifconn = NULL;

static int
outputfunc(struct nafmodule *caller, struct nafnet_if *iff, naf_sbuf_t *sb)
{
	naf_u8_t *buf;
	int buflen;

	if (iff != naf_linuxtun__if)
		return -1;

	/*
	 * The proper sbuf way to do this would be getrawbuf(), but fuck that,
	 * we already have the exact block we need.
	 */
	buf = sb->sbuf_buf;
	buflen = sb->sbuf_buflen;
	sb->sbuf_flags &= ~NAF_SBUF_FLAG_FREEBUF;

	if (naf_conn_reqwrite(naf_linuxtun__ifconn, buf, buflen) == -1) {
		naf_free(caller, buf);
		return -1;
	}
	naf_sbuf_free(caller, sb);

	return 0;
}

static int
opentun(void)
{
	int fd;
	struct ifreq ifr;

	fd = open("/dev/net/tun", O_RDWR);
	if (fd == -1) {
		dvprintf(naf_linuxtun__module, "open(/dev/net/tun): %s\n", strerror(errno));
		return -1;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

	if (ioctl(fd, TUNSETIFF, &ifr) == -1) {
		dvprintf(naf_linuxtun__module, "ioctl(TUNSETIFF): %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	dvprintf(naf_linuxtun__module, "opened Linux tun device %s\n", ifr.ifr_name);

	naf_linuxtun__ifconn = naf_conn_addconn(naf_linuxtun__module,
						fd,
						NAF_CONN_TYPE_LOCAL|NAF_CONN_TYPE_READRAW);
	naf_linuxtun__if = naf_net_if_add(naf_linuxtun__module, "lnx%d", outputfunc);

	return 0;
}

static int
connready(struct nafmodule *mod, struct nafconn *conn, naf_u16_t what)
{

	if (what & NAF_CONN_READY_WRITE) {
		naf_u8_t *buf;
		int buflen;

		if (naf_conn_takewrite(conn, &buf, &buflen) == -1)
			return -1;
		naf_free(mod, buf);

		return 0;
	}

	if (what & NAF_CONN_READY_READ) {
		static naf_u8_t dg[65536];
		int err;

		err = read(conn->fdt->fd, dg, sizeof(dg));
		if (err <= 0) {
			dvprintf(naf_linuxtun__module, "error reading from tun device: %s\n", (err == 0) ? "EOF" : strerror(err));
			naf_conn_schedulekill(conn);
			return -1;
		}

		dvprintf(naf_linuxtun__module, "received %d octet datagram\n", err);
		naf_ipv4_input(naf_linuxtun__module, naf_linuxtun__if, dg, err);

		return 0;
	}

	return 0;
}

static int
modinit(struct nafmodule *mod)
{

	naf_linuxtun__module = mod;

	if (opentun() == -1)
		dprintf(naf_linuxtun__module, "failed to open Linux TUN device\n");

	return 0;
}

static int
modshutdown(struct nafmodule *mod)
{

	if (naf_linuxtun__if)
		naf_net_if_rem(mod, naf_linuxtun__if);
	if (naf_linuxtun__ifconn)
		naf_conn_schedulekill(naf_linuxtun__ifconn);

	naf_linuxtun__module = NULL;

	return 0;
}

static void
freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{
	return;
}

static void
signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {

		NAFCONFIG_UPDATEINTMODPARMDEF(mod, "debug",
					      naf_linuxtun__debug,
					      NAF_LINUXTUN_DEBUG_DEFAULT);
	}

	return;
}

static void
connkill(struct nafmodule *mod, struct nafconn *conn)
{

	if (conn == naf_linuxtun__ifconn) {
		naf_net_if_rem(mod, naf_linuxtun__if);
		naf_linuxtun__if = NULL;
		naf_linuxtun__ifconn = NULL;
	}

	return;
}

static int
modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "linuxtun");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->freetag = freetag;
	mod->signal = signalhandler;
	mod->connkill = connkill;
	mod->connready = connready;

	return 0;
}

int
naf_linuxtun__register(void)
{
	return naf_module__registerresident("linuxtun", modfirst, NAF_MODULE_PRI_THIRDPASS);
}

#else /* HAVE_LINUX_IF_TUN_H */

int
naf_linuxtun__register(void)
{
	return 0;
}

#endif

