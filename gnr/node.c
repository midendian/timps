
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> /* tolower() */
#include <time.h>

#include <naf/nafmodule.h>
#include <naf/nafrpc.h>
#include <naf/nafstats.h>
#include <naf/naftag.h>

#include <gnr/gnrnode.h>
#include <gnr/gnrmsg.h>
#include <gnr/gnrevents.h>
#include "core.h"


int gnr_node_tag_add(struct nafmodule *mod, struct gnrnode *gn, const char *name, char type, void *data)
{

	if (!gn)
		return -1;

	return naf_tag_add(&gn->taglistv, mod, name, type, data);
}

int gnr_node_tag_remove(struct nafmodule *mod, struct gnrnode *gn, const char *name, char *typeret, void **dataret)
{

	if (!gn)
		return -1;

	return naf_tag_remove(&gn->taglistv, mod, name, typeret, dataret);
}

int gnr_node_tag_ispresent(struct nafmodule *mod, struct gnrnode *gn, const char *name)
{

	if (!gn)
		return -1;

	return naf_tag_ispresent(&gn->taglistv, mod, name);
}

int gnr_node_tag_fetch(struct nafmodule *mod, struct gnrnode *gn, const char *name, char *typeret, void **dataret)
{

	if (!gn)
		return -1;

	return naf_tag_fetch(&gn->taglistv, (void *)gn, name, typeret, dataret);
}


static int gnr_event_throw_withnode(gnr_event_t ev, struct gnrnode *node, struct gnr_event_ei_nodechange *ei)
{
	struct gnr_event_info gei;

	if (!node)
		return -1;

	memset(&gei, 0, sizeof(struct gnr_event_info));
	gei.gei_event = ev;
	gei.gei_node = node;
	gei.gei_extinfo = (void *)ei;

	return gnr_event_throw(&gei);
}


static struct {
	naf_longstat_t total;
	naf_longstat_t local;
	naf_longstat_t peered;
	naf_longstat_t external;
} gnr__nodestats;


#define GNR_NODE_HASH_SIZE 64
static struct gnrnode *gnr__nodehash[GNR_NODE_HASH_SIZE];
typedef naf_u16_t gnr_nhkey_t;

static gnr_nhkey_t gnr_node__hash_getkey(const char *name)
{
	gnr_nhkey_t key;

	for (key = 0; *name; name++) {
		if (*name != ' ') {
			key ^= (gnr_nhkey_t)tolower(*name);
			key <<= 1;
		}
	}

	return key;
}

static int gnr_node__hash_getidx(const char *name)
{
	return gnr_node__hash_getkey(name) % GNR_NODE_HASH_SIZE;
}

static void gnr_node__hash_init(void)
{
	int i;

	for (i = 0; i < GNR_NODE_HASH_SIZE; i++)
		gnr__nodehash[i] = NULL;

	return;
}

static int gnr_node__hash_add(struct gnrnode *gn)
{
	int idx;

	idx = gnr_node__hash_getidx(gn->name);

	gn->next = gnr__nodehash[idx];
	gnr__nodehash[idx] = gn;

	return 0;
}

static struct gnrnode *gnr_node__hash_remove(struct gnrnode *gn)
{
	int idx;
	struct gnrnode *cur, **prev;

	idx = gnr_node__hash_getidx(gn->name);

	for (prev = &gnr__nodehash[idx]; (cur = *prev); ) {

		if (cur == gn) {
			*prev = cur->next;
			return cur;
		}

		prev = &cur->next;
	}

	return NULL;
}

