
/*
 * Sort of a dummy module to handle the hooks for things that don't
 * easily fit elsewhere -- mostly RPC stuff.
 */

#include <config.h>

#include <string.h>
#include <stdlib.h> /* for exit() */

#include <naf/nafmodule.h>
#include <naf/nafrpc.h>

#include "core.h"
#include "memory.h"

#include "module.h" /* for naf_module__registerresident() */


#define MEMORY_DEBUG_DEFAULT 0
int naf_memory_debug = MEMORY_DEBUG_DEFAULT; /* imported by memory.c */

static struct nafmodule *ourmodule = NULL;


static int statusiter(struct nafmodule *mod, struct nafmodule *curmod, void *udata)
{
	naf_rpc_arg_t **top = (naf_rpc_arg_t **)udata;
	naf_rpc_arg_t **ptop;

	if ((ptop = naf_rpc_addarg_array(mod, top, curmod->name)) && strlen(curmod->statusline))
		naf_rpc_addarg_string(mod, ptop, "status", curmod->statusline);

	return 0;
}

/*
 * core->modstatus()
 *   IN:
 *      [optional] string module;
 *
 *   OUT:
 *      [optional] array modules {
 *          array modulename {
 *              string status;
 *          }
 *      }
 */
static void __rpc_core_modstatus(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t *module, **modules;

	if ((module = naf_rpc_getarg(req->inargs, "module"))) {
		if (module->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	if ((modules = naf_rpc_addarg_array(mod, &req->returnargs, "modules"))) {

		if (module) {
			struct nafmodule *pmod;

			if ((pmod = naf_module_findbyname(ourmodule, module->data.string)))
				statusiter(ourmodule, pmod, (void *)modules);

		} else
			naf_module_iter(ourmodule, statusiter, (void *)modules);
	}

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}

/*
 * core->shutdown()
 *   IN:
 *
 *   OUT:
 *
 */
static void __rpc_core_shutdown(struct nafmodule *mod, naf_rpc_req_t *req)
{

	req->status = NAF_RPC_STATUS_SUCCESS;

	nafsignal(NULL, NAF_SIGNAL_SHUTDOWN);
	naf_module__unloadall();

	exit(2);

	return;
}

static int modinit(struct nafmodule *mod)
{

	ourmodule = mod;

	naf_rpc_register_method(mod, "modstatus", __rpc_core_modstatus, "Get module list and status");
	naf_rpc_register_method(mod, "modmemoryuse", __rpc_core_modmemoryuse, "Get module memory usage");
	naf_rpc_register_method(mod, "shutdown", __rpc_core_shutdown, "Shut down");
	naf_rpc_register_method(mod, "listmodules", __rpc_core_listmodules, "Get modules list");

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	ourmodule = NULL;

	return 0;
}

static int modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "core");
	mod->init = modinit;
	mod->shutdown = modshutdown;

	return 0;
}

int naf_core__register(void)
{
	return naf_module__registerresident("core", modfirst, NAF_MODULE_PRI_THIRDPASS);
}

