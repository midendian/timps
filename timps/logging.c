
#include <naf/nafmodule.h>
#include <gnr/gnrmsg.h>
#include <gnr/gnrevents.h>


struct nafmodule *timps_logging__module = NULL;


static int
tlogging_msglogger(struct nafmodule *mod, int stage, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
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

	dvprintf(mod, "MESSAGE: %s: %s[%s][%s] -> %s[%s][%s] [%s by %s]: %s\n",
			typename,

			gmhi->srcnode ? gmhi->srcnode->name : gm->srcname,
			gmhi->srcnode ? gmhi->srcnode->service : gm->srcnameservice,
			(gmhi->srcnode && gmhi->srcnode->ownermod) ? gmhi->srcnode->ownermod->name : "unknown",

			gmhi->destnode ? gmhi->destnode->name : gm->destname,
			gmhi->destnode ? gmhi->destnode->service : gm->destnameservice,
			(gmhi->destnode && gmhi->destnode->ownermod) ? gmhi->srcnode->ownermod->name : "unknown",

			route, gmhi->targetmod ? gmhi->targetmod->name : "unknown",

			gm->msgstring);

	/* XXX admin logs, personal logs, etc */

	return 0;
}

static void
tlogging_nodeeventhandler(struct nafmodule *mod, struct gnr_event_info *gei)
{
	struct gnr_event_ei_nodechange *einc;

	einc = (struct gnr_event_ei_nodechange *)gei->gei_extinfo;

	if (gei->gei_event == GNR_EVENT_NODEUP) {

		dvprintf(mod, "user online: %s[%s][%s][%s%s%s]\n",
				gei->gei_node->name,
				gei->gei_node->service,
				gei->gei_node->ownermod ? gei->gei_node->ownermod->name : "unknown",
				(gei->gei_node->metric == GNR_NODE_METRIC_LOCAL) ? "local" : "",
				GNR_NODE_METRIC_ISPEERED(gei->gei_node->metric) ? "peer" : "",
				(gei->gei_node->metric == GNR_NODE_METRIC_MAX) ? "remote" : "");

	} else if (gei->gei_event == GNR_EVENT_NODEDOWN) {

		dvprintf(mod, "user offline: %s[%s][%s][%s%s%s] -- %s%s%s\n",
				gei->gei_node->name,
				gei->gei_node->service,
				gei->gei_node->ownermod ? gei->gei_node->ownermod->name : "unknown",
				(gei->gei_node->metric == GNR_NODE_METRIC_LOCAL) ? "local" : "",
				GNR_NODE_METRIC_ISPEERED(gei->gei_node->metric) ? "peer" : "",
				(gei->gei_node->metric == GNR_NODE_METRIC_MAX) ? "remote" : "",
				(einc && (einc->reason == GNR_NODE_OFFLINE_REASON_TIMEOUT)) ? "timed out" : "",
				(einc && (einc->reason == GNR_NODE_OFFLINE_REASON_DISCONNECTED)) ? "disconnected" : "",
				(!einc || ((einc->reason != GNR_NODE_OFFLINE_REASON_TIMEOUT) && (einc->reason != GNR_NODE_OFFLINE_REASON_DISCONNECTED))) ? "unknown reason" : "");
	}

	/* XXX admin log, personal logs, etc */

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
#if 0 /* XXX check for log file changes */
#endif
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

