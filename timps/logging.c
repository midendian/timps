
/*
 * Should have four types of logs:
 *   - system log [this is handled by naf]
 *   - admin log (contains all messages, session start/end, node up/down)
 *   - per-user log (seperate log file for each user)
 *   - per-session log (seperate log file for each session) (for emailing, etc)
 *
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#include <naf/nafmodule.h>
#include <naf/nafconfig.h>
#include <gnr/gnrmsg.h>
#include <gnr/gnrevents.h>


static struct nafmodule *timps_logging__module = NULL;
static char *timps_logging__adminlogfn = NULL;
static FILE *timps_logging__adminlogstream = NULL;


static char *myctime(void)
{
	static char retbuf[64];
	struct tm *lt;
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	lt = localtime((time_t *)&tv.tv_sec);
	strftime(retbuf, 64, "%a %b %e %H:%M:%S %Z %Y", lt);

	return retbuf;
}


static void tlogging__logmsg_admin(struct nafmodule *mod, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{
	static const char *typenames[] = {
		"unknown", "message", "group-invite", "group-message",
		"group-join", "group-part", "rendezvous",
	};
	const char *typename = typenames[0];
	static const char *routes[] = {
		"not routed", "locally routed", "routed through peer", "forwarded to host",
		"routed internally", "dropped", "delayed",
	};
	const char *route = routes[0];

	if (gm->type == GNR_MSG_MSGTYPE_IM) typename = typenames[1];
	else if (gm->type == GNR_MSG_MSGTYPE_GROUPINVITE) typename = typenames[2];
	else if (gm->type == GNR_MSG_MSGTYPE_GROUPIM) typename = typenames[3];
	else if (gm->type == GNR_MSG_MSGTYPE_GROUPJOIN) typename = typenames[4];
	else if (gm->type == GNR_MSG_MSGTYPE_GROUPPART) typename = typenames[5];
	else if (gm->type == GNR_MSG_MSGTYPE_RENDEZVOUS) typename = typenames[6];

	if (gm->routeflags & GNR_MSG_ROUTEFLAG_ROUTED_LOCAL) route = routes[1];
	else if (gm->routeflags & GNR_MSG_ROUTEFLAG_ROUTED_PEER) route = routes[2];
	else if (gm->routeflags & GNR_MSG_ROUTEFLAG_ROUTED_FORWARD) route = routes[3];
	else if (gm->routeflags & GNR_MSG_ROUTEFLAG_ROUTED_INTERNAL) route = routes[4];
	else if (gm->routeflags & GNR_MSG_ROUTEFLAG_DROPPED) route = routes[5];
	else if (gm->routeflags & GNR_MSG_ROUTEFLAG_DELAYED) route = routes[6];

	fprintf(timps_logging__adminlogstream,
			"%s  %16.16s:  %s[%s][%s] -> %s[%s][%s] [%s by %s]: [%s] %s\n",
			myctime(),

			typename,

			gmhi->srcnode ? gmhi->srcnode->name : gm->srcname,
			gmhi->srcnode ? gmhi->srcnode->service : gm->srcnameservice,
			(gmhi->srcnode && gmhi->srcnode->ownermod) ? gmhi->srcnode->ownermod->name : "unknown",

			gmhi->destnode ? gmhi->destnode->name : gm->destname,
			gmhi->destnode ? gmhi->destnode->service : gm->destnameservice,
			(gmhi->destnode && gmhi->destnode->ownermod) ? gmhi->destnode->ownermod->name : "unknown",

			route, gmhi->targetmod ? gmhi->targetmod->name : "unknown",

			gm->msgtexttype ? gm->msgtexttype : "text/plain",
			gm->msgtext);
	fflush(timps_logging__adminlogstream);

	return;
}

static int
tlogging_msglogger(struct nafmodule *mod, int stage, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{

	if (timps_logging__adminlogstream)
		tlogging__logmsg_admin(mod, gm, gmhi);

	/* XXX per-user log */
	/* XXX per-session log */

	return 0;
}