static void freenode(struct gnrnode *gn, int reason)
{

	{
		struct gnr_event_ei_nodechange ei;

		memset(&ei, 0, sizeof(struct gnr_event_ei_nodechange));

		ei.reason = reason;

		gnr_event_throw_withnode(GNR_EVENT_NODEDOWN, gn, &ei);
	}

#if 0
	dvprintf(gnr__module, "user offline: %s[%s][%s][%s%s%s] -- %s%s%s\n",
			gn->name,
			gn->service,
			gn->ownermod ? gn->ownermod->name : "unknown",
			(gn->metric == GNR_NODE_METRIC_LOCAL) ? "local" : "",
			GNR_NODE_METRIC_ISPEERED(gn->metric) ? "peer" : "",
			(gn->metric == GNR_NODE_METRIC_MAX) ? "remote" : "",
			(reason == GNR_NODE_OFFLINE_REASON_TIMEOUT) ? "timed out" : "",
			(reason == GNR_NODE_OFFLINE_REASON_DISCONNECTED) ? "disconnected" : "",
			((reason != GNR_NODE_OFFLINE_REASON_TIMEOUT) && (reason != GNR_NODE_OFFLINE_REASON_DISCONNECTED)) ? "unknown reason" : "");
#endif

	gnr__nodestats.total--;
	if (gn->metric == GNR_NODE_METRIC_LOCAL)
		gnr__nodestats.local--;
	else if (gn->metric == GNR_NODE_METRIC_MAX)
		gnr__nodestats.external--;
	else
		gnr__nodestats.peered--;

	naf_tag_freelist(&gn->taglistv, gn);
	naf_free(gnr__module, gn->name);
	naf_free(gnr__module, gn->service);
	naf_free(gnr__module, gn);

	return;
}

static void gnr_node__hash_timeout(void)
{
	int idx;
	time_t now;

	now = time(NULL);

	for (idx = 0; idx < GNR_NODE_HASH_SIZE; idx++) {
		struct gnrnode *cur, **prev;

		for (prev = &gnr__nodehash[idx]; (cur = *prev); ) {

			if (cur->metric == GNR_NODE_METRIC_LOCAL) {
				/* local nodes never time out */
				prev = &cur->next;
				continue;
			}

			if ((cur->ttl == -1) ||
					(cur->refcount > 0) ||
					((now - cur->lastuse) < cur->ttl)) {
				prev = &cur->next;
				continue;
			}

			*prev = cur->next;

			freenode(cur, GNR_NODE_OFFLINE_REASON_TIMEOUT);
		}
	}

	return;
}

static void gnr_node__hash_free(void)
{
	int i;

	for (i = 0; i < GNR_NODE_HASH_SIZE; i++) {
		struct gnrnode *cur;

		for (cur = gnr__nodehash[i]; cur; ) {
			struct gnrnode *tmp;

			tmp = cur->next;
			freenode(cur, GNR_NODE_OFFLINE_REASON_UNKNOWN);
			cur = tmp;
		}
		gnr__nodehash[i] = NULL;
	}

	return;
}

/*
 * Ignores space and case.
 *
 * This really has nothing to do with gnrnode's.
 */
int gnr_node_namecmp(const char *sn1, const char *sn2)
{
	const char *curPtr1, *curPtr2;

	curPtr1 = sn1;
	curPtr2 = sn2;
	for (curPtr1 = sn1, curPtr2 = sn2;
			(*curPtr1 != (char) NULL) &&
			(*curPtr2 != (char) NULL); ) {
		if ( (*curPtr1 == ' ') || (*curPtr2 == ' ') ) {
			if (*curPtr1 == ' ')
				curPtr1++;
			if (*curPtr2 == ' ')
				curPtr2++;
		} else {
			if (toupper(*curPtr1) != toupper(*curPtr2))
				return 1;
			curPtr1++;
			curPtr2++;
		}
	}

	/* Should both be NULL */
	if (*curPtr1 != *curPtr2)
		return 1;

	return 0;
}

/*
 * Really awful kind of thing to export (for performance reasons), but eh.
 */
struct gnrnode *gnr_node_find(struct nafmodule *mod, int (*matcher)(struct nafmodule *, struct gnrnode *, const void *), const void *data)
{
	int idx;

	if (!matcher)
		return NULL;

	for (idx = 0; idx < GNR_NODE_HASH_SIZE; idx++) {
		struct gnrnode *gn;

		for (gn = gnr__nodehash[idx]; gn; gn = gn->next) {
			if (matcher(mod, gn, data))
				return gn;
		}
	}

	return NULL;
}

/*
 * Also annoying to export, for same reason as gnr_node_find().
 */
