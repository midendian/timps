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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#include <naf/nafmodule.h>
#include <naf/nafconfig.h>
#include <naf/nafrpc.h>
#include <gnr/gnrmsg.h>
#include <gnr/gnrnode.h>
#include <naf/naftlv.h>

#include "oscar.h"
#include "oscar_internal.h"
#include "flap.h"
#include "ckcache.h"
#include "im.h"


#define TIMPS_OSCAR_DEBUG_DEFAULT 0
int timps_oscar__debug = TIMPS_OSCAR_DEBUG_DEFAULT;
struct nafmodule *timps_oscar__module = NULL;
#define TIMPS_OSCAR_AUTHORIZER_DEFAULT "login.oscar.aol.com:5190"
char *timps_oscar__authorizer = NULL;
#define TIMPS_OSCAR_ENABLEPROROGUEALL_DEFAULT 0
int timps_oscar__enableprorogueall = TIMPS_OSCAR_ENABLEPROROGUEALL_DEFAULT;
#define TIMPS_OSCAR_KEEPALIVE_FREQUENCY_DEFAULT 15
int timps_oscar__keepalive_frequency = TIMPS_OSCAR_KEEPALIVE_FREQUENCY_DEFAULT;
#define TIMPS_OSCAR_TXTIMEOUT_DEFAULT 30
int timps_oscar__txtimeout = TIMPS_OSCAR_TXTIMEOUT_DEFAULT;

static int
toscar_msgrouting(struct nafmodule *mod, int stage, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{

	if (!gmhi->srcnode || !gmhi->destnode)
		return 0;

	/* If the target is local and one of ours, take it. */
	if ((gmhi->destnode->metric == GNR_NODE_METRIC_LOCAL) &&
					(gmhi->destnode->ownermod == mod)) {
		gm->routeflags |= GNR_MSG_ROUTEFLAG_ROUTED_LOCAL;
		return 1;
	}

	/* If the source is local and one of ours, take it. */
	if ((gmhi->srcnode->metric == GNR_NODE_METRIC_LOCAL) &&
					(gmhi->srcnode->ownermod == mod)) {
		gm->routeflags |= GNR_MSG_ROUTEFLAG_ROUTED_FORWARD;
		return 1;
	}

	return 0;
}

int
toscar_sncmp(const char *sn1, const char *sn2)
{
	const char *p1, *p2;

	for (p1 = sn1, p2 = sn2; *p1 && *p2; p1++, p2++) {

		while (*p1 == ' ')
			p1++;
		while (*p2 == ' ')
			p2++;
		if (toupper(*p1) != toupper(*p2))
			return 1;
	}

	/* should both be NULL */
	if (*p1 != *p2)
		return 1;

	return 0;
}

static int
toscar_gnroutputfunc(struct nafmodule *mod, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{

	if (timps_oscar__debug > 1) {
		dvprintf(mod, "toscar_gnroutputfunc: type = %d, to = '%s'[%s](%d), from = '%s'[%s](%d), msg = (%s) '%s'\n",
				gm->type,
				gm->srcname, gm->srcnameservice,
				gmhi->srcnode ? gmhi->srcnode->metric : -1,
				gm->destname, gm->destnameservice,
				gmhi->destnode ? gmhi->destnode->metric : -1,
				gm->msgtexttype ? gm->msgtexttype : "type not specified",
				gm->msgtext);
	}

	if (gmhi->destnode->metric == GNR_NODE_METRIC_LOCAL) {
		struct nafconn *conn;

		if (!(conn = toscar__findconn(mod, gmhi->destnode->name))) {
			if (timps_oscar__debug > 0)
				dvprintf(mod, "gnroutputfunc(local): unable to find connection for local node '%s'[%s]\n", gmhi->destnode->name, gmhi->destnode->service);
			return -1;
		}

		toscar_icbm_sendincoming(mod, conn->endpoint, gm, gmhi);

	} else if (gmhi->srcnode->metric == GNR_NODE_METRIC_LOCAL) {
		struct nafconn *conn;

		if (!(conn = toscar__findconn(mod, gmhi->srcnode->name))) {
			if (timps_oscar__debug > 0)
				dvprintf(mod, "gnroutputfunc(forward): unable to find connection for local node '%s'[%s]\n", gmhi->srcnode->name, gmhi->srcnode->service);
			return -1;
		}

		toscar_icbm_sendoutgoing(mod, conn, gm, gmhi);
	}

	return 0;
}

static void
freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{

	if ((strcmp(tagname, "conn.logintlvs") == 0) ||
			(strcmp(tagname, "conn.cookietlvs") == 0))
		naf_tlv_free(mod, (naf_tlv_t *)tagdata);
	else if (strcmp(tagname, "conn.loginsnacid") == 0)
		; /* an int */
	else if (strcmp(tagname, "conn.screenname") == 0) {
		char *sn = (char *)tagdata;
		struct nafconn *conn = (struct nafconn *)object;
		struct gnrnode *node;

		if ((node = gnr_node_findbyname(sn, OSCARSERVICE)) &&
				!toscar__userhasotherclient(mod, sn, conn))
			gnr_node_offline(node, GNR_NODE_OFFLINE_REASON_DISCONNECTED);

		naf_free(mod, sn);

	} else if (strcmp(tagname, "gnrmsg.oscarmsgcookie") == 0) {
		naf_u8_t *msgck = (naf_u8_t *)tagdata;

		naf_free(mod, msgck);

	} else if (strcmp(tagname, "gnrmsg.snacid") == 0) {
		/* an int */
	} else if (strcmp(tagname, "gnrmsg.oscarmsgtlv") == 0) {
		naf_tlv_t *msgtlv = (naf_tlv_t *)tagdata;

		naf_tlv_free(mod, msgtlv);

	} else if (strcmp(tagname, "gnrmsg.extraoscartlvs") == 0) {
		naf_tlv_t *tlvs = (naf_tlv_t *)tagdata;

		naf_tlv_free(mod, tlvs);

	} else if (strcmp(tagname, "gnrmsg.srcuserinfo") == 0) {
		struct touserinfo *toui = (struct touserinfo *)tagdata;

		touserinfo_free(mod, toui);

	} else
		dvprintf(mod, "freetag: unknown tagname '%s'\n", tagname);

	return;
}


static int
takeconn(struct nafmodule *mod, struct nafconn *conn)
{

	conn->type &= ~NAF_CONN_TYPE_DETECTING;
	conn->type |= NAF_CONN_TYPE_FLAP;

	return toscar_flap_prepareconn(mod, conn);
}

static int
connready(struct nafmodule *mod, struct nafconn *conn, naf_u16_t what)
{

	if (what & NAF_CONN_READY_READ) {
		if (toscar_flap_handleread(mod, conn) == -1)
			return -1;
	}

	if (what & NAF_CONN_READY_WRITE) {
		if (toscar_flap_handlewrite(mod, conn) == -1)
			return -1;
	}

	return 0;
}

/*
 * oscar->disconnectuser()
 * IN:
 *    string sn;
 *
 * OUT:
 *    None.
 */
static void __rpc_oscar_disconnectuser(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t *name;

	name = naf_rpc_getarg(req->inargs, "sn");

	if (!name || (name->type != NAF_RPC_ARGTYPE_STRING)) {
		req->status = NAF_RPC_STATUS_INVALIDARGS;
		return;
	}

	req->status = NAF_RPC_STATUS_UNKNOWNFAILURE;
	if (toscar__force_disconnect(mod, name->data.string) != -1)
		req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}

static int
modinit(struct nafmodule *mod)
{

	timps_oscar__module = mod;

	if (gnr_msg_register(mod, toscar_gnroutputfunc) == -1) {
		dprintf(mod, "modinit: gsr_msg_register failed\n");
		return -1;
	}
	gnr_msg_addmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_ROUTING, 75, toscar_msgrouting, "Route AIM/OSCAR messages");

	naf_rpc_register_method(mod, "disconnectuser", __rpc_oscar_disconnectuser, "Forcefully disconnect an OSCAR user");

	return 0;
}

static int
modshutdown(struct nafmodule *mod)
{
	naf_rpc_unregister_method(mod, "disconnectuser");

	gnr_msg_remmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_ROUTING, toscar_msgrouting);
	gnr_msg_unregister(mod);

	timps_oscar__module = NULL;

	return 0;
}

