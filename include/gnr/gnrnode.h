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

#ifndef __GNRNODE_H__
#define __GNRNODE_H__

/*
 * gnrnode structures are kept for three types of nodes:
 *
 *   1) Directly-connected nodes: these nodes have their connection
 *      maintained by this instance of gnr/etc.  Their metric is exactly
 *      zero and the lifetime of the gnrnode is equal to the lifetime of the
 *      connection.
 *   2) Peer-connected nodes: these nodes are known to be available through
 *      connected peers, although they may not be _all_ nodes available through
 *      peers.  A gnrnode is created after a successful FINDNODE operation (in
 *      the case of TSRP) and lasts for some configured timeout after the last
 *      message is transacted with that node.  Their metric is greater than
 *      zero but less than GNR_NODE_METRIC_MAX.
 *   3) External nodes: these nodes are known but have no direct connection
 *      to the peer network.  Messages are still transacted with these nodes,
 *      however the only routes available are those gained indirectly through
 *      local nodes.  The gnrnode has a lifetime similar in semantics to that
 *      of peered nodes.  Their metric is exactly GNR_NODE_METRIC_MAX -- that
 *      is, the metric is infinite.  (In TSRP, these are "negative records" in
 *      that they are created if a FINDNODE fails.  Also, they can be created
 *      when a message is received through an indirect channel.)
 *
 * gnrnodes are unique based on the name/service pair.  Case and spacing are
 * ignored.
 */
struct gnrnode {
#define GNR_NODE_NAME_MAXLEN 128
	char *name;
	char *service;
#define GNR_NODE_FLAG_NONE      0x00000000
#define GNR_NODE_FLAG_LOCALONLY 0x00000001 /* do not disclose to peers */
	naf_u32_t flags;
#define GNR_NODE_METRIC_LOCAL       0
#define GNR_NODE_METRIC_MAX     65535
#define GNR_NODE_METRIC_ISPEERED(x) (((x) > GNR_NODE_METRIC_LOCAL) && \
						((x) < GNR_NODE_METRIC_MAX))
	int metric; /* =0 direct, 1<=x<MAX peer, =MAX external */
	struct nafmodule *ownermod;
	void *taglistv; /* naf_tag_t */

	/*
	 * Ephemeral gnrnodes will still be kept around as long as refcount is
	 * nonzero (except for bug-detection timeouts).
	 */
	int refcount;

	/*
	 * These are only maintained and used when ->metric > 0 (peered and
	 * external nodes).
	 */
	time_t lastuse; /* last message to/from this node */
#define GNR_NODE_TTL_DEFAULT 30
	int ttl; /* num of seconds to keep record after lastuse */

	time_t createtime;

	struct gnrnode *next;
};


struct gnr_event_ei_nodechange {
#define GNR_NODE_OFFLINE_REASON_UNKNOWN 0
#define GNR_NODE_OFFLINE_REASON_TIMEOUT 1
#define GNR_NODE_OFFLINE_REASON_DISCONNECTED 2
	naf_u32_t reason;
};


int gnr_node_offline(struct gnrnode *gn, int reason);
struct gnrnode *gnr_node_findbyname(const char *name, const char *service);
struct gnrnode *gnr_node_online(struct nafmodule *mod, const char *name, const char *service, naf_u32_t flags, int metric);

/*
 * !!! Avoid using these !!!
 */
/* Iterate through all nodes */
struct gnrnode *gnr_node_find(struct nafmodule *mod, int (*matcher)(struct nafmodule *, struct gnrnode *, const void *), const void *data);
/* Iterate through all nodes; mark offline if handler returns non-zero */
void gnr_node_offline_many(struct nafmodule *mod, int (*matcher)(struct nafmodule *, struct gnrnode *, const void *), const void *data, int reason);

int gnr_node_namecmp(const char *n1, const char *n2);
int gnr_node_softeq(struct gnrnode *gn1, struct gnrnode *gn2);
void gnr_node_away(struct gnrnode *gn, int val);
void gnr_node_idle(struct gnrnode *gn, int seconds);
void gnr_node_usehit(struct gnrnode *gn);
int gnr_node_remetric(struct gnrnode *gn, int newmetric);
int gnr_node_setttl(struct gnrnode *gn, int newttl);
void gnr_node_ref(struct nafmodule *mod, struct gnrnode *gn);
void gnr_node_unref(struct nafmodule *mod, struct gnrnode *gn);

int gnr_node_tag_add(struct nafmodule *mod, struct gnrnode *gn, const char *name, char type, void *data);
int gnr_node_tag_remove(struct nafmodule *mod, struct gnrnode *gn, const char *name, char *typeret, void **dataret);
int gnr_node_tag_ispresent(struct nafmodule *mod, struct gnrnode *gn, const char *name);
int gnr_node_tag_fetch(struct nafmodule *mod, struct gnrnode *gn, const char *name, char *typeret, void **dataret);

#endif /* __GNRNODE_H__ */

