
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include <naf/nafmodule.h>
#include <naf/nafconfig.h>
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
		struct gnrnode *node;

		if ((node = gnr_node_findbyname(sn, OSCARSERVICE)))
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


static int
modinit(struct nafmodule *mod)
{

	timps_oscar__module = mod;

	if (gnr_msg_register(mod, toscar_gnroutputfunc) == -1) {
		dprintf(mod, "modinit: gsr_msg_register failed\n");
		return -1;
	}
	gnr_msg_addmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_ROUTING, 75, toscar_msgrouting, "Route AIM/OSCAR messages");

	return 0;
}

static int
modshutdown(struct nafmodule *mod)
{

	gnr_msg_remmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_ROUTING, toscar_msgrouting);
	gnr_msg_unregister(mod);

	timps_oscar__module = NULL;

	return 0;
}

static void
signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {
		char *str;

		if ((str = naf_config_getmodparmstr(mod, "debug")))
			timps_oscar__debug = atoi(str);
		if (timps_oscar__debug == -1)
			timps_oscar__debug = TIMPS_OSCAR_DEBUG_DEFAULT;


		if (timps_oscar__authorizer) {
			naf_free(mod, timps_oscar__authorizer);
			timps_oscar__authorizer = NULL;
		}
		if ((str = naf_config_getmodparmstr(mod, "authorizer")))
			timps_oscar__authorizer = naf_strdup(mod, str);
		if (!timps_oscar__authorizer)
			timps_oscar__authorizer = naf_strdup(mod, TIMPS_OSCAR_AUTHORIZER_DEFAULT);
	}

	return;
}

static void
timerhandler(struct nafmodule *mod)
{
	time_t now;

	now = time(NULL);

	toscar_ckcache_timer(mod, now);

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

