
/*
 * This module exports an API that makes it simple for modules to store a
 * cache of arbitrary key/value pairs, with finite timeouts.  Each module
 * can store several seperate "lists" of key/value pairs, specified by a 
 * list ID number.  
 *
 * Before using the API, a module must register for the services using
 * naf_cache_register(), which sets up the internal environment for that
 * module.
 *
 * New lists are created with naf_cache_addlist(), which specifies a function
 * used to free items of this list, and a timeout that applies to all items
 * in this list.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <naf/nafmodule.h>
#include <naf/nafcache.h>

#include "module.h" /* for naf_module__registerresident() */
#include "cache.h"

static struct nafmodule *ourmodule = NULL;


/* ------------------------------ cachepair ------------------------------ */


struct cachepair {
	void *key;
	void *value;
	time_t addtime;
	struct cachepair *next;
};

struct cachelist {
	naf_cache_lid_t lid;
	time_t lastrun;
	int timeout;
	naf_cache_freepairfunc_t freepair;
	struct cachepair *pairs;
	struct cachelist *next;
};


static struct cachepair *cp_alloc(void *key, void *value)
{
	struct cachepair *cp;

	if (!(cp = naf_malloc(ourmodule, sizeof(struct cachepair))))
		return NULL;
	memset(cp, 0, sizeof(struct cachepair));

	cp->key = key;
	cp->value = value;
	cp->addtime = time(NULL);

	return cp;
}

static void cp_free(struct nafmodule *mod, struct cachelist *cl, struct cachepair *cp)
{

	if (cl->freepair)
		cl->freepair(mod, cl->lid, cp->key, cp->value, cp->addtime);
	naf_free(ourmodule, cp);

	return;
}

/* ------------------------------ cachelist ------------------------------ */


static struct cachelist *cl_alloc(naf_cache_lid_t lid, int timeout, naf_cache_freepairfunc_t freepair)
{
	struct cachelist *cl;

	if (!(cl = naf_malloc(ourmodule, sizeof(struct cachelist))))
		return NULL;
	memset(cl, 0, sizeof(struct cachelist));

	cl->lid = lid;
	cl->lastrun = time(NULL);
	cl->timeout = timeout;
	cl->freepair = freepair;
	cl->pairs = NULL;

	return cl;
}

static void cl_free(struct nafmodule *mod, struct cachelist *cl)
{
	struct cachepair *cp;

	for (cp = cl->pairs; cp; cp = cp->next) {
		struct cachepair *cptmp;

		cptmp = cp->next;
		cp_free(mod, cl, cp);
		cp = cptmp;
	}
	naf_free(ourmodule, cl);

	return;
}


/* ------------------------------ ctd ------------------------------ */


struct cachetagdata {
	struct cachelist *lists;
};

static struct cachetagdata *ctd_alloc(void)
{
	struct cachetagdata *ctd;

	if (!(ctd = naf_malloc(ourmodule, sizeof(struct cachetagdata))))
		return NULL;
	memset(ctd, 0, sizeof(struct cachetagdata));

	ctd->lists = NULL;

	return ctd;
}

static void ctd_free(struct nafmodule *mod, struct cachetagdata *ctd)
{
	struct cachelist *cl;

	for (cl = ctd->lists; cl; cl = cl->next) {
		struct cachelist *cltmp;

		cltmp = cl->next;
		cl_free(mod, cl);
		cl = cltmp;
	}
	naf_free(ourmodule, ctd);

	return;
}

static struct cachelist *ctd_findlist(struct cachetagdata *ctd, naf_cache_lid_t lid)
{
	struct cachelist *cl;

	for (cl = ctd->lists; cl; cl = cl->next) {
		if (cl->lid == lid)
			return cl;
	}

	return NULL;
}


/* ------------------------------ naf_cache ------------------------------ */


int naf_cache_register(struct nafmodule *mod)
{
	struct cachetagdata *ctd;

	if (naf_module_tag_ispresent(ourmodule, mod, "module.cachetagdata") == 1)
		return 0; /* already registered */

	if (!(ctd = ctd_alloc()))
		return -1;

	if (naf_module_tag_add(ourmodule, mod, "module.cachetagdata", 'V', (void *)ctd) == -1) {
		ctd_free(mod, ctd);
		return -1;
	}

	return 0;
}

void naf_cache_unregister(struct nafmodule *mod)
{
	struct cachetagdata *ctd;

	if (naf_module_tag_fetch(ourmodule, mod, "module.cachetagdata", NULL, (void **)&ctd) == -1)
		return; /* not registered */

	ctd_free(mod, ctd);

	return;
}

int naf_cache_addlist(struct nafmodule *mod, naf_cache_lid_t lid, int timeout, naf_cache_freepairfunc_t freepair)
{
	struct cachetagdata *ctd;
	struct cachelist *cl;

	if (naf_module_tag_fetch(ourmodule, mod, "module.cachetagdata", NULL, (void **)&ctd) == -1)
		return -1; /* not registered */

	if ((cl = ctd_findlist(ctd, lid)))
		return -1; /* already present */

	if (!(cl = cl_alloc(lid, timeout, freepair)))
		return -1;

	cl->next = ctd->lists;
	ctd->lists = cl;

	return 0;
}

