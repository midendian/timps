/*
 * naf - Networked Application Framework
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * naf is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * naf is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * Sort of a dummy module to handle the hooks for things that don't
 * easily fit elsewhere -- mostly RPC stuff.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* for exit() */
#endif

#include <naf/nafmodule.h>
#include <naf/nafrpc.h>
#include <naf/nafconfig.h>

#include "core.h"
#include "memory.h"

#include "module.h" /* for naf_module__registerresident() */


#define NAF_MEMORY_DEBUG_DEFAULT 1
int naf_memory__debug = NAF_MEMORY_DEBUG_DEFAULT; /* imported by memory.c */

static struct nafmodule *naf_core__module = NULL;


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

			if ((pmod = naf_module_findbyname(naf_core__module, module->data.string)))
				statusiter(naf_core__module, pmod, (void *)modules);

		} else
			naf_module_iter(naf_core__module, statusiter, (void *)modules);
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

	naf_core__module = mod;

	naf_rpc_register_method(mod, "modstatus", __rpc_core_modstatus, "Get module list and status");
	naf_rpc_register_method(mod, "modmemoryuse", __rpc_core_modmemoryuse, "Get module memory usage");
	naf_rpc_register_method(mod, "shutdown", __rpc_core_shutdown, "Shut down");
	naf_rpc_register_method(mod, "listmodules", __rpc_core_listmodules, "Get modules list");

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	naf_core__module = NULL;

	return 0;
}

static void signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {
		NAFCONFIG_UPDATEINTMODPARMDEF(mod, "debug",
					      naf_memory__debug,
					      NAF_MEMORY_DEBUG_DEFAULT);
	}

	return;
}

static int modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "core");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->signal = signalhandler;

	return 0;
}

int naf_core__register(void)
{
	return naf_module__registerresident("core", modfirst, NAF_MODULE_PRI_THIRDPASS);
}

