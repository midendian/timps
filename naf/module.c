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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <naf/nafconfig.h>
#include <naf/nafrpc.h>
#include <naf/naftag.h>

#include "module.h"
#include "core.h"
#include "memory.h"

#define MODULE_MAXFILENAME_LEN 256

#define MOD_STATUS_NOTLOADED 0x00
#define MOD_STATUS_LOADED    0x01
#define MOD_STATUS_DISABLED  0x02
#define MOD_STATUS_ERROR     0x04
#define MOD_STATUS_RESIDENT  0x10 /* ORed with others */

struct modlist_item {
	char filename[MODULE_MAXFILENAME_LEN+1];
	naf_u8_t status;
	void *dlhandle;
	int (*firstproc)(struct nafmodule *);
	struct nafmodule module;
	time_t lasttimerrun;
	int startuppri;
	struct modlist_item *next;
};
static struct modlist_item *modlist = NULL;

void __rpc_core_listmodules(struct nafmodule *mod, naf_rpc_req_t *req)
{
	static const char *statuses[] = {
		"not loaded", "loaded", "disabled", "error", "unknown"
	};
	const char *curstat = NULL;
	struct modlist_item *cur;
	naf_rpc_arg_t **modules;

	if ((modules = naf_rpc_addarg_array(mod, &req->returnargs, "modules"))) {

		for (cur = modlist; cur; cur = cur->next) {
			naf_rpc_arg_t **ptop;

			if ((ptop = naf_rpc_addarg_array(mod, modules, cur->filename))) {

				if (cur->status & MOD_STATUS_ERROR)
					curstat = statuses[3];
				else if (cur->status & MOD_STATUS_DISABLED)
					curstat = statuses[2];
				else if (cur->status & MOD_STATUS_LOADED)
					curstat = statuses[1];
				else if (cur->status & MOD_STATUS_NOTLOADED)
					curstat = statuses[0];
				else 
					curstat = statuses[4];

				if (cur->status & MOD_STATUS_LOADED)
					naf_rpc_addarg_string(mod, ptop, "name", cur->module.name);
				naf_rpc_addarg_string(mod, ptop, "status", curstat);
			}
		}
	}

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}

static struct modlist_item *newmi(const char *fn)
{
	struct modlist_item *mi;

	if (!fn || (strlen(fn) > MODULE_MAXFILENAME_LEN) || 
			!(mi = malloc(sizeof(struct modlist_item))))
		return NULL;
	memset(mi, 0, sizeof(struct modlist_item));

	mi->dlhandle = NULL;
	strncpy(mi->filename, fn, sizeof(mi->filename));
	mi->status = MOD_STATUS_NOTLOADED;
	mi->startuppri = NAF_MODULE_PRI_LASTPASS; /* user-loaded modules come last */

	return mi;
}

int naf_module__add(const char *fn)
{
	struct modlist_item *mi;

	if (!(mi = newmi(fn)))
		return -1;

	mi->next = modlist;
	modlist = mi;

	return 0;
}

/* same as above, but appends instead of prepends */
int naf_module__add_last(const char *fn)
{
	struct modlist_item *mi, *cur;

	if (!(mi = newmi(fn)))
		return -1;

	mi->next = NULL;

	if (!modlist)
		modlist = mi;
	else {
		for (cur = modlist; cur->next; cur = cur->next)
			;
		cur->next = mi;
	}

	return 0;
}

int naf_module__registerresident(const char *name, int (*firstproc)(struct nafmodule *), int startuppri)
{
	struct modlist_item *newmi;

	if (!name || !firstproc)
		return -1;

	if (!(newmi = malloc(sizeof(struct modlist_item))))
		return -1;
	memset(newmi, 0, sizeof(struct modlist_item));

	newmi->dlhandle = NULL;
	snprintf(newmi->filename, sizeof(newmi->filename), "resident/%s", name);
	newmi->firstproc = firstproc;
	newmi->startuppri = startuppri;

	newmi->status = MOD_STATUS_NOTLOADED | MOD_STATUS_RESIDENT;

	newmi->next = modlist;
	modlist = newmi;

	return 0;
}

