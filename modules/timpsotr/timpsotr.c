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

/*
 * timps-otr provides support for the Off-The-Record messaging protocol
 * described here:  http://www.cypherpunks.ca/otr/
 *
 * You'll need to go there anyway to get libotr, which you'll need to make
 * this compile.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <naf/nafmodule.h>
#include <naf/nafconfig.h>
#include <gnr/gnrmsg.h>
#include <gnr/gnrnode.h>
#include <gnr/gnrevents.h>

#include <gcrypt.h>
#include <libotr/privkey.h>
#include <libotr/proto.h>
#include <libotr/message.h>

#define TOTR_PRIVATEKEYFN "otr.private_key"
#define TOTR_FINGERPRINTSFN "otr.fingerprints"

static struct nafmodule *timps_otr__module = NULL;
#define TOTR_DEBUG_DEFAULT 0
static int timps_otr__debug = TOTR_DEBUG_DEFAULT;
static char *timps_otr__keyfilepath = NULL;
#define TOTR_BOTNAME_DEFAULT "$TOTRHelper"
static struct gnrnode *timps_otr__botnode = NULL;

static void totr_uiop__create_privkey(void *opdata, const char *accountname, const char *protocol);
static void totr_uiop__inject_message(void *opdata, const char *accountname, const char *protocol, const char *recipient, const char *message);
static void totr_uiop__notify(void *opdata, OtrlNotifyLevel level, const char *title, const char *primary, const char *secondary);
static void totr_uiop__update_context_list(void *opdata);
static const char *totr_uiop__protocol_name(void *opdata, const char *protocol);
static void totr_uiop__protocol_name_free(void *opdata, const char *protocol_name);
static void totr_uiop__confirm_fingerprint(void *opdata, const char *username, const char *protocol, OTRKeyExchangeMsg kem, void (*response_cb)(struct s_OtrlMessageAppOps *ops, void *opdata, OTRConfirmResponse *response_data, int resp), OTRConfirmResponse *response_data);
static void totr_uiop__write_fingerprints(void *opdata);
static void totr_uiop__gone_secure(void *opdata, ConnContext *context);
static void totr_uiop__gone_insecure(void *opdata, ConnContext *context);
static void totr_uiop__still_secure(void *opdata, ConnContext *context, int is_reply);
static void totr_uiop__log_message(void *opdata, const char *message);   
static OtrlMessageAppOps timps_otr__ui_ops = {
	totr_uiop__create_privkey,
	totr_uiop__inject_message,
	totr_uiop__notify,
	totr_uiop__update_context_list,
	totr_uiop__protocol_name,
	totr_uiop__protocol_name_free,
	totr_uiop__confirm_fingerprint,
	totr_uiop__write_fingerprints,
	totr_uiop__gone_secure,
	totr_uiop__gone_insecure,
	totr_uiop__still_secure,
	totr_uiop__log_message,
};

#define TOTR_SENDMSGBUF_LEN 2048
static int
totr_vsendmsg(struct nafmodule *mod, char *srcname, char *srcservice, char *destname, char *destservice, char *format, va_list ap)
{
	struct gnrmsg *gm;
	char *buf;

	if (!srcname || !srcservice || !destname || !destservice)
		return -1;

	if (!(buf = naf_malloc(mod, TOTR_SENDMSGBUF_LEN)))
		return -1;

	if (!(gm = gnr_msg_new(mod))) {
		naf_free(mod, buf);
		return -1;
	}

	gm->type = GNR_MSG_MSGTYPE_IM;
	gm->srcname = srcname; gm->srcnameservice = srcservice;
	gm->destname = destname; gm->destnameservice = destservice;

	vsnprintf(buf, TOTR_SENDMSGBUF_LEN, format, ap);
	gm->msgtexttype = "text/html"; gm->msgtext = buf;

	gnr_msg_route(mod, gm);

	gnr_msg_free(mod, gm);
	return 0;
}

static int
totrbot_sendmsg(struct nafmodule *mod, char *destname, char *destservice, char *format, ...)
{
	int ret;
	va_list ap;

	if (!timps_otr__botnode || !destname || !destservice)
		return -1;

	/* Don't need error handling since varargs just blows up on error... */
	va_start(ap, format);
	ret = totr_vsendmsg(mod, timps_otr__botnode->name, timps_otr__botnode->service, destname, destservice, format, ap);
	va_end(ap);

	return ret;
}