static void tlogging__lognodeevent_admin(struct nafmodule *mod, struct gnrnode *node, gnr_event_t event, naf_u32_t reason)
{
	static const char *eventnames[] = {
		"unknown event", "user connected", "user disconnected",
		"user flag change"
	};
	const char *eventname = eventnames[0];
	static const char *offlinereasons[] = {
		"unknown reason", "remote user timeout", "disconnected"
	};
	const char *rstr = NULL;

	if (event == GNR_EVENT_NODEUP) eventname = eventnames[1];
	else if (event == GNR_EVENT_NODEDOWN) eventname = eventnames[2];
	else if (event == GNR_EVENT_NODEFLAGCHANGE) eventname = eventnames[3];

	if (event == GNR_EVENT_NODEDOWN) {
		if (reason == GNR_NODE_OFFLINE_REASON_UNKNOWN)
			rstr = offlinereasons[0];
		else if (reason == GNR_NODE_OFFLINE_REASON_TIMEOUT)
			rstr = offlinereasons[1];
		else if (reason == GNR_NODE_OFFLINE_REASON_DISCONNECTED)
			rstr = offlinereasons[2];
	}

	fprintf(timps_logging__adminlogstream,
			"%s  %s:  %s[%s][%s][%s%s%s] %s%s%s\n",
			myctime(),
			eventname,
			node->name, node->service,
			node->ownermod ? node->ownermod->name : "unknown",
			(node->metric == GNR_NODE_METRIC_LOCAL) ? "local" : "",
			GNR_NODE_METRIC_ISPEERED(node->metric) ? "peer" : "",
			(node->metric == GNR_NODE_METRIC_MAX) ? "remote" : "",
			rstr ? "(" : "", rstr ? rstr : "", rstr ? ")" : "");
	fflush(timps_logging__adminlogstream);

	return;
}

static void
tlogging_nodeeventhandler(struct nafmodule *mod, struct gnr_event_info *gei)
{
	struct gnr_event_ei_nodechange *einc;

	einc = (struct gnr_event_ei_nodechange *)gei->gei_extinfo;

	if (timps_logging__adminlogstream) {
		tlogging__lognodeevent_admin(mod, gei->gei_node, gei->gei_event, 
				einc ? einc->reason :
					GNR_NODE_OFFLINE_REASON_UNKNOWN);
	}

	/* XXX per-user log */
	/* XXX per-session log */

	return;
}


static int
modinit(struct nafmodule *mod)
{

	timps_logging__module = mod;

	if (gnr_msg_register(mod, NULL /* no outputfunc */) == -1) {
		dprintf(mod, "modinit: gsr_msg_register failed\n");
		return -1;
	}
	gnr_msg_addmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_POSTROUTING, 50, tlogging_msglogger, "Log messages");

	gnr_event_register(mod, tlogging_nodeeventhandler, GNR_EVENTMASK_NODE);

	return 0;
}

static int
modshutdown(struct nafmodule *mod)
{

	if (timps_logging__adminlogstream) {
		fprintf(timps_logging__adminlogstream,
				"%s  admin logging stopped (shutting down)\n",
				myctime());
		fflush(timps_logging__adminlogstream);
	}

	gnr_event_unregister(mod, tlogging_nodeeventhandler);
gnr_msg_remmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_POSTROUTING, tlogging_msglogger);
	gnr_msg_unregister(mod);

	timps_logging__module = NULL;

	return 0;
}

static void
signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {
		char *npath, *nadminfn, *nadminfn_full = NULL;

		npath = naf_config_getmodparmstr(mod, "logfilepath");
		if ((nadminfn = naf_config_getmodparmstr(mod, "adminlogfile"))) {
			int len;

			len = (npath ? strlen(npath) : 0) + 1 + strlen(nadminfn) + 1;
			if ((nadminfn_full = naf_malloc(mod, len))) {
				snprintf(nadminfn_full, len, "%s/%s",
						npath ? npath : "",
						nadminfn);
			}
		}

		if ((!!timps_logging__adminlogfn != !!nadminfn_full) ||
				(nadminfn_full && timps_logging__adminlogfn && (strcmp(timps_logging__adminlogfn, nadminfn_full) != 0))) {
			/* kill the old one */
			if (timps_logging__adminlogstream) {
				fprintf(timps_logging__adminlogstream,
						"%s  admin logging stopped\n",
						myctime());
				fflush(timps_logging__adminlogstream);
				fclose(timps_logging__adminlogstream);
				timps_logging__adminlogstream = NULL;
			}
			naf_free(mod, timps_logging__adminlogfn);
			timps_logging__adminlogfn = NULL;
		}

		if (nadminfn_full && !timps_logging__adminlogstream) {
			FILE *f;

			if ((f = fopen(nadminfn_full, "a+"))) {
				timps_logging__adminlogfn = nadminfn_full;
				nadminfn_full = NULL;
				timps_logging__adminlogstream = f;
				f = NULL;

				fprintf(timps_logging__adminlogstream,
						"%s  admin logging started\n",
						myctime());
				fflush(timps_logging__adminlogstream);
			}
		}
	}

	return;
}

static int
modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "timps-logging");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->signal = signalhandler;

	return 0;
}


int
timps_logging__register(void)
{
	return naf_module__registerresident("timps-logging", modfirst, NAF_MODULE_PRI_THIRDPASS);
}