static int naf_module__load(struct modlist_item *mi)
{
	int ret = 0;

	if (mi->status & MOD_STATUS_LOADED)
		return 0;

	if (!(mi->status & MOD_STATUS_RESIDENT)) {

		if (!(mi->dlhandle = dlopen(mi->filename, RTLD_NOW))) {
			dvprintf(NULL, "%s: dlopen: %s\n", mi->filename, dlerror());
			mi->status = MOD_STATUS_ERROR;
			return -1;
		}

		if (!(mi->firstproc = dlsym(mi->dlhandle, "nafmodulemain"))) {
			dvprintf(NULL, "%s: dlsym(nafmodulemain): %s\n", mi->filename, dlerror());
			dlclose(mi->dlhandle);
			mi->status = MOD_STATUS_ERROR;
			return -1;
		}
	}

	if (!mi->firstproc || ((ret = mi->firstproc(&mi->module)) != 0)) {
		dvprintf(NULL, "%s: nafmodulemain returned error %d\n", mi->filename, ret);
		if (mi->dlhandle)
			dlclose(mi->dlhandle);
		mi->status |= MOD_STATUS_ERROR;
		return -1;
	}

	mi->status |= MOD_STATUS_LOADED;

	if (mi->module.init)
		mi->module.init(&mi->module);

	dvprintf(NULL, "loaded module %s [%s]\n", mi->module.name, mi->filename);

	mi->lasttimerrun = time(NULL);

	return 0;
}

int naf_module__loadall(int minpri)
{
	struct modlist_item *cur;
	int pri;
 
	for (pri = NAF_MODULE_PRI_MAX; pri >= minpri; pri--) {
		for (cur = modlist; cur; cur = cur->next) {
			if (cur->startuppri != pri)
				continue;
			naf_module__load(cur);
		}
	}

	return 0;
}

int naf_module__unloadall(void)
{
	struct modlist_item *cur;
 
	for (cur = modlist; cur; cur = cur->next) {

		if (!(cur->status & MOD_STATUS_LOADED))
			continue;

		if (cur->module.shutdown)
			cur->module.shutdown(&cur->module);
		if (cur->dlhandle)
			dlclose(cur->dlhandle);
		cur->status &= ~MOD_STATUS_LOADED;
		cur->status |= MOD_STATUS_NOTLOADED;
		cur->lasttimerrun = 0;
		naf_memory__module_free(&cur->module);
		/* XXX free tags */
	}

	return 0;
}

void naf_module_iter(struct nafmodule *caller, int (*ufunc)(struct nafmodule *, struct nafmodule *, void *), void *udata)
{
	struct modlist_item *cur;

	if (!caller || !ufunc)
		return;

	for (cur = modlist; cur; cur = cur->next) {

		if (!(cur->status & MOD_STATUS_LOADED))
			continue;

		if (ufunc(caller, &cur->module, udata))
			return;
	}

	return;
}

struct nafmodule *naf_module_findbyname(struct nafmodule *caller, const char *name)
{
	struct modlist_item *cur;

	if (!caller || !name)
		return NULL;

	for (cur = modlist; cur; cur = cur->next) {

		if (!(cur->status & MOD_STATUS_LOADED))
			continue;

		if (strcasecmp(name, cur->module.name) == 0)
			return &cur->module;
	}

	return NULL;
}

int nafeventv(struct nafmodule *source, naf_event_t event, va_list inap)
{
	struct modlist_item *cur;

	for (cur = modlist; cur; cur = cur->next) {

		if (&cur->module == source)
			continue;

		if (!(cur->status & MOD_STATUS_LOADED))
			continue;

		if (cur->module.event) {
			va_list ap;

			va_copy(ap, inap);
			cur->module.event(&cur->module, source, event, ap);
			va_end(ap);
		}
	}
 
	return 0;
}

int nafevent(struct nafmodule *source, naf_event_t event, ...)
{
	va_list ap;

	va_start(ap, event);
	nafeventv(source, event, ap);
	va_end(ap);

	return 0;
}