static void
signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {

		NAFCONFIG_UPDATEINTMODPARMDEF(mod, "debug",
					      timps_oscar__debug,
					      TIMPS_OSCAR_DEBUG_DEFAULT);

		NAFCONFIG_UPDATESTRMODPARMDEF(mod, "authorizer",
					      timps_oscar__authorizer,
					      TIMPS_OSCAR_AUTHORIZER_DEFAULT);

		NAFCONFIG_UPDATEBOOLMODPARMDEF(mod, "enableprorogueall",
					       timps_oscar__enableprorogueall,
					       TIMPS_OSCAR_ENABLEPROROGUEALL_DEFAULT);

		NAFCONFIG_UPDATEINTMODPARMDEF(mod, "keepalivefrequency",
					      timps_oscar__keepalive_frequency,
					      TIMPS_OSCAR_KEEPALIVE_FREQUENCY_DEFAULT);

		NAFCONFIG_UPDATEINTMODPARMDEF(mod, "txtimeout",
					      timps_oscar__txtimeout,
					      TIMPS_OSCAR_TXTIMEOUT_DEFAULT);

	}

	return;
}

static int
toscar__keepalive_matcher(struct nafmodule *mod, struct nafconn *conn, const void *ud)
{
	const time_t now = (time_t)ud;

	if (!(conn->type & NAF_CONN_TYPE_FLAP) ||
	    !(conn->type & NAF_CONN_TYPE_SERVER))
		return 0;

	if ((now - conn->lasttx_soft) > timps_oscar__keepalive_frequency) {
		if (timps_oscar__debug > 1) {
			dvprintf(mod, "[%lu] sending nop (%d seconds since last tx)\n",
				 conn->cid,
				 now - conn->lasttx_soft);
		}
		toscar_flap_sendnop(mod, conn);
	}

	/*
	 * This closes connections that are 'stuck', at the TCP level.  If a
	 * host doesn't ack our data (to let us send our queue) for more than
	 * thirty seconds (default), then consider the connection dead. This
	 * can occur for a variety of reasons, most of them not good.
	 *
	 * The check above makes sure there's always pending data at least
	 * this often.
	 */
	if ((now - conn->lasttx_hard) > timps_oscar__txtimeout) {
		dvprintf(mod, "[%lu] connection timed out, closing (%d seconds since last hard tx)\n",
			 conn->cid,
			 now - conn->lasttx_hard);
		naf_conn_schedulekill(conn);
	}

	return 0;
}

static void
timerhandler(struct nafmodule *mod)
{
	time_t now;

	now = time(NULL);

	toscar_ckcache_timer(mod, now);
	naf_conn_find(mod, toscar__keepalive_matcher, (void *)now);

	return;
}

static int
modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "timps-oscar");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->freetag = freetag;
	mod->signal = signalhandler;
	mod->connready = connready;
	mod->takeconn = takeconn;
	mod->timer = timerhandler;
	mod->timerfreq = 5;

	return 0;
}

int
timps_oscar__register(void)
{
	return naf_module__registerresident("timps-oscar", modfirst, NAF_MODULE_PRI_THIRDPASS);
}