void gnr_node_offline_many(struct nafmodule *mod, int (*matcher)(struct nafmodule *, struct gnrnode *, const void *), const void *data, int reason)
{
	int idx;

	if (!matcher)
		return;

	for (idx = 0; idx < GNR_NODE_HASH_SIZE; idx++) {
		struct gnrnode *cur, **prev;

		for (prev = &gnr__nodehash[idx]; (cur = *prev); ) {

			if (matcher(mod, cur, data)) {
				*prev = cur->next;
				freenode(cur, reason);
			} else
				prev = &cur->next;
		}
	}

	return;
}

struct gnrnode *gnr_node_findbyname(const char *name, const char *service)
{
	int idx;
	struct gnrnode *gn;

	if (!name)
		return NULL;

	idx = gnr_node__hash_getidx(name);

	for (gn = gnr__nodehash[idx]; gn; gn = gn->next) {

		if (gnr_node_namecmp(gn->name, name) == 0) {

			/* If service isn't specified, match on any */
			if (service && (strcasecmp(gn->service, service) != 0))
				continue;

			return gn;
		}
	}

	return NULL;
}

struct gnrnode *gnr_node_online(struct nafmodule *owner, const char *name, const char *service, naf_u32_t flags, int metric)
{
	struct gnrnode *gn;

	if (!owner || !name || !service)
		return NULL;

	if ((gn = gnr_node_findbyname(name, service)))
		return gn;

	if (!(gn = naf_malloc(gnr__module, sizeof(struct gnrnode))))
		return NULL;
	memset(gn, 0, sizeof(struct gnrnode));

	if (!(gn->name = naf_strdup(gnr__module, name))) {
		naf_free(gnr__module, gn);
		return NULL;
	}
	if (!(gn->service = naf_strdup(gnr__module, service))) {
		naf_free(gnr__module, gn->name);
		naf_free(gnr__module, gn);
		return NULL;
	}
	gn->flags = flags;
	gn->metric = metric;
	gn->ownermod = owner;
	gn->taglistv = NULL;
	gn->refcount = 0; /* caller should immediatly _ref if it wants it */
	gn->createtime = gn->lastuse = time(NULL);
	gn->ttl = -1;

	if (gn->metric > GNR_NODE_METRIC_LOCAL)
		gn->ttl = GNR_NODE_TTL_DEFAULT;

	gnr_node__hash_add(gn);

	gnr__nodestats.total++;
	if (gn->metric == GNR_NODE_METRIC_LOCAL)
		gnr__nodestats.local++;
	else if (gn->metric == GNR_NODE_METRIC_MAX)
		gnr__nodestats.external++;
	else
		gnr__nodestats.peered++;

#if 0
	dvprintf(gnr__module, "user online: %s[%s][%s][%s%s%s]\n",
			gn->name,
			gn->service,
			gn->ownermod ? gn->ownermod->name : "unknown",
			(gn->metric == GNR_NODE_METRIC_LOCAL) ? "local" : "",
			GNR_NODE_METRIC_ISPEERED(gn->metric) ? "peer" : "",
			(gn->metric == GNR_NODE_METRIC_MAX) ? "remote" : "");
#endif

	gnr_event_throw_withnode(GNR_EVENT_NODEUP, gn, NULL);

	return gn;
}

/* XXX make this static? */
void gnr_node_usehit(struct gnrnode *gn)
{

	if (!gn)
		return;

	gn->lastuse = time(NULL);

	return;
}

/* Remetric'ing is a complicated thing. It's probably broken. */
int gnr_node_remetric(struct gnrnode *gn, int newmetric)
{

	if (!gn)
		return -1;

	if (gn->metric < newmetric)
		dvprintf(gnr__module, "BUG: gnr_node_remetric asked to make node %s[%s] farther away (new = %d, old = %d)\n", gn->name, gn->service, newmetric, gn->metric);


	if (gn->metric == GNR_NODE_METRIC_LOCAL)
		gnr__nodestats.local--;
	else if (gn->metric == GNR_NODE_METRIC_MAX)
		gnr__nodestats.external--;
	else
		gnr__nodestats.peered--;


	gn->metric = newmetric;


	if (gn->metric == GNR_NODE_METRIC_LOCAL)
		gnr__nodestats.local++;
	else if (gn->metric == GNR_NODE_METRIC_MAX)
		gnr__nodestats.external++;
	else
		gnr__nodestats.peered++;


	if (gn->metric == GNR_NODE_METRIC_LOCAL)
		gn->ttl = -1;
	else
		gn->ttl = GNR_NODE_TTL_DEFAULT;


	gnr_node_usehit(gn);

	return 0;
}

