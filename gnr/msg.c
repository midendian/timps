/*
 * gnr - Generic interNode message Routing
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * gnr is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * gnr is distributed in the hope that it will be useful, but WITHOUT ANY
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

#include <naf/nafmodule.h>
#include <naf/nafrpc.h>
#include <naf/naftag.h>

#include <gnr/gnrmsg.h>
#include <gnr/gnrnode.h>
#include "core.h"

struct mhlist {
	int position;
	struct nafmodule *module;
	gnrmsg_msghandlerfunc_t handlerfunc;
	char *description;
	struct mhlist *next;
};
static struct mhlist *gnr__msghandlers[GNR_MSG_MSGHANDLER_STAGE_MAX+1];

static struct mhlist *mh_alloc(void)
{
	struct mhlist *mh;

	if (!(mh = naf_malloc(gnr__module, sizeof(struct mhlist))))
		return NULL;
	memset(mh, 0, sizeof(struct mhlist));

	return mh;
}

static void mh_free(struct mhlist *mh)
{

	if (mh)
		naf_free(gnr__module, mh->description);
	naf_free(gnr__module, mh);

	return;
}

static void mh_insert(int stage, struct mhlist *mh)
{
	struct mhlist *cur, **prev;

	for (prev = &gnr__msghandlers[stage]; (cur = *prev); ) {
		if (cur->position >= mh->position)
			break;
		prev = &cur->next;
	}

	mh->next = cur;
	*prev = mh;

	return;
}

static struct mhlist *mh_remove(int stage, gnrmsg_msghandlerfunc_t handlerfunc)
{
	struct mhlist *cur, **prev;

	for (prev = &gnr__msghandlers[stage]; (cur = *prev); ) {

		if (cur->handlerfunc == handlerfunc) {
			*prev = cur->next;
			return cur;
		}

		prev = &cur->next;
	}

	return NULL;
}

int gnr_msg_addmsghandler(struct nafmodule *mod, int stage, int position, gnrmsg_msghandlerfunc_t handlerfunc, const char *desc)
{
	struct mhlist *mh;

	if (stage > GNR_MSG_MSGHANDLER_STAGE_MAX)
		return -1;
	
	if ((position < GNR_MSG_MSGHANDLER_POS_MIN) ||
			(position > GNR_MSG_MSGHANDLER_POS_MAX))
		return -1;

	if (!(mh = mh_alloc()))
		return -1;

	mh->position = position;
	mh->module = mod;
	mh->handlerfunc = handlerfunc;
	if (!(mh->description = naf_strdup(gnr__module, desc))) {
		mh_free(mh);
		return -1;
	}

	mh_insert(stage, mh);

	return 0;
}

int gnr_msg_remmsghandler(struct nafmodule *mod, int stage, gnrmsg_msghandlerfunc_t handlerfunc)
{
	struct mhlist *mh;

	if (stage > GNR_MSG_MSGHANDLER_STAGE_MAX)
		return -1;

	if (!(mh = mh_remove(stage, handlerfunc)))
		return -1;

	mh_free(mh);

	return 0;
}


static void __rpc_gnr_listmsghandlers(struct nafmodule *mod, naf_rpc_req_t *req)
{
	static const char *stages[] = {
		"prerouting", "routing", "postrouting"
	};
	naf_rpc_arg_t **head;
	int i;

	for (i = 0; i <= GNR_MSG_MSGHANDLER_STAGE_MAX; i++) {
		struct mhlist *mh;

		if ((head = naf_rpc_addarg_array(mod, &req->returnargs, stages[i]))) {

			for (mh = gnr__msghandlers[i]; mh; mh = mh->next) {
				char buf[128];
				naf_rpc_arg_t **ptop;

				snprintf(buf, sizeof(buf), "%u", mh->position);

				if ((ptop = naf_rpc_addarg_array(mod, head, buf))) {

					naf_rpc_addarg_scalar(mod, ptop, "position", mh->position);
					naf_rpc_addarg_string(mod, ptop, "module", mh->module->name);
					naf_rpc_addarg_string(mod, ptop, "description", mh->description);
				}
			}
		}
	}

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}


struct gnrmsg *gnr_msg_new(struct nafmodule *mod)
{
	struct gnrmsg *gm;

	if (!(gm = naf_malloc(mod, sizeof(struct gnrmsg))))
		return NULL;
	memset(gm, 0, sizeof(struct gnrmsg));

	return gm;
}

void gnr_msg_free(struct nafmodule *mod, struct gnrmsg *gm)
{

	naf_tag_freelist(&gm->taglist, (void *)gm);

	naf_free(mod, gm);

	return;
}


int gnr_msg_clonetags(struct gnrmsg *destgm, struct gnrmsg *srcgm)
{

	if (!destgm || !srcgm)
		return -1;

	return naf_tag_cloneall((void **)&destgm->taglist, (void **)&srcgm->taglist);
}

int gnr_msg_tag_add(struct nafmodule *mod, struct gnrmsg *gm, const char *name, char type, void *data)
{

	if (!gm)
		return -1;

	return naf_tag_add(&gm->taglist, mod, name, type, data);
}

int gnr_msg_tag_remove(struct nafmodule *mod, struct gnrmsg *gm, const char *name, char *typeret, void **dataret)
{

	if (!gm)
		return -1;

	return naf_tag_remove(&gm->taglist, mod, name, typeret, dataret);
}

int gnr_msg_tag_ispresent(struct nafmodule *mod, struct gnrmsg *gm, const char *name)
{

	if (!gm)
		return -1;

	return naf_tag_ispresent(&gm->taglist, mod, name);
}

int gnr_msg_tag_fetch(struct nafmodule *mod, struct gnrmsg *gm, const char *name, char *typeret, void **dataret)
{

	if (!gm)
		return -1;

	return naf_tag_fetch(&gm->taglist, mod, name, typeret, dataret);
}


int gnr_msg_route(struct nafmodule *srcmod, struct gnrmsg *gm)
{
	struct gnrmsg_handler_info gmhi;
	struct mhlist *mh;

	gmhi.srcmod = srcmod;
	gmhi.destconn = NULL;
	gmhi.targetmod = NULL;

	/* Make sure we're routing a sane message... */
	if (!gm || !gm->srcname || !gm->srcnameservice ||
			!gm->destname || !gm->destnameservice) {
		dvprintf(gnr__module, "gnr_msg_route: invalid args (%p/%s[%s]/%s[%s])\n",
				gm,
				gm ? gm->srcname : NULL,
				gm ? gm->srcnameservice : NULL, 
				gm ? gm->destname : NULL,
				gm ? gm->destnameservice : NULL);
		return -1;
	}

	gmhi.srcnode = gnr_node_findbyname(gm->srcname, gm->srcnameservice);
	gmhi.destnode = gnr_node_findbyname(gm->destname, gm->destnameservice);

	if (gnr__debug > 0) {
		dvprintf(gnr__module, "gnr_msg_route: from %s, %s[%s] -> %s[%s], msgtext = (%s) '%s', msgflags = %08lx, srconn = %d\n",
				srcmod,
				gm->srcname, gm->srcnameservice,
				gm->destname, gm->destnameservice,
				gm->msgtexttype ? gm->msgtexttype : "type not specified",
				gm->msgtext, gm->msgflags,
				gm->srcconn ? gm->srcconn->cid : -1);
	}

	/* First, pre-route. (handler return value is ignored) */
	for (mh = gnr__msghandlers[GNR_MSG_MSGHANDLER_STAGE_PREROUTING]; mh; mh = mh->next) {
		mh->handlerfunc(mh->module, GNR_MSG_MSGHANDLER_STAGE_PREROUTING, gm, &gmhi);
	}

	/* Next, route. */
	for (mh = gnr__msghandlers[GNR_MSG_MSGHANDLER_STAGE_ROUTING]; mh; mh = mh->next) {
		if (mh->handlerfunc(mh->module, GNR_MSG_MSGHANDLER_STAGE_ROUTING, gm, &gmhi)) {
			gmhi.targetmod = mh->module;
			break;
		}
	}

	/* Then post-route. */
	for (mh = gnr__msghandlers[GNR_MSG_MSGHANDLER_STAGE_POSTROUTING]; mh; mh = mh->next) {
		mh->handlerfunc(mh->module, GNR_MSG_MSGHANDLER_STAGE_POSTROUTING, gm, &gmhi);
	}

	/* And finally, output. */
	if (gmhi.targetmod) {
		gnrmsg_outputfunc_t outf = NULL;
		
		naf_module_tag_fetch(gnr__module, gmhi.targetmod, "module.gnrmsg_outputfunc", NULL, (void **)&outf);
		
		if (outf)
			outf(gmhi.targetmod, gm, &gmhi);
	}
 
	return 0;
}


int gnr_msg__register(struct nafmodule *mod)
{

	memset(gnr__msghandlers, 0, sizeof(struct mhlist)*(GNR_MSG_MSGHANDLER_STAGE_MAX+1));

	naf_rpc_register_method(mod, "listmsghandlers", __rpc_gnr_listmsghandlers, "List registered message handlers");

	return 0;
}

int gnr_msg__unregister(struct nafmodule *mod)
{

	naf_rpc_unregister_method(mod, "listmsghandlers");

	return 0;
}


int gnr_msg_register(struct nafmodule *mod, gnrmsg_outputfunc_t outputfunc)
{

	if (!mod)
		return -1;

	/*
	 * Passive plugins (logging, stats, etc) don't need an outputfunc.
	 */
	if (outputfunc) {
		if (naf_module_tag_add(gnr__module, mod, "module.gnrmsg_outputfunc", 'V', (void *)outputfunc) == -1)
			return -1;
	}

	return 0;
}

int gnr_msg_unregister(struct nafmodule *mod)
{
	void *outputfunc;

	naf_module_tag_remove(gnr__module, mod, "module.gnrmsg_outputfunc", NULL, (void **)&outputfunc);

	return 0;
}



