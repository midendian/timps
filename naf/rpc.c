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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <naf/nafmodule.h>
#include <naf/nafrpc.h>

#include "module.h" /* for naf_module__registerresident() */

static struct nafmodule *ourmodule = NULL;

static void naf_rpc_arg_free(struct nafmodule *mod, naf_rpc_arg_t *arg);
static void naf_rpc_arg_freelist(struct nafmodule *mod, naf_rpc_arg_t *arg);

static naf_rpc_arg_t *naf_rpc_arg_new(struct nafmodule *mod, const char *name)
{
	naf_rpc_arg_t *arg;

	if (!(arg = naf_malloc(mod, sizeof(naf_rpc_arg_t))))
		return NULL;
	memset(arg, 0, sizeof(naf_rpc_arg_t));

	if (!(arg->name = naf_strdup(mod, name))) {
		naf_free(mod, arg);
		return NULL;
	}

	return arg;
}

static void naf_rpc_arg_free(struct nafmodule *mod, naf_rpc_arg_t *arg)
{

	naf_free(mod, arg->name);

	if (arg->type == NAF_RPC_ARGTYPE_ARRAY)
		naf_rpc_arg_freelist(mod, arg->data.children);
	else if (arg->type == NAF_RPC_ARGTYPE_GENERIC)
		naf_free(mod, arg->data.generic);
	else if (arg->type == NAF_RPC_ARGTYPE_STRING)
		naf_free(mod, arg->data.string);

	naf_free(mod, arg);

	return;
}

static void naf_rpc_arg_freelist(struct nafmodule *mod, naf_rpc_arg_t *arg)
{

	while (arg) {
		naf_rpc_arg_t *tmp;

		tmp = arg->next;
		naf_rpc_arg_free(mod, arg);
		arg = tmp;
	}

	return;
}

void naf_rpc_request_free(struct nafmodule *mod, naf_rpc_req_t *req)
{

	naf_rpc_arg_freelist(mod, req->inargs);
	naf_rpc_arg_freelist(mod, req->returnargs);

	naf_free(mod, req->target);
	naf_free(mod, req->method);
	naf_free(mod, req);

	return;
}

naf_rpc_req_t *naf_rpc_request_new(struct nafmodule *mod, const char *targetmod, const char *method)
{
	naf_rpc_req_t *req;

	if (!(req = naf_malloc(mod, sizeof(naf_rpc_req_t))))
		return NULL;
	memset(req, 0, sizeof(naf_rpc_req_t));

	if (!(req->target = naf_strdup(mod, targetmod))) {
		naf_rpc_request_free(mod, req);
		return NULL;
	}

	if (!(req->method = naf_strdup(mod, method))) {
		naf_rpc_request_free(mod, req);
		return NULL;
	}

	req->status = NAF_RPC_STATUS_PENDING;

	return req;
}


struct rpcmethod {
	char *name;
	naf_rpc_method_t func;
	char *desc;
	struct rpcmethod *next;
};

struct rpcmoduleinfo {
	struct rpcmethod *methods;
};

static void freemethod(struct rpcmethod *rm)
{

	naf_free(ourmodule, rm->name);
	naf_free(ourmodule, rm->desc);
	naf_free(ourmodule, rm);

	return;
}

static void freeinfo(struct rpcmoduleinfo *rpi)
{
	struct rpcmethod *rm;

	for (rm = rpi->methods; rm; ) {
		struct rpcmethod *tmp;

		tmp = rm->next;
		freemethod(rm);
		rm = tmp;
	}

	naf_free(ourmodule, rpi);

	return;
}

static struct rpcmoduleinfo *fetchinfo(struct nafmodule *mod)
{
	struct rpcmoduleinfo *rpi = NULL;

	if ((naf_module_tag_fetch(ourmodule, mod, "module.rpi", NULL, (void **)&rpi) == -1) || !rpi) {

		if (!(rpi = naf_malloc(ourmodule, sizeof(struct rpcmoduleinfo))))
			return NULL;
		memset(rpi, 0, sizeof(struct rpcmoduleinfo));

		if (naf_module_tag_add(ourmodule, mod, "module.rpi", 'V', (void *)rpi) == -1) {
			naf_free(ourmodule, rpi);
			return NULL;
		}
	}

