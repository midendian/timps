
/*
 * Most useless naf module evar.
 */

#include <naf/nafmodule.h>

static struct nafmodule *ourmodule = NULL;

static int modinit(struct nafmodule *mod)
{

	dprintf(mod, "module initializing!\n");

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	dprintf(mod, "module shutting down!\n");

	return 0;
}

int nafmodulemain(struct nafmodule *mod)
{

	naf_module_setname(mod, "nafbasicmodule");
	mod->init = modinit;
	mod->shutdown = modshutdown;

	return 0;
}

