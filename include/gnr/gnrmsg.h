
#ifndef __GNRMSG_H__
#define __GNRMSG_H__

#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <gnr/gnrnode.h>

typedef naf_u16_t gnrmsg_msgtype_t;
#define GNR_MSG_MSGTYPE_IM           (gnrmsg_msgtype_t) 0x0000
#define GNR_MSG_MSGTYPE_GROUPINVITE  (gnrmsg_msgtype_t) 0x0001
#define GNR_MSG_MSGTYPE_GROUPIM      (gnrmsg_msgtype_t) 0x0002
#define GNR_MSG_MSGTYPE_GROUPJOIN    (gnrmsg_msgtype_t) 0x0003
#define GNR_MSG_MSGTYPE_GROUPPART    (gnrmsg_msgtype_t) 0x0004
#define GNR_MSG_MSGTYPE_RENDEZVOUS   (gnrmsg_msgtype_t) 0x0005

typedef naf_u32_t gnrmsg_msgflag_t;
#define GNR_MSG_MSGFLAG_NONE         (gnrmsg_msgflag_t) 0x00000000
#define GNR_MSG_MSGFLAG_AUTORESPONSE (gnrmsg_msgflag_t) 0x00000001
#define GNR_MSG_MSGFLAG_ACKREQUESTED (gnrmsg_msgflag_t) 0x00000002
#define GNR_MSG_MSGFLAG_SWAPSRCDEST  (gnrmsg_msgflag_t) 0x00000010 /* srcname and destname are swapped (really only applies to peer routing) */
#define GNR_MSG_MSGFLAG_METAMESSAGE  (gnrmsg_msgflag_t) 0x00000040 /* "user typing", etc */

typedef naf_u16_t gnrmsg_routeflag_t;
#define GNR_MSG_ROUTEFLAG_NONE           (gnrmsg_routeflag_t) 0x00000000
#define GNR_MSG_ROUTEFLAG_ROUTED_LOCAL   (gnrmsg_routeflag_t) 0x00000001
#define GNR_MSG_ROUTEFLAG_ROUTED_PEER    (gnrmsg_routeflag_t) 0x00000002
#define GNR_MSG_ROUTEFLAG_ROUTED_FORWARD (gnrmsg_routeflag_t) 0x00000004
#define GNR_MSG_ROUTEFLAG_ROUTED_INTERNAL (gnrmsg_routeflag_t)0x00000008
#define GNR_MSG_ROUTEFLAG_DROPPED        (gnrmsg_routeflag_t) 0x00000010
#define GNR_MSG_ROUTEFLAG_DELAYED        (gnrmsg_routeflag_t) 0x00000020 /* will be rerouted later */

/* mask for any of the valid routed flags */
#define GNR_MSG_ROUTEFLAG_ROUTED       (GNR_MSG_ROUTEFLAG_ROUTED_LOCAL | \
					GNR_MSG_ROUTEFLAG_ROUTED_PEER | \
					GNR_MSG_ROUTEFLAG_ROUTED_FORWARD | \
					GNR_MSG_ROUTEFLAG_ROUTED_INTERNAL)
#define GNR_MSG_ROUTEFLAG_ISROUTED(x)  (!!((x) & GNR_MSG_ROUTEFLAG_ROUTED))


struct gnrmsg; /* below */
typedef void *(*gnrmsg_msgbuf_clonefunc_t)(struct gnrmsg *gm);
typedef void (*gnrmsg_msgbuf_freefunc_t)(struct gnrmsg *gm);

struct gnrmsg {

	/* Source node and destination node (in alphanumeric form) */
	char *srcname;
	char *srcnameservice;
	char *destname;
	char *destnameservice;

	/* Message type (GNR_MSG_MSGTYPE_) */
	gnrmsg_msgtype_t type;

	/* 
	 * msgbuf/msgbuflen are message/service-specific and are not
	 * touched by any of the internal routing routines.
	 *
	 * It is optional, but this can contain more detailed message
	 * information, hints, etc.  For cases where messages are delayed or
	 * otherwise need duplication, the msgbuf_clonefunc and msgbuf_freefunc
	 * fields must be filled in if msgbuf is non-NULL.
 	 */
	void *msgbuf;
	size_t msgbuflen;
	gnrmsg_msgbuf_clonefunc_t msgbuf_clonefunc;
	gnrmsg_msgbuf_freefunc_t msgbuf_freefunc;