	return rpi;
}

static struct rpcmethod *findmethod(struct rpcmoduleinfo *rpi, const char *name)
{
	struct rpcmethod *rm;

	for (rm = rpi->methods; rm; rm = rm->next) {
		if (strcasecmp(rm->name, name) == 0)
			return rm;
	}

	return NULL;
}

static int addmethod(struct rpcmoduleinfo *rpi, const char *name, naf_rpc_method_t func, const char *desc)
{
	struct rpcmethod *rm;

	if (findmethod(rpi, name))
		return 0;

	if (!(rm = naf_malloc(ourmodule, sizeof(struct rpcmethod))))
		return -1;
	memset(rm, 0, sizeof(struct rpcmethod));

	if (!(rm->name = naf_strdup(ourmodule, name))) {
		naf_free(ourmodule, rm);
		return -1;
	}
	rm->func = func;
	if (desc && !(rm->desc = naf_strdup(ourmodule, desc))) {
		naf_free(ourmodule, rm->name);
		naf_free(ourmodule, rm);
		return -1;
	}

	rm->next = rpi->methods;
	rpi->methods = rm;

	return 0;
}

int naf_rpc_register_method(struct nafmodule *mod, const char *name, naf_rpc_method_t func, const char *desc)
{
	struct rpcmoduleinfo *rpi;

	if (!(rpi = fetchinfo(mod)))
		return -1;

	return addmethod(rpi, name, func, desc);
}

int naf_rpc_unregister_method(struct nafmodule *mod, const char *name)
{
	struct rpcmoduleinfo *rpi;
	struct rpcmethod *cur, **prev;

	if (!(rpi = fetchinfo(mod)))
		return -1;

	for (prev = &rpi->methods; (cur = *prev); ) {

		if (strcasecmp(cur->name, name) == 0) {
			*prev = cur->next;
			freemethod(cur);
			return 0;
		}

		prev = &cur->next;
	}

	return -1;
}

int naf_rpc_request_issue(struct nafmodule *mod, naf_rpc_req_t *req)
{
	struct nafmodule *target;
	struct rpcmoduleinfo *rpi;
	struct rpcmethod *rm;

	if (!req)
		return -1;

	if (!(target = naf_module_findbyname(mod, req->target))) {
		req->status = NAF_RPC_STATUS_UNKNOWNTARGET;
		return 0;
	}

	if (!(rpi = fetchinfo(target))) {
		req->status = NAF_RPC_STATUS_UNKNOWNMETHOD;
		return 0;
	}

	if (!(rm = findmethod(rpi, req->method))) {
		req->status = NAF_RPC_STATUS_UNKNOWNMETHOD;
		return 0;
	}

	if (1)
		dvprintf(ourmodule, "%s->%s() invoked by %s\n", req->target, req->method, mod->name);

	rm->func(target, req); /* will set req->status */

	return 0;
}

static void appendarg(naf_rpc_arg_t **head, naf_rpc_arg_t *narg)
{

	narg->next = NULL;

	if (!*head)
		*head = narg;
	else {
		naf_rpc_arg_t *cur;

		for (cur = *head; cur->next; cur = cur->next)
			;
		cur->next = narg;
	}

	return;
}

int naf_rpc_addarg_scalar(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name, naf_rpcu32_t val)
{
	naf_rpc_arg_t *narg;

	if (!(narg = naf_rpc_arg_new(mod, name)))
		return -1;

	narg->type = NAF_RPC_ARGTYPE_SCALAR;
	narg->length = sizeof(naf_rpcu32_t);
	narg->data.scalar = val;

	appendarg(head, narg);

	return 0;
}

int naf_rpc_addarg_bool(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name, naf_rpcu8_t val)
{
	naf_rpc_arg_t *narg;

	if (!(narg = naf_rpc_arg_new(mod, name)))
		return -1;

	narg->type = NAF_RPC_ARGTYPE_BOOL;
	narg->length = sizeof(naf_rpcu8_t);
	narg->data.boolean = val;

	appendarg(head, narg);

	return 0;
}

