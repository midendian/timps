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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <naf/nafmodule.h>
#include <naf/nafstats.h>
#include <naf/nafrpc.h>

#include "module.h" /* for naf_module__registerresident() only */


static struct nafmodule *ourmodule = NULL;


#define NAF_STATS_TYPE_LONG  0
#define NAF_STATS_TYPE_SHORT 1

#define NAF_STATS_MAXNAMELEN 128


struct naf_stat {
	char name[NAF_STATS_MAXNAMELEN];
	int type;
	union {
		naf_longstat_t *lstat;
		naf_shortstat_t *sstat;
	} stat;
	struct nafmodule *owner;
	struct naf_stat *next;
};
static struct naf_stat *naf__statslist = NULL;


static int mkstatname(const char *mod, const char *name, char *buf, int buflen)
{

	if (mod && (strlen(mod)+1+strlen(name)+1 > buflen))
		return -1;
	else if (!mod && (strlen(name)+1 > buflen))
		return -1;

	snprintf(buf, buflen, "%s%s%s", mod ? mod : "",
					mod ? "." : "",
					name);

	return 0;
}

static int registerstat(struct nafmodule *owner, const char *name, int type, void *statp)
{
	struct naf_stat *nps;

	if (!(nps = naf_malloc(ourmodule, sizeof(struct naf_stat))))
		return -1;
	memset(nps, 0, sizeof(struct naf_stat));

	if (mkstatname(owner ? owner->name : NULL, name, nps->name, sizeof(nps->name)) == -1) {
		naf_free(ourmodule, nps);
		return -1;
	}

	nps->type = type;
	if (nps->type == NAF_STATS_TYPE_LONG)
		nps->stat.lstat = (naf_longstat_t *)statp;
	else if (nps->type == NAF_STATS_TYPE_SHORT)
		nps->stat.sstat = (naf_shortstat_t *)statp;

	nps->owner = owner;

	nps->next = naf__statslist;
	naf__statslist = nps;

	return 0;
}

static void naf_stats__freestat(struct naf_stat *nps)
{

	naf_free(ourmodule, nps);

	return;
}

int naf_stats_unregisterstat(struct nafmodule *owner, const char *name)
{
	char realname[NAF_STATS_MAXNAMELEN+1];
	struct naf_stat *cur, **prev;

	if (mkstatname(owner ? owner->name : NULL, name, realname, sizeof(realname)) == -1)
		return -1;

	for (prev = &naf__statslist; (cur = *prev); ) {

		if (strcasecmp(realname, cur->name) == 0) {
			*prev = cur->next;
			naf_stats__freestat(cur);
			return 0;
		}

		prev = &cur->next;
	}

	return -1;
}

static struct naf_stat *findstat(const char *name)
{
	struct naf_stat *nps;

	for (nps = naf__statslist; nps; nps = nps->next) {

		if (strcasecmp(name, nps->name) == 0)
			return nps;
	}

	return NULL;
}

int naf_stats_getstatvalue(struct nafmodule *mod, const char *name, int *typeret, void *buf, int *buflen)
{
	char realname[NAF_STATS_MAXNAMELEN+1];
	struct naf_stat *nps;
	int len;

	if (!name)
		return -1;

	if (mkstatname(mod->name, name, realname, sizeof(realname)) == -1)
		return -1;

	if (!(nps = findstat(realname)))
		return -1;

	if (typeret)
		*typeret = nps->type;

	if (!buf || !buflen || !*buflen)
		return 0; /* they dont' want the value */

	if (nps->type == NAF_STATS_TYPE_SHORT)
		len = sizeof(naf_shortstat_t);
	else if (nps->type == NAF_STATS_TYPE_LONG)
		len = sizeof(naf_longstat_t);
	else
		return -1;

	if (*buflen < len)
		return -1;

	*buflen = len;
	if (nps->type == NAF_STATS_TYPE_SHORT)
		*(naf_shortstat_t *)buf = *nps->stat.sstat;
	else if (nps->type == NAF_STATS_TYPE_LONG)
		*(naf_longstat_t *)buf = *nps->stat.lstat;

	return 0;
}

int naf_stats_register_longstat(struct nafmodule *owner, const char *name, naf_longstat_t *statp)
{
	return registerstat(owner, name, NAF_STATS_TYPE_LONG, (void *)statp);
}