static int
totr_sendmsg(struct nafmodule *mod, char *srcname, char *srcservice, char *destname, char *destservice, char *format, ...)
{
	int ret;
	va_list ap;

	if (!srcname || !srcservice || !destname || !destservice)
		return -1;

	va_start(ap, format);
	ret = totr_vsendmsg(mod, srcname, srcservice, destname, destservice, format, ap);
	va_end(ap);

	return ret;
}

static char *
totr_mknormalisedsn(struct nafmodule *mod, const char *isn)
{
	char *osn, *d;
	const char *c;

	/* it'll never get longer... */
	if (!(osn = naf_malloc(mod, strlen(isn)+1)))
		return NULL;

	for (c = isn, d = osn; *c; c++) {
		if (*c != ' ')
			*(d++) = tolower(*c);
	}
	*d = '\0';

	return osn;
}

static char *
totr_mkfilename(struct nafmodule *mod, const char *fn)
{
	int nfnlen;
	char *nfn;

	if (!fn)
		return NULL;

	nfnlen = (timps_otr__keyfilepath ? strlen(timps_otr__keyfilepath) : 0) + 1 + strlen(fn) + 1;
	if (!(nfn = naf_malloc(mod, nfnlen)))
		return NULL;
	snprintf(nfn, nfnlen, "%s/%s",
			timps_otr__keyfilepath ? timps_otr__keyfilepath : "",
			fn);

	return nfn;
}


static void
totr_uiop__create_privkey(void *opdata, const char *accountname, const char *protocol)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;
	char *pkfn;
	char *fingerprint;

	if (timps_otr__debug > 0)
		dvprintf(mod, "creating private key for %s[%s]\n", accountname, protocol);

	if (!(pkfn = totr_mkfilename(mod, TOTR_PRIVATEKEYFN)))
		return;

	otrl_privkey_generate(pkfn, accountname, protocol);
	fingerprint = otrl_privkey_fingerprint(accountname, protocol);

	if (fingerprint) {
		totrbot_sendmsg(mod, (char *)accountname, (char *)protocol,
				"Generated private key for %s[%s]: %.80s",
				accountname, protocol, fingerprint);
	}

	naf_free(mod, pkfn);

	return;
}

/* XXX when does this happen? I think all the cases are covered elsewhere */
static void
totr_uiop__inject_message(void *opdata, const char *accountname, const char *protocol, const char *recipient, const char *message)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;
	struct gnrmsg *gm;

	if (!accountname || !protocol || !recipient)
		return;

	if (!(gm = gnr_msg_new(mod)))
		return;

	gm->type = GNR_MSG_MSGTYPE_IM;
	gm->srcname = (char *)accountname;
	gm->srcnameservice = (char *)protocol;
	gm->destname = (char *)recipient;
	gm->destnameservice = (char *)protocol;
	gm->msgtexttype = "text/html";
	gm->msgtext = (char *)message;

	gnr_msg_route(mod, gm);

	gnr_msg_free(mod, gm);
	return;
}

static void
totr_uiop__notify(void *opdata, OtrlNotifyLevel level, const char *title, const char *primary, const char *secondary)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;

#if 0
	/*
	 * XXX If we had user-specific contexts in libotr, we could do this
	 * all nice like, but since we have no idea who to send these to,
	 * we got nothin.
	 */
	totrbot_sendmsg(mod, accountname, protocol, "<b>%s%s%s</b>: %s<br/>%s",
			(level == OTRL_NOTIFY_ERROR) ? "ERROR" : "",
			(level == OTRL_NOTIFY_WARNING) ? "WARNING" : "",
			(level == OTRL_NOTIFY_INFO) ? "Note" : "",
			primary ? primary : "",
			secondary ? secondary : "");
#else
	dvprintf(mod, "notification from libotr: %s%s%s: %s (%s)\n",
			(level == OTRL_NOTIFY_ERROR) ? "ERROR" : "",
			(level == OTRL_NOTIFY_WARNING) ? "WARNING" : "",
			(level == OTRL_NOTIFY_INFO) ? "Note" : "",
			primary ? primary : "",
			secondary ? secondary : "");
#endif

}

static void
totr_uiop__update_context_list(void *opdata)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;

	/*
	 * XXX We could probably do something here in order to make the 
	 * routing handler not get called so much (ie, only call it for
	 * connections we know we have contexts for).
	 */
	return;
}

static const char *
totr_uiop__protocol_name(void *opdata, const char *protocol)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;

	return naf_strdup(mod, protocol);
}

/* XXX hey guys, free() functions can't take a const... */
static void
totr_uiop__protocol_name_free(void *opdata, const char *protocol_name)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;

	naf_free(mod, (char *)protocol_name);

	return;
}