int naf_rpc_addarg_generic(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name, unsigned char *data, int datalen)
{
	naf_rpc_arg_t *narg;

	if (!(narg = naf_rpc_arg_new(mod, name)))
		return -1;

	narg->type = NAF_RPC_ARGTYPE_GENERIC;
	narg->length = datalen;
	if (!(narg->data.generic = naf_malloc(mod, datalen))) {
		naf_rpc_arg_free(mod, narg);
		return -1;
	}
	memcpy(narg->data.generic, data, datalen);

	appendarg(head, narg);

	return 0;
}

int naf_rpc_addarg_string(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name, const char *string)
{
	naf_rpc_arg_t *narg;

	if (!(narg = naf_rpc_arg_new(mod, name)))
		return -1;

	narg->type = NAF_RPC_ARGTYPE_STRING;
	narg->length = strlen(string) + 1;
	if (!(narg->data.string = naf_strdup(mod, string))) {
		naf_rpc_arg_free(mod, narg);
		return -1;
	}

	appendarg(head, narg);

	return 0;
}

/* returns an arglist head for future addarg calls for adding children */
naf_rpc_arg_t **naf_rpc_addarg_array(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name)
{
	naf_rpc_arg_t *narg;

	if (!(narg = naf_rpc_arg_new(mod, name)))
		return NULL;

	narg->type = NAF_RPC_ARGTYPE_ARRAY;
	narg->data.children = NULL;

	appendarg(head, narg);

	return &narg->data.children;
}

naf_rpc_arg_t *naf_rpc_getarg(naf_rpc_arg_t *head, const char *name)
{
	naf_rpc_arg_t *arg;

	for (arg = head; arg; arg = arg->next) {

		if (strcasecmp(arg->name, name) == 0)
			return arg;
	}

	return NULL;
}


static int helpiter(struct nafmodule *mod, struct nafmodule *cur, void *udata)
{
	naf_rpc_arg_t **top = (naf_rpc_arg_t **)udata;
	naf_rpc_arg_t **ptop;
	struct rpcmoduleinfo *rpi;
	struct rpcmethod *rm;

	if (!(rpi = fetchinfo(cur)))
		return 0;

	if (!rpi->methods)
		return 0;

	if ((ptop = naf_rpc_addarg_array(mod, top, cur->name))) {

		for (rm = rpi->methods; rm; rm = rm->next) {
			naf_rpc_arg_t **method;

			if ((method = naf_rpc_addarg_array(mod, ptop, rm->name))) {

				if (rm->desc)
					naf_rpc_addarg_string(mod, method, "description", rm->desc);
			}
		}
	}

	return 0;
}

/*
 * rpc->help()
 *   IN:
 *      [optional] string module;
 *
 *   OUT:
 *      [optional] array methods {
 *          array module {
 *              array method {
 *      	    [optional] string description;
 *      	}
 *          }
 *      }
 *
 */
static void __rpc_rpc_help(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t *module;
	naf_rpc_arg_t **methods;

	if ((module = naf_rpc_getarg(req->inargs, "module"))) {
		if (module->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	dvprintf(mod, "rpc: rpc->help invoked with module=%s\n", module ? module->data.string : "(none)");

	if ((methods = naf_rpc_addarg_array(mod, &req->returnargs, "methods"))) {

		if (module) {
			struct nafmodule *pmod;

			if ((pmod = naf_module_findbyname(ourmodule, module->data.string)))
				helpiter(ourmodule, pmod, (void *)methods);

		} else
			naf_module_iter(ourmodule, helpiter, (void *)methods);
	}

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}

static int modinit(struct nafmodule *mod)
{

	ourmodule = mod;

	naf_rpc_register_method(mod, "help", __rpc_rpc_help, "List registered methods");

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	ourmodule = NULL;

	return 0;
}

static void freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{

	if (strcmp(tagname, "module.rpi") == 0) {
		struct rpcmoduleinfo *rpi = (struct rpcmoduleinfo *)tagdata;

		freeinfo(rpi);
	}

	return;
}

static int modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "rpc");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->freetag = freetag;

	return 0;
}

int naf_rpc__register(void)
{
	return naf_module__registerresident("rpc", modfirst, NAF_MODULE_PRI_FIRSTPASS);
}

