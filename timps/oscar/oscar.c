
#include <stdlib.h>
#include <time.h>

#include <naf/nafmodule.h>
#include <naf/nafconfig.h>
#include <gnr/gnrmsg.h>

#include "oscar.h"
#include "oscar_internal.h"
#include "flap.h"
#include "ckcache.h"


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
			(gmhi->destnode->ownermod == mod))
		return 1;
	/* If the source is local and one of ours, take it. */
	if ((gmhi->srcnode->metric == GNR_NODE_METRIC_LOCAL) &&
			(gmhi->srcnode->ownermod == mod))
		return 1;

	return 0;
}

static int
toscar_gnroutputfunc(struct nafmodule *mod, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{
	/* XXX */
	
	if (timps_oscar__debug > 0) {
		dvprintf(mod, "toscar_gnroutputfunc: type = %d, to = '%s'[%s](%d), from = '%s'[%s](%d), msg = (%s) '%s'\n",
				gm->type,
				gm->srcname, gm->srcnameservice,
				gmhi->srcnode ? gmhi->srcnode->metric : -1,
				gm->destname, gm->destnameservice,
				gmhi->destnode ? gmhi->destnode->metric : -1,
				gm->msgtexttype ? gm->msgtexttype : "type not specified",
				gm->msgtext);
	}

	return 0;
}

static void
freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{

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