static void
totr_uiop__confirm_fingerprint(void *opdata, const char *username, const char *protocol, OTRKeyExchangeMsg kem, void (*response_cb)(struct s_OtrlMessageAppOps *ops, void *opdata, OTRConfirmResponse *response_data, int resp), OTRConfirmResponse *response_data)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;
	char fingerprint[45];

	otrl_privkey_hash_to_human(fingerprint, kem->key_fingerprint);

	if (timps_otr__debug > 0) {
		dvprintf(mod, "received fingerprint for %s[%s]: %s\n",
					username, protocol, fingerprint);
	}

	/* XXX XXX XXX do a dialog via TOTRHelper here */

	/* just accept it */
	response_cb(&timps_otr__ui_ops, mod, response_data, 1 /* accept */);

	return;
}

static void
totr_uiop__write_fingerprints(void *opdata)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;
	char *fpfn;

	if (!(fpfn = totr_mkfilename(mod, TOTR_FINGERPRINTSFN)))
		return;

	otrl_privkey_write_fingerprints(fpfn);

	naf_free(mod, fpfn);

	return;
}

static void
totr_uiop__gone_secure(void *opdata, ConnContext *context)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;
	char fingerprint[45];
	unsigned char *sessionid;
	char sess1[21], sess2[21];
	SessionDirection dir;

	/* (mostly copied from gaim-otr) */

	dir = context->sesskeys[1][0].dir;
	otrl_privkey_hash_to_human(fingerprint, context->active_fingerprint->fingerprint);
	sessionid = context->sesskeys[1][0].sessionid;
	{ /* make human-readable version of the sessionid (in two parts) */
		int i;
		for (i = 0; i < 10; i++)
			sprintf(sess1 + (i * 2), "%02x", sessionid[i]);
		sess1[20] = '\0';
		for (i = 0; i < 10; i++)
			sprintf(sess2 + (i * 2), "%02x", sessionid[i+10]);
		sess2[20] = '\0';
	}

	totr_sendmsg(mod, context->username, context->protocol,
			context->accountname, context->protocol,
			"<br><br><hr>Private connection established with %s[%s].<br>Fingerprint for %s[%s]: %s<br>Secure ID for this session: %s%s%s%s%s%s<br><hr><br>",
			context->username, context->protocol,
			context->username, context->protocol,
			fingerprint,
			(dir == SESS_DIR_LOW) ? "<b>" : "",
			sess1,
			(dir == SESS_DIR_LOW) ? "</b>" : "",
			(dir == SESS_DIR_HIGH) ? "<b>" : "",
			sess2,
			(dir == SESS_DIR_HIGH) ? "</b>" : "");

	return;
}

static void
totr_uiop__gone_insecure(void *opdata, ConnContext *context)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;

	totr_sendmsg(mod, context->username, context->protocol,
			context->accountname, context->protocol,
			"<br><br><hr>Private connection with %s[%s] <b>has been lost.</b><br><hr><br>",
			context->username, context->protocol);

	return;
}

static void
totr_uiop__still_secure(void *opdata, ConnContext *context, int is_reply)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;

	totr_sendmsg(mod, context->username, context->protocol,
			context->accountname, context->protocol,
			"<br><br><hr>Private connection with %s[%s] has been successfully refreshed.</b><br><hr><br>",
			context->username, context->protocol);

	return;
}

static void
totr_uiop__log_message(void *opdata, const char *message)
{
	struct nafmodule *mod = (struct nafmodule *)opdata;

	/* XXX Just like the notify handler, it would help if we knew what
	 * user's instance of libotr generated this, so we could send it
	 * directly to them.
	 */
	/* Comes with \n on the end */
	dvprintf(mod, "message from libotr: %s", message);

	return;
}


