
#include <string.h>
#include <stdlib.h>

#include <naf/nafmodule.h>
#include <naf/nafconfig.h>

#include "core.h"
#include "msg.h"
#include "node.h"

#define GNR_DEBUG_DEFAULT 0
int gnr__debug = GNR_DEBUG_DEFAULT;
struct nafmodule *gnr__module = NULL;


struct evhandler {
	struct nafmodule *evh_mod;
	gnr_eventhandlerfunc_t evh_func;
	gnr_event_t evh_evmask;
	struct evhandler *evh__next;
};
static struct evhandler *gnr__evhlist = NULL;

static struct evhandler *evh__alloc(struct nafmodule *mod)
{
	struct evhandler *evh;

	if (!(evh = naf_malloc(mod, sizeof(struct evhandler))))
		return NULL;
	memset(evh, 0, sizeof(struct evhandler));

	return evh;
}

static void evh__free(struct nafmodule *mod, struct evhandler *evh)
{

	naf_free(mod, evh);

	return;
}

static void evh__freeall(struct nafmodule *mod, struct evhandler *evh)
{

	while (evh) {
		struct evhandler *tmp;

		tmp = evh->evh__next;
		evh__free(mod, evh);
		evh = tmp;
	}

	return;
}

static struct evhandler *evh_find(struct nafmodule *ownermod, gnr_eventhandlerfunc_t func)
{
	struct evhandler *evh;

	for (evh = gnr__evhlist; evh; evh = evh->evh__next) {
		if ((evh->evh_mod == ownermod) &&
				(evh->evh_func == func)) 
			return evh;
	}

	return NULL;
}

int gnr_event_register(struct nafmodule *mod, gnr_eventhandlerfunc_t evfunc, gnr_event_t evmask)
{
	struct evhandler *evh;

	if (!mod || !evfunc)
		return -1;
	if (evh_find(mod, evfunc))
		return -1;

	if (!(evh = evh__alloc(gnr__module)))
		return -1;
	evh->evh_mod = mod;
	evh->evh_func = evfunc;
	evh->evh_evmask = evmask;

	evh->evh__next = gnr__evhlist;
	gnr__evhlist = evh;

	return 0;
}

int gnr_event_unregister(struct nafmodule *mod, gnr_eventhandlerfunc_t func)
{
	struct evhandler *cur, **prev;

	if (!mod || !func)
		return -1;

	for (prev = &gnr__evhlist; (cur = *prev); ) {
		if ((cur->evh_mod == mod) &&
				(cur->evh_func == func)) {
			*prev = cur->evh__next;
			evh__free(gnr__module, cur);
		} else
			prev = &cur->evh__next;
	}

	return 0;
}

int gnr_event_throw(struct gnr_event_info *gei)
{
	struct evhandler *evh;

	if (!gei)
		return -1;

	for (evh = gnr__evhlist; evh; evh = evh->evh__next) {
		if (evh->evh_evmask & gei->gei_event)
			evh->evh_func(evh->evh_mod, gei);
	}

	return 0;
}

static int modinit(struct nafmodule *mod)
{

	gnr_msg__register(mod); /* must be first */
	gnr_node__register(mod);

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	gnr_node__unregister(mod);
	gnr_msg__unregister(mod); /* must be last */

	evh__freeall(mod, gnr__evhlist);

	gnr__module = NULL;

	return 0;
}

static void timerhandler(struct nafmodule *mod)
{

	gnr_node__timeout(mod);

	return;
}

static void freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{

	if (strcmp(tagname, "module.gnrmsg_outputfunc") == 0) {

		/* pointer to non-dynamic object */

	} else {

		dvprintf(mod, "freetag: unknown tag '%s'\n", tagname);

	}

	return;
}

static void signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {
		char *debugstr;

		if ((debugstr = naf_config_getmodparmstr(mod, "debug")))
			gnr__debug = atoi(debugstr);
		if (gnr__debug == -1)
			gnr__debug = GNR_DEBUG_DEFAULT;
	}

	return;
}

static int modfirst(struct nafmodule *mod)
{

	gnr__module = mod;

	naf_module_setname(mod, "gnr");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->signal = signalhandler;
	mod->freetag = freetag;
	mod->timerfreq = 15;
	mod->timer = timerhandler;

	return 0;
}

int gnr_core_register(void)
{
	return naf_module__registerresident("gnr", modfirst, NAF_MODULE_PRI_SECONDPASS);
}


