
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <naf/naf.h>
#include <naf/nafconfig.h>
#include <naf/nafmodule.h>
#include <gnr/gnr.h>
#include <gnr/gnrmsg.h>

#include "oscar/oscar.h"
#include "logging.h"


#define TIMPS_DEBUG_DEFAULT 0
int timps__debug = TIMPS_DEBUG_DEFAULT;
struct nafmodule *timps__module = NULL;


static void dumpbox(struct nafmodule *mod, const char *prefix, naf_conn_cid_t cid, unsigned char *buf, int len)
{
	int z = 0, x, y;
	char tmpstr[256];

	while (z<len) {
		x = snprintf(tmpstr, sizeof(tmpstr), "%sput, %d bytes to cid %ld:      ", prefix, len, cid);
		for (y = 0; y < 8; y++) {
			if (z<len) {
				snprintf(tmpstr+x, sizeof(tmpstr)-strlen(tmpstr), "%02x ", buf[z]);
				z++;
				x += 3;
			} else
				break;
		}
		dvprintf(mod, "%s\n", tmpstr);
	}
}


static int
timps_msgrouting(struct nafmodule *mod, int stage, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{
	/* XXX security stuff here */
	return 0;
}

static void
freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{

	dvprintf(mod, "freetag: unknown tagname '%s'\n", tagname);

	return;
}

static int
modinit(struct nafmodule *mod)
{

	timps__module = mod;

	if (gnr_msg_register(mod, NULL /* no outputfunc */) == -1) {
		dprintf(mod, "modinit: gsr_msg_register failed\n");
		return -1;
	}
	gnr_msg_addmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_ROUTING, 75, timps_msgrouting, "Implement auditing rules.");


	return 0;
}

static int
modshutdown(struct nafmodule *mod)
{

	gnr_msg_remmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_ROUTING, timps_msgrouting);
	gnr_msg_unregister(mod);

	timps__module = NULL;

	return 0;
}

static void
signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {
		char *debugstr;

		if ((debugstr = naf_config_getmodparmstr(mod, "debug")))
			timps__debug = atoi(debugstr);
		if (timps__debug == -1)
			timps__debug = TIMPS_DEBUG_DEFAULT;
	}

	return;
}

static int
modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "timps");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->freetag = freetag;
	mod->signal = signalhandler;

	return 0;
}

int
main(int argc, char **argv) 
{

	if (naf_init0("timpsd", "0.10",
				"Transparent Instant Messaging Proxy Server", 
				"(c)2003,2004,2005 Adam Fritzler (mid@zigamorph.net)") == -1)
		return -1;

	/* get the gnr core going */
	if (gnr_core_register() == -1)
		return -1;
	/* register all the timps support modules */
	timps_oscar__register();
	timps_logging__register();
	/* timps core */
	naf_module__registerresident("timps", modfirst, NAF_MODULE_PRI_THIRDPASS);

	/* do this thing. */
	if (naf_init1(argc, argv) == -1)
		return -1;

	if (naf_init_final() == -1)
		return -1;

	return naf_main();
}