static int
totr_msgroutinghandler(struct nafmodule *mod, int stage, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{

	if (gm->type != GNR_MSG_MSGTYPE_IM)
		return 0;

	/*
	 * XXX This works fine now, but sometimes srcmod isn't available,
	 * like in the case of messages coming across peers.  Need a better
	 * way to detect that a message has already been here.
	 */
	if (gmhi->srcmod == mod)
		return 0;

	if (timps_otr__botnode && (gmhi->destnode == timps_otr__botnode)) {
		gm->routeflags |= GNR_MSG_ROUTEFLAG_ROUTED_LOCAL;
		return 1;
	}

	/*
	 * To us, "receiving" and "sending" don't have a lot of meaning, since
	 * we sit in the path of a potentially very varied set of circumstances.
	 *
	 * But to a client (like libotr was designed to be), "sending" means a
	 * message with a local source and a remote destination, and "receiving"
	 * means a message with a remote source and a local destination.
	 *
	 * However, since in timps we can locally route some messages -- which
	 * will result in messages with both a local source and a local
	 * destination -- we only use the "local" side of the definitions above.
	 *
	 * XXX make more assurances that otrl doesn't get called superfluously.
	 * XXX make sure the case works where the timps-otr module is active,
	 * but a client is actually using their own OTR.  I'm guessing that
	 * makes this very confused.
	 */
	if (gmhi->srcnode->metric == GNR_NODE_METRIC_LOCAL) { /* sending */
		char *src, *dest, *newmsgtext = NULL, *newmsgtext2 = NULL;
		gcry_error_t err;

		if (!gm->msgtext || strstr(gm->msgtext, OTR_MESSAGE_TAG))
			return 0;

		src = totr_mknormalisedsn(mod, gmhi->srcnode->name);
		dest = totr_mknormalisedsn(mod, gmhi->destnode->name);
		if (!src || !dest)
			return 0;

		err = otrl_message_sending(&timps_otr__ui_ops, mod,
				src, gmhi->srcnode->service,
				dest, gm->msgtext,
				&newmsgtext);

		if (err && !newmsgtext) {
			newmsgtext2 = naf_strdup(mod, "[encryption failed; previous message not sent]");
			/* 
			 * XXX have TOTRHelper send a "It looks like you're
			 * having trouble talking to [dest], do you want to end
			 * the OTR session" dialog
			 */
		} else if (newmsgtext)
			newmsgtext2 = naf_strdup(mod, newmsgtext);
		else
			; /* non-encrypted cases */

		if (newmsgtext)
			otrl_message_free(newmsgtext);
		naf_free(mod, src);
		naf_free(mod, dest);
		if (!newmsgtext2)
			return 0; /* do nothing */

		if (gnr_msg_tag_add(mod, gm, "gnrmsg.newotrmsgtext", 'S', (void *)newmsgtext2) == -1)
			naf_free(mod, newmsgtext2);

		gm->routeflags |= GNR_MSG_ROUTEFLAG_ROUTED_LOCAL;
		return 1; /* take it */

	} else if (gmhi->destnode->metric == GNR_NODE_METRIC_LOCAL) { /* receiving */
		char *src, *dest, *newmsgtext = NULL, *newmsgtext2 = NULL;
		gcry_error_t err;

		src = totr_mknormalisedsn(mod, gmhi->srcnode->name);
		dest = totr_mknormalisedsn(mod, gmhi->destnode->name);
		if (!src || !dest)
			return 0;

		err = otrl_message_receiving(&timps_otr__ui_ops, mod,
				dest, gmhi->destnode->service,
				src, gm->msgtext,
				&newmsgtext, NULL, NULL);

		if (newmsgtext) {
			newmsgtext2 = naf_strdup(mod, newmsgtext);
			otrl_message_free(newmsgtext);
		}
		naf_free(mod, src);
		naf_free(mod, dest);
		if (!newmsgtext2)
			return 0; /* do nothing */

		if (gnr_msg_tag_add(mod, gm, "gnrmsg.newotrmsgtext", 'S', (void *)newmsgtext2) == -1)
			naf_free(mod, newmsgtext2);

		gm->routeflags |= GNR_MSG_ROUTEFLAG_ROUTED_LOCAL;
		return 1; /* take it */
	}

	return 0;
}

static int
totr_sendnewmsg(struct nafmodule *mod, struct gnrnode *src, struct gnrnode *dest, char *msgtexttype, char *msgtext)
{
	struct gnrmsg *gm;

	if (!src || !dest)
		return -1;

	if (!(gm = gnr_msg_new(mod)))
		return -1;

	gm->type = GNR_MSG_MSGTYPE_IM;
	gm->srcname = src->name;
	gm->srcnameservice = src->service;
	gm->destname = dest->name;
	gm->destnameservice = dest->service;
	gm->msgtexttype = msgtexttype;
	gm->msgtext = msgtext;

	gnr_msg_route(mod, gm);

	gnr_msg_free(mod, gm);
	return 0;
}

static int
totr_gnroutputfunc(struct nafmodule *mod, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{

	if (timps_otr__botnode && (gmhi->destnode == timps_otr__botnode)) {

		/*
		 * XXX parse commands; see if part of open dialog (don't
		 * forget to strip html!)
		 *
		 * Need commands for:
		 *   - looking at active sessions
		 *   - examining fingerprint store
		 *   - forcing an OTR session closed
		 *   - forcing an OTR key refresh
		 * Also need dialog for things like accepting keys.
		 */
		totrbot_sendmsg(mod, gmhi->srcnode->name, gmhi->srcnode->service, "<b>Hello!</b>");

	} else {
		char *newmsgtext = NULL;

		gnr_msg_tag_remove(mod, gm, "gnrmsg.newotrmsgtext", NULL, (void **)&newmsgtext);
		if (!newmsgtext)
			return -1;

		/* XXX should try to move over old tags so things like buddy icon negotiation can still work */
		totr_sendnewmsg(mod, gmhi->srcnode, gmhi->destnode, gm->msgtexttype, newmsgtext);

		naf_free(mod, newmsgtext);
	}

	return 0;
}

static void
totr_nodeeventhandler(struct nafmodule *mod, struct gnr_event_info *gei)
{
	struct gnr_event_ei_nodechange *einc;

	einc = (struct gnr_event_ei_nodechange *)gei->gei_extinfo;


	if (timps_otr__botnode && (gei->gei_node == timps_otr__botnode)) {

		if (gei->gei_event == GNR_EVENT_NODEDOWN)
			timps_otr__botnode = NULL;

		return;
	}

	return;
}

static void
freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{

	if (strcmp(tagname, "gnrmsg.newotrmsgtext") == 0)
		naf_free(mod, (char *)tagdata);
	else
		dvprintf(mod, "freetag: unknown tagname '%s'\n", tagname);

	return;
}

static int
modinit(struct nafmodule *mod)
{

	timps_otr__module = mod;

	if (gnr_msg_register(mod, totr_gnroutputfunc) == -1) {
		dprintf(mod, "modinit: gsr_msg_register failed\n");
		return -1;
	}
	gnr_msg_addmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_ROUTING, 30, totr_msgroutinghandler, "Off-the-record messaging");

	gnr_event_register(mod, totr_nodeeventhandler, GNR_EVENTMASK_NODE);

	timps_otr__botnode = gnr_node_online(mod, TOTR_BOTNAME_DEFAULT, "AIM",
				GNR_NODE_FLAG_LOCALONLY, GNR_NODE_METRIC_LOCAL);

	/* XXX get libotr/gcry to allow us to use naf_malloc/free (not hard) */
	OTRL_INIT;

	return 0;
}

static int
modshutdown(struct nafmodule *mod)
{

	otrl_context_forget_all();
	otrl_privkey_forget_all();
	/* XXX why is there no OTRL_UNINIT? looks like there should be... */

	if (timps_otr__botnode)
		gnr_node_offline(timps_otr__botnode, GNR_NODE_OFFLINE_REASON_DISCONNECTED);

	gnr_event_unregister(mod, totr_nodeeventhandler);
	gnr_msg_remmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_ROUTING, totr_msgroutinghandler);
	gnr_msg_unregister(mod);

	timps_otr__module = NULL;

	return 0;
}