int gnr_node_setttl(struct gnrnode *gn, int newttl)
{

	if (!gn)
		return -1;

	if (gn->metric == GNR_NODE_METRIC_LOCAL)
		return 0;

	if (newttl == -1)
		gn->ttl = GNR_NODE_TTL_DEFAULT;
	else
		gn->ttl = newttl;

	return 0;
}

int gnr_node_offline(struct gnrnode *gn, int reason)
{

	if (gnr_node__hash_remove(gn) != gn)
		return -1; /* uhm. */

	freenode(gn, reason);

	return 0;
}

void gnr_node_ref(struct nafmodule *mod, struct gnrnode *gn)
{

	if (!gn)
		return;

	gn->refcount++;

	return;
}

void gnr_node_unref(struct nafmodule *mod, struct gnrnode *gn)
{

	if (!gn)
		return;

	gn->refcount--;

	return;
}


static int gnr_node__msg_postrouting(struct nafmodule *mod, int stage, struct gnrmsg *gm, struct gnrmsg_handler_info *hinfo)
{

	/* Only routed messages count towards timeout. */
	if (GNR_MSG_ROUTEFLAG_ISROUTED(gm->routeflags)) {

		if (hinfo->srcnode)
			gnr_node_usehit(hinfo->srcnode);

		if (hinfo->destnode)
			gnr_node_usehit(hinfo->destnode);

	}

	return 0;
}


static int putnodes_matcher(struct nafmodule *mod, void *udv, const char *tagname, char tagtype, void *tagdata)
{
	naf_rpc_arg_t **head = (naf_rpc_arg_t **)udv;

	if (tagtype == 'S')
		naf_rpc_addarg_string(mod, head, tagname, (char *)tagdata);
	else if (tagtype == 'I')
		naf_rpc_addarg_scalar(mod, head, tagname, (int)tagdata);

	return 0; /* keep going */
}

static int listnodes_putnodes(struct nafmodule *mod, naf_rpc_arg_t **head, struct gnrnode *nhead, int wt, int count)
{
	struct gnrnode *n;

	for (n = nhead; n && ((count > 0) || (count == -1)); n = n->next) {
		naf_rpc_arg_t **rnode;
		char ns[GNR_NODE_NAME_MAXLEN+GNR_NODE_NAME_MAXLEN+2+1];

		snprintf(ns, sizeof(ns), "%s[%s]", n->name, n->service);

		if ((rnode = naf_rpc_addarg_array(mod, head, ns))) {

			naf_rpc_addarg_string(mod, rnode, "name", n->name);
			naf_rpc_addarg_string(mod, rnode, "service", n->service);
			naf_rpc_addarg_string(mod, rnode, "owner", n->ownermod->name);
			naf_rpc_addarg_scalar(mod, rnode, "metric", n->metric);
			naf_rpc_addarg_scalar(mod, rnode, "refcount", n->refcount);
			if (n->metric != GNR_NODE_METRIC_LOCAL) {
				naf_rpc_addarg_scalar(mod, rnode, "ttl", n->ttl);
				naf_rpc_addarg_scalar(mod, rnode, "lastuse", n->lastuse);
			}

			if (wt) {
				naf_rpc_arg_t **tags;

				if ((tags = naf_rpc_addarg_array(mod, rnode, "tags"))) 
					naf_tag_iter(&n->taglistv, NULL, putnodes_matcher, (void *)tags);
			}
		}

		if (count != -1)
			count--;
	}

	return 0;
}

/*
 * gnr->listnodes()
 *   IN:
 *      [optional] bool wanttags;
 *      [optional] scalar index;
 *
 *   OUT:
 *      [optional] array nodes {
 *          array name[service] {
 *              string name;
 *              string service;
 *              string owner;
 *              scalar metric;
 *              scalar refcount;
 *              [optional] scalar ttl;
 *              [optional] scalar lastuse;
 *          }
 *      }
 */
