
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
#include <ctype.h>

#include <naf/nafmodule.h>
#include <naf/nafconfig.h>
#include <gnr/gnrmsg.h>
#include <gnr/gnrevents.h>


static struct nafmodule *timps_logging__module = NULL;
static char *timps_logging__adminlogfn = NULL;
static FILE *timps_logging__adminlogstream = NULL;

#define TLOGGING_ENABLEPERUSERLOGS_DEFAULT 0
static int timps_logging__enableuserlogs = TLOGGING_ENABLEPERUSERLOGS_DEFAULT;


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

static void tlogging__logmsg_peruser(struct nafmodule *mod, FILE *f, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{
	static const char *typenames[] = {
		"unknown", "message", "group-invite", "group-message",
		"group-join", "group-part", "rendezvous",
	};
	const char *typename = typenames[0];

	if (gm->type == GNR_MSG_MSGTYPE_IM) typename = typenames[1];
	else if (gm->type == GNR_MSG_MSGTYPE_GROUPINVITE) typename = typenames[2];
	else if (gm->type == GNR_MSG_MSGTYPE_GROUPIM) typename = typenames[3];
	else if (gm->type == GNR_MSG_MSGTYPE_GROUPJOIN) typename = typenames[4];
	else if (gm->type == GNR_MSG_MSGTYPE_GROUPPART) typename = typenames[5];
	else if (gm->type == GNR_MSG_MSGTYPE_RENDEZVOUS) typename = typenames[6];

	fprintf(f, "%s | %s | %s[%s] | %s[%s] | %s | %s\n",
			myctime(),

			typename,

			gmhi->srcnode ? gmhi->srcnode->name : gm->srcname,
			gmhi->srcnode ? gmhi->srcnode->service : gm->srcnameservice,

			gmhi->destnode ? gmhi->destnode->name : gm->destname,
			gmhi->destnode ? gmhi->destnode->service : gm->destnameservice,

			gm->msgtexttype ? gm->msgtexttype : "text/plain",
			gm->msgtext);
	fflush(f);

	return;
}

static int
tlogging_msglogger(struct nafmodule *mod, int stage, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{

	/* admin log */
	if (timps_logging__adminlogstream)
		tlogging__logmsg_admin(mod, gm, gmhi);

	/* per-user logs (both sides) */
	if (gmhi->srcnode) {
		FILE *peruser = NULL;

		gnr_node_tag_fetch(mod, gmhi->srcnode, "gnrnode.userlogstream", NULL, (void **)&peruser);
		if (peruser)
			tlogging__logmsg_peruser(mod, peruser, gm, gmhi);
	}
	if (gmhi->destnode) {
		FILE *peruser = NULL;

		gnr_node_tag_fetch(mod, gmhi->destnode, "gnrnode.userlogstream", NULL, (void **)&peruser);
		if (peruser)
			tlogging__logmsg_peruser(mod, peruser, gm, gmhi);
	}

	/* XXX per-session log */

	return 0;
}

static void tlogging__lognodeevent(struct nafmodule *mod, FILE *f, struct gnrnode *node, gnr_event_t event, naf_u32_t reason)
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

	fprintf(f, "%s  %s:  %s[%s][%s][%s%s%s] %s%s%s\n",
			myctime(),
			eventname,
			node->name, node->service,
			node->ownermod ? node->ownermod->name : "unknown",
			(node->metric == GNR_NODE_METRIC_LOCAL) ? "local" : "",
			GNR_NODE_METRIC_ISPEERED(node->metric) ? "peer" : "",
			(node->metric == GNR_NODE_METRIC_MAX) ? "remote" : "",
			rstr ? "(" : "", rstr ? rstr : "", rstr ? ")" : "");
	fflush(f);

	return;
}

static char *mkuserlogfn(struct nafmodule *mod, struct gnrnode *node)
{
	static const char prefix[] = {"timps-userlog."};
	char *fn, *nc;
	const char *path, *c;
	int fnlen;

	path = naf_config_getmodparmstr(mod, "logfilepath");
	fnlen = (path ? strlen(path) : 0) + 1 + strlen(prefix) + strlen(node->service) + 1 + strlen(node->name) + 1;
	if (!(fn = naf_malloc(mod, fnlen)))
		return NULL;
	snprintf(fn, fnlen, "%s/%s%s.", path ? path : "", prefix, node->service);
	nc = fn + strlen(fn);
	for (c = node->name; *c; c++) { /* remove spaces, make lowercase */
		if (*c != ' ')
			*nc = tolower(*c), nc++;
	}
	*nc = '\0';

	return fn;
}

static void
tlogging_nodeeventhandler(struct nafmodule *mod, struct gnr_event_info *gei)
{
	struct gnr_event_ei_nodechange *einc;
	FILE *peruser = NULL;

	einc = (struct gnr_event_ei_nodechange *)gei->gei_extinfo;

	gnr_node_tag_fetch(mod, gei->gei_node, "gnrnode.userlogstream", NULL, (void **)&peruser);

	/* only do logs for local users */
	if ((gei->gei_event == GNR_EVENT_NODEUP) && !peruser &&
			(gei->gei_node->metric == GNR_NODE_METRIC_LOCAL) &&
			timps_logging__enableuserlogs) {
		char *fn;

		fn = mkuserlogfn(mod, gei->gei_node);
		if (!fn || !(peruser = fopen(fn, "a")) ||
				(gnr_node_tag_add(mod, gei->gei_node, "gnrnode.userlogstream", 'V', (void *)peruser) == -1)) {
			dvprintf(mod, "unable to open log file '%s' for user '%s'\n", fn, gei->gei_node->name);
			if (peruser) {
				fclose(peruser);
				peruser = NULL;
			}
		}
		naf_free(mod, fn);
	}

	if (timps_logging__adminlogstream) {
		tlogging__lognodeevent(mod, timps_logging__adminlogstream,
				gei->gei_node, gei->gei_event,
				einc ? einc->reason : GNR_NODE_OFFLINE_REASON_UNKNOWN);
	}

	if (peruser) {
		tlogging__lognodeevent(mod, peruser, gei->gei_node,
				gei->gei_event, einc ? einc->reason : GNR_NODE_OFFLINE_REASON_UNKNOWN);
	}

	if ((gei->gei_event == GNR_EVENT_NODEDOWN) && peruser) {
		gnr_node_tag_remove(mod, gei->gei_node, "gnrnode.userlogstream", NULL, (void **)&peruser);
		fclose(peruser);
		peruser = NULL;
	}

	/* XXX per-session log */

	return;
}


static void
freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{

	if (strcmp(tagname, "gnrnode.userlogstream") == 0) {
		FILE *f = (FILE *)tagdata;

		/*
		 * This should never actually happen, since we remove the
		 * tag in the NODEDOWN event.
		 */
		fclose(f);

	} else {

		dvprintf(mod, "freetag: unknown tagname '%s'\n", tagname);
	}

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
		int i;

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

		/*
		 * XXX Currently, any changes made to the log file path or
		 * enabling/disabling per-user logs will not apply to users
		 * already online.  Their logs will stay open (closed)
		 * until they log out (log in) next.
		 */
		if ((i = naf_config_getmodparmbool(mod, "enableperuserlogs")) == -1)
			i = TLOGGING_ENABLEPERUSERLOGS_DEFAULT;
		timps_logging__enableuserlogs = i;

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
	mod->freetag = freetag;

	return 0;
}


int
timps_logging__register(void)
{
	return naf_module__registerresident("timps-logging", modfirst, NAF_MODULE_PRI_THIRDPASS);
}