int naf_stats_register_shortstat(struct nafmodule *owner, const char *name, naf_shortstat_t *statp)
{
	return registerstat(owner, name, NAF_STATS_TYPE_SHORT, (void *)statp);
}

static void freestatlist(void)
{
	struct naf_stat *nps;

	for (nps = naf__statslist; nps; ) {
		struct naf_stat *tmp;

		tmp = nps->next;
		naf_stats__freestat(nps);
		nps = tmp;
	}

	naf__statslist = NULL;

	return;
}


/*
 * stats->getstat()
 *   IN:
 *      [optional] string module;
 *      string stat;
 *
 *   OUT:
 *      [optional] string module;
 *      string stat;
 *      [optional] scalar value;
 *      
 */
static void __rpc_stats_getstat(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t *module, *stat;
	struct naf_stat *nps;
	char buf[NAF_STATS_MAXNAMELEN+1];

	if ((module = naf_rpc_getarg(req->inargs, "module"))) {
		if (module->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	if ((stat = naf_rpc_getarg(req->inargs, "stat"))) {
		if (stat->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	if (!stat || (mkstatname(module ? module->data.string : NULL, stat->data.string, buf, sizeof(buf)) == -1)) {
		req->status = NAF_RPC_STATUS_INVALIDARGS;
		return;
	}

	if (module)
		naf_rpc_addarg_string(mod, &req->returnargs, "module", module->data.string);
	naf_rpc_addarg_string(mod, &req->returnargs, "stat", stat->data.string);

	if ((nps = findstat(buf))) {

		/* NAF RPC only has one scalar type, and is big enough for LONG */
		if (nps->type == NAF_STATS_TYPE_SHORT)
			naf_rpc_addarg_scalar(mod, &req->returnargs, "value", *nps->stat.sstat);
		else if (nps->type == NAF_STATS_TYPE_LONG)
			naf_rpc_addarg_scalar(mod, &req->returnargs, "value", *nps->stat.lstat);
	}

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}

/*
 * stats->liststats()
 *   IN:
 *      [optional] string filter;
 *      [optional] bool wantvalues;
 *
 *   OUT:
 *      [optional] array stats {
 *          array stat {
 *              [optional] scalar value;
 *          }
 *      }
 */
static void __rpc_stats_liststats(struct nafmodule *mod, naf_rpc_req_t *req)
{
	struct naf_stat *nps;
	int wantvalues = 0;
	naf_rpc_arg_t *filter, *wv, **head;

	if ((filter = naf_rpc_getarg(req->inargs, "filter"))) {
		if (filter->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}

	}

	if ((wv = naf_rpc_getarg(req->inargs, "wantvalues"))) {
		if (wv->type != NAF_RPC_ARGTYPE_BOOL) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
		wantvalues = !!wv->data.boolean;
	}

	if (!(head = naf_rpc_addarg_array(mod, &req->returnargs, "stats"))) {
		req->status = NAF_RPC_STATUS_UNKNOWNFAILURE;
		return;
	}

	for (nps = naf__statslist; nps; nps = nps->next) {
		naf_rpc_arg_t **ass;

		if (filter && (strncasecmp(nps->name, filter->data.string, strlen(filter->data.string)) != 0))
			continue;

		if ((ass = naf_rpc_addarg_array(mod, head, nps->name))) {

			if (wantvalues) {
				if (nps->type == NAF_STATS_TYPE_SHORT)
					naf_rpc_addarg_scalar(mod, ass, "value", *nps->stat.sstat);
				else if (nps->type == NAF_STATS_TYPE_LONG)
					naf_rpc_addarg_scalar(mod, ass, "value", *nps->stat.lstat);
			}
		}
	}

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}


static struct {
	naf_longstat_t starttime;
} corestats = {
	0,
};


static int modinit(struct nafmodule *mod)
{

	ourmodule = mod;

	corestats.starttime = time(NULL);

	naf_stats_register_longstat(mod, "core.starttime", &corestats.starttime);

	naf_rpc_register_method(mod, "getstat", __rpc_stats_getstat, "Retrieve value of named statistic");
	naf_rpc_register_method(mod, "liststats", __rpc_stats_liststats, "Retrieve list of statistics and values");

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	freestatlist();

	ourmodule = NULL;

	return 0;
}

static int modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "stats");
	mod->init = modinit;
	mod->shutdown = modshutdown;

	return 0;
}

int naf_stats__register(void)
{
	return naf_module__registerresident("stats", modfirst, NAF_MODULE_PRI_THIRDPASS);
}