#ifdef NOVAMACROS
int dvprintf(struct nafmodule *mod, ...)
{
	va_list ap;

	va_start(ap, mod);
	nafeventv(mod, NAF_EVENT_GENERICOUTPUT, ap);
	va_end(ap);

	return 0;
}
#endif /* def NOVAMACROS */

/* 
 * Broadcast a signal to all modules.
 *
 * source will usually be NULL.
 */
void nafsignal(struct nafmodule *source, int signum)
{
	struct modlist_item *cur;

	for (cur = modlist; cur; cur = cur->next) {

		if (&cur->module == source)
			continue;

		if (!(cur->status & MOD_STATUS_LOADED))
			continue;

		if (cur->module.signal)
			cur->module.signal(&cur->module, source, signum);
	}
 
	return;
}

/*
 * This is run by the main loop every NAF_TIMER_ACCURACY seconds.
 */
void naf_module__timerrun(void)
{
	struct modlist_item *cur;

	for (cur = modlist; cur; cur = cur->next) {

		if (!(cur->status & MOD_STATUS_LOADED))
			continue;

		if (cur->module.timerfreq) {
			if ((time(NULL) - cur->lasttimerrun) >= cur->module.timerfreq) {
				cur->module.timer(&cur->module);
				cur->lasttimerrun = time(NULL);
			}
		}

	}

	return;
}

int naf_module__protocoldetect(struct nafmodule *mod, struct nafconn *conn)
{
	struct modlist_item *cur;
	int ret = 0;

	for (cur = modlist; cur && !conn->owner; cur = cur->next) {

		if (&cur->module == mod)
			continue;

		if (!(cur->status & MOD_STATUS_LOADED))
			continue;

		if (cur->module.protocoldetect) {
			if ((ret = cur->module.protocoldetect(&cur->module, conn)) == 1) {

				conn->owner = &cur->module;
				if (cur->module.takeconn) {
					if (cur->module.takeconn(&cur->module, conn) == -1)
						return -1;
				}

				return 1;

			} else if (ret == -1)
				return -1;
		}
	}
 
	return 0;
}

int naf_module__protocoldetecttimeout(struct nafmodule *mod, struct nafconn *conn)
{
	struct modlist_item *cur;
	int ret = 0;

	for (cur = modlist; cur && !conn->owner; cur = cur->next) {

		if (&cur->module == mod)
			continue;

		if (!(cur->status & MOD_STATUS_LOADED))
			continue;

		if (cur->module.connready) {
			if ((ret = cur->module.connready(&cur->module, conn, NAF_CONN_READY_DETECTTO)) == 1) {
				conn->owner = &cur->module;
				return 1;
			} 
		}
	}
 
	return 0;
}


int naf_module_setname(struct nafmodule *mod, const char *name)
{

	if (!mod || !name)
		return -1;

	strncpy(mod->name, name, NAF_MODULE_NAME_MAX);

	return 0;
}

void naf_module_setstatusline(struct nafmodule *mod, const char *line)
{

	if (!mod)
		return;

	strncpy(mod->statusline, line, NAF_MODULE_STATUSLINE_MAXLEN);

	return;
}


int naf_module_tag_add(struct nafmodule *mod, struct nafmodule *target, const char *name, char type, void *data)
{

	if (!target)
		return -1;

	return naf_tag_add(&target->taglist, mod, name, type, data);
}

int naf_module_tag_remove(struct nafmodule *mod, struct nafmodule *target, const char *name, char *typeret, void **dataret)
{

	if (!target)
		return -1;

	return naf_tag_remove(&target->taglist, mod, name, typeret, dataret);
}

int naf_module_tag_ispresent(struct nafmodule *mod, struct nafmodule *target, const char *name)
{

	if (!target)
		return -1;

	return naf_tag_ispresent(&target->taglist, mod, name);
}

int naf_module_tag_fetch(struct nafmodule *mod, struct nafmodule *target, const char *name, char *typeret, void **dataret)
{

	if (!target)
		return -1;

	return naf_tag_fetch(&target->taglist, mod, name, typeret, dataret);
}