int naf_cache_remlist(struct nafmodule *mod, naf_cache_lid_t lid)
{
	struct cachetagdata *ctd;
	struct cachelist *cur, **prev;

	if (naf_module_tag_fetch(ourmodule, mod, "module.cachetagdata", NULL, (void **)&ctd) == -1)
		return -1; /* not registered */

	for (prev = &ctd->lists; (cur = *prev); ) {

		if (cur->lid == lid) {
			*prev = cur->next;
			cl_free(mod, cur);
		} else
			prev = &cur->next;
	}

	return 0;
}

int naf_cache_addpair(struct nafmodule *mod, naf_cache_lid_t lid, void *key, void *value)
{
	struct cachetagdata *ctd;
	struct cachelist *cl;
	struct cachepair *cp;

	if (naf_module_tag_fetch(ourmodule, mod, "module.cachetagdata", NULL, (void **)&ctd) == -1)
		return -1; /* not registered */

	if (!(cl = ctd_findlist(ctd, lid)))
		return -1; /* unknown lid */

	if (!(cp = cp_alloc(key, value)))
		return -1;

	cp->next = cl->pairs;
	cl->pairs = cp;

	return 0;
}

int naf_cache_findpair(struct nafmodule *mod, naf_cache_lid_t lid, naf_cache_matcherfunc_t matcher, void *matcherdata, void **keyret, void **valueret, int hit)
{
	struct cachetagdata *ctd;
	struct cachelist *cl;
	struct cachepair *cur;

	if (naf_module_tag_fetch(ourmodule, mod, "module.cachetagdata", NULL, (void **)&ctd) == -1)
		return -1; /* not registered */

	if (!(cl = ctd_findlist(ctd, lid)))
		return -1; /* unknown lid */

	for (cur = cl->pairs; cur; cur = cur->next) {

		if (matcher(mod, cl->lid, cur->key, cur->value, matcherdata)) {
			if (keyret)
				*keyret = cur->key;
			if (valueret)
				*valueret = cur->value;
			if (hit)
				cur->addtime = time(NULL);

			return 1;
		}
	}

	return 0;
}

int naf_cache_rempairs(struct nafmodule *mod, naf_cache_lid_t lid, naf_cache_matcherfunc_t matcher, void *matcherdata)
{
	struct cachetagdata *ctd;
	struct cachelist *cl;
	struct cachepair *cur, **prev;

	if (naf_module_tag_fetch(ourmodule, mod, "module.cachetagdata", NULL, (void **)&ctd) == -1)
		return -1; /* not registered */

	if (!(cl = ctd_findlist(ctd, lid)))
		return -1; /* unknown lid */

	for (prev = &cl->pairs; (cur = *prev); ) {

		if (matcher(mod, cl->lid, cur->key, cur->value, matcherdata)) {

			*prev = cur->next;

			cp_free(mod, cl, cur);

		} else
			prev = &cur->next;
	}

	return 0;
}

static int modinit(struct nafmodule *mod)
{

	ourmodule = mod; /* needed for exported APIs */

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	ourmodule = NULL;

	return 0;
}

static int timerhandler_iter(struct nafmodule *mod, struct nafmodule *curmod, void *udata)
{
	struct cachetagdata *ctd;
	struct cachelist *cl;
	time_t now;

	now = (time_t)udata;

	if (naf_module_tag_fetch(mod, curmod, "module.cachetagdata", NULL, (void **)&ctd) == -1)
		return 0;

	for (cl = ctd->lists; cl; cl = cl->next) {
		struct cachepair *cur, **prev;

		if ((now - cl->lastrun) < cl->timeout)
			continue;

		for (prev = &cl->pairs; (cur = *prev); ) {

			if ((now - cur->addtime) >= cl->timeout) {
				*prev = cur->next;
				cp_free(curmod, cl, cur);
			} else
				prev = &cur->next;
		}

		cl->lastrun = now;
	}

	return 0;
}

/* Called every NAF_CACHE_TIMER_FREQ seconds by the main loop */
static void timerhandler(struct nafmodule *mod)
{
	time_t now;

	now = time(NULL);

	naf_module_iter(mod, timerhandler_iter, (void *)now);

	return;
}

static void freetag(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata)
{

	if (strcmp(tagname, "module.cachetagdata") == 0) {
		struct nafmodule *modobj = (struct nafmodule *)object;

		ctd_free(modobj, (struct cachetagdata *)tagdata);
	}

	return;
}


static int modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "cache");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->timerfreq = NAF_CACHE_TIMER_FREQ;
	mod->timer = timerhandler;
	mod->freetag = freetag;

	return 0;
}

int naf_cache__register(void)
{
	return naf_module__registerresident("cache", modfirst, NAF_MODULE_PRI_THIRDPASS);
}