static void
signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {
		char *str;

		naf_free(mod, timps_otr__keyfilepath);
		str = naf_config_getmodparmstr(mod, "keyfilepath");
		timps_otr__keyfilepath = str ? naf_strdup(mod, str) : NULL;
		{
			char *pkfn, *fpfn;

			/* clear out old stuff... */
			otrl_context_forget_all();
			otrl_privkey_forget_all();

			/*
			 * XXX  I'd really prefer libotr to have per-user
			 * contexts (ie, no internal globals).  Mingling
			 * potentially totally unrelated users'
			 * keys/fingerprints is VERY BAD.
			 *
			 * When someone makes that so: key files should be
			 * loaded in the NODEUP for local users, and freed
			 * on NODEDOWN.
			 */

			pkfn = totr_mkfilename(mod, TOTR_PRIVATEKEYFN);
			fpfn = totr_mkfilename(mod, TOTR_FINGERPRINTSFN);
			if (pkfn && fpfn) {
				otrl_privkey_read(pkfn);
				otrl_privkey_read_fingerprints(fpfn, NULL, NULL);
			}
			naf_free(mod, pkfn);
			naf_free(mod, fpfn);
		}

		str = naf_config_getmodparmstr(mod, "debug");
		timps_otr__debug = str ? atoi(str) : -1;
		if (timps_otr__debug == -1)
			timps_otr__debug = TOTR_DEBUG_DEFAULT;
	}

	return;
}

int nafmodulemain(struct nafmodule *mod)
{

	naf_module_setname(mod, "timps-otr");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->signal = signalhandler;
	mod->freetag = freetag;

	return 0;
}