	/*
	 * msgstring must be a human-readable representation of the message
	 * being routed.
 	 */
	char *msgstring;

	gnrmsg_msgflag_t msgflags;
	gnrmsg_routeflag_t routeflags;
	char *groupname; /* if applicable */

	/* Where the message came from (optional; used for hinting) */
	struct nafconn *srcconn;

	void *taglist;

};

struct gnrmsg *gnr_msg_new(struct nafmodule *mod);
void gnr_msg_free(struct nafmodule *mod, struct gnrmsg *gm);

int gnr_msg_route(struct nafmodule *srcmod, struct gnrmsg *gm);

int gnr_msg_tag_add(struct nafmodule *mod, struct gnrmsg *gm, const char *name, char type, void *data);
int gnr_msg_tag_remove(struct nafmodule *mod, struct gnrmsg *gm, const char *name, char *typeret, void **dataret);
int gnr_msg_tag_ispresent(struct nafmodule *mod, struct gnrmsg *gm, const char *name);
int gnr_msg_tag_fetch(struct nafmodule *mod, struct gnrmsg *gm, const char *name, char *typeret, void **dataret);
int gnr_msg_clonetags(struct gnrmsg *destgm, struct gnrmsg *srcgm);


struct gnrmsg_handler_info {
	struct nafmodule *srcmod;
	struct nafconn *destconn;
	struct nafmodule *targetmod; /* result of routing stage */
	struct gnrnode *srcnode; /* gnrnode for message 'source' */
	struct gnrnode *destnode; /* gnrnode for message 'destination' */
	/* XXX should support tags here? (better place than in gnrmsg itself */
};

/*
 * There are various stages of message route decision making.  All callbacks
 * take the same form.
 */
typedef int (*gnrmsg_msghandlerfunc_t)(struct nafmodule *mod, int stage, struct gnrmsg *gm, struct gnrmsg_handler_info *hinfo);

/*
 *  Pre-routing: when the message first enters into the system; all event
 *    handlers must return the message unhandled. (Mainly for logging and 
 *    other things that need to see every message that goes through.)
 */
#define GNR_MSG_MSGHANDLER_STAGE_PREROUTING  0
/*
 *  Routing: Pass to everyone and make a decision about routing.  The first
 *    handler to returned handled sets the conn it wants the message to go
 *    to and returns. (Theres still some semantics that need defining here,
 *    particularly in terms of a route precidence system.  Perhaps do some
 *    scoring?)
 */
#define GNR_MSG_MSGHANDLER_STAGE_ROUTING     1
/*
 *  Post-routing: when the message has a route and is about to leave the
 *    system.  (This is also mainly for logging purposes and as a last-
 *    chance to keep messages from escaping to the network at large.)
 */
#define GNR_MSG_MSGHANDLER_STAGE_POSTROUTING 2

#define GNR_MSG_MSGHANDLER_STAGE_MAX         GNR_MSG_MSGHANDLER_STAGE_POSTROUTING

/* Absolute order of handlers */
#define GNR_MSG_MSGHANDLER_POS_MIN 0
#define GNR_MSG_MSGHANDLER_POS_MID 50
#define GNR_MSG_MSGHANDLER_POS_MAX 100

int gnr_msg_addmsghandler(struct nafmodule *mod, int stage, int position, gnrmsg_msghandlerfunc_t handlerfunc, const char *desc);
int gnr_msg_remmsghandler(struct nafmodule *mod, int stage, gnrmsg_msghandlerfunc_t handlerfunc);

/*
 * After a message is assigned a route, the outputfunc of the target's owner
 * module is called. It must then put the message onto the wire (or arrange to
 * have it put there) and return handled.  (Again, minor semantics need to be
 * defined for, as an example, if the output function returns unhandled: is the
 * message dropped or is it bounced in some way?)
 */
typedef int (*gnrmsg_outputfunc_t)(struct nafmodule *mod, struct gnrmsg *gm, struct gnrmsg_handler_info *hinfo);


/*
 * Modules that plan on making use of the gnr system must maintain a
 * registration.
 */
int gnr_msg_register(struct nafmodule *mod, gnrmsg_outputfunc_t outputfunc);
int gnr_msg_unregister(struct nafmodule *mod);


#endif /* ndef __GNRMSG_H__ */