static void __rpc_gnr_listnodes(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t **head, *idx, *wt;
	int wanttags = 0;
	int index = -1;


	if ((idx = naf_rpc_getarg(req->inargs, "index"))) {
		if (idx->type != NAF_RPC_ARGTYPE_SCALAR) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
		index = idx->data.scalar;

		if (index != -1) {
			if ((index >= GNR_NODE_HASH_SIZE) || (index < 0)) {
				req->status = NAF_RPC_STATUS_INVALIDARGS;
				return;
			}
		}
	}

	if ((wt = naf_rpc_getarg(req->inargs, "wanttags"))) {
		if (wt->type != NAF_RPC_ARGTYPE_BOOL) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
		wanttags = !!wt->data.boolean;
	}


	if (!(head = naf_rpc_addarg_array(mod, &req->returnargs, "nodes"))) {
		req->status = NAF_RPC_STATUS_UNKNOWNFAILURE;
		return;
	}

	if (index == -1) {
		for (index = 0; index < GNR_NODE_HASH_SIZE; index++)
			listnodes_putnodes(mod, head, gnr__nodehash[index], wanttags, -1);
	} else
		listnodes_putnodes(mod, head, gnr__nodehash[index], wanttags, -1);

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}

/*
 * gnr->getnodeinfo()
 *   IN:
 *      string name;
 *      [optional] string service;
 *
 *   OUT:
 *      [optional] array nodes {
 *          array name[service] {
 *              string name;
 *              string service;
 *              string owner;
 *              scalar metric;
 *              scalar refcount;
 *              [optional] scalar ttl;
 *              [optional] scalar lastuse;
 *          }
 *      }
 */
static void __rpc_gnr_getnodeinfo(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t **head, *name, *serv;
	struct gnrnode *gn;

	name = naf_rpc_getarg(req->inargs, "name");
	if (!name || (name->type != NAF_RPC_ARGTYPE_STRING)) {
		req->status = NAF_RPC_STATUS_INVALIDARGS;
		return;
	}

	if ((serv = naf_rpc_getarg(req->inargs, "service"))) {
		if (serv->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	if (!(head = naf_rpc_addarg_array(mod, &req->returnargs, "nodes"))) {
		req->status = NAF_RPC_STATUS_UNKNOWNFAILURE;
		return;
	}

	if ((gn = gnr_node_findbyname(name->data.string, serv ? serv->data.string : NULL)))
		listnodes_putnodes(mod, head, gn, 1, 1);

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}


void gnr_node__timeout(struct nafmodule *mod)
{

	gnr_node__hash_timeout();

	return;
}

int gnr_node__register(struct nafmodule *mod)
{

	gnr_node__hash_init();

	naf_stats_register_longstat(mod, "nodes.current.total", &gnr__nodestats.total);
	naf_stats_register_longstat(mod, "nodes.current.local", &gnr__nodestats.local);
	naf_stats_register_longstat(mod, "nodes.current.peered", &gnr__nodestats.peered);
	naf_stats_register_longstat(mod, "nodes.current.external", &gnr__nodestats.external);

	gnr_msg_addmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_POSTROUTING, GNR_MSG_MSGHANDLER_POS_MAX, gnr_node__msg_postrouting, "Update node statistics");

	naf_rpc_register_method(mod, "listnodes", __rpc_gnr_listnodes, "Retrieve node list and info");
	naf_rpc_register_method(mod, "getnodeinfo", __rpc_gnr_getnodeinfo, "Retrieve node info");

	return 0;
}

int gnr_node__unregister(struct nafmodule *mod)
{

	naf_rpc_unregister_method(mod, "listnodes");
	naf_rpc_unregister_method(mod, "getnodeinfo");

	gnr_msg_remmsghandler(mod, GNR_MSG_MSGHANDLER_STAGE_POSTROUTING, gnr_node__msg_postrouting);

	naf_stats_unregisterstat(mod, "nodes.current.total");
	naf_stats_unregisterstat(mod, "nodes.current.local");
	naf_stats_unregisterstat(mod, "nodes.current.peered");
	naf_stats_unregisterstat(mod, "nodes.current.external");

	gnr_node__hash_free();

	return 0;
}

