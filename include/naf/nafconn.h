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

#ifndef __NAFCONN_H__
#define __NAFCONN_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#include <naf/nafmodule.h>
#include <naf/naftypes.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <libnbio.h> /* for nbio_fd_t definition */

/*
 * Connection's data type (exactly one of these)
 *
 * XXX these should be refielded as a 'protocol' type.
 *
 * XXX This exposes too much, and if nothing else, is a very finite namespace.
 */
#define NAF_CONN_TYPE_DETECTING   0x00000001 /* Unknown condition */
#define NAF_CONN_TYPE_FLAP        0x00000002 /* src/oscar/ */
#define NAF_CONN_TYPE_APIP        0x00000004 /* src/apip/ */
#define NAF_CONN_TYPE_MSNP        0x00000008 /* src/msnp/ */
#define NAF_CONN_TYPE_YAHOO       0x00000010 /* src/yahoo/ */
#define NAF_CONN_TYPE_IRC         0x00000020 /* src/irc/ */
#define NAF_CONN_TYPE_ABFLAP      0x00000040 /* src/abp.c (obsolete) */
#define NAF_CONN_TYPE_ABROUTER    0x00000080 /* plugins/abrouter/ */
#define NAF_CONN_TYPE_ABDPCON     0x00000100 /* plugins/abdpcon/ */
#define NAF_CONN_TYPE_FLAPSTANDALONE 0x00000200 /* plugins/apaimclient/ */
#define NAF_CONN_TYPE_DSR         0x00000400 /* plugins/abbscs/ */
#define NAF_CONN_TYPE_SOAP        0x00000800 /* src/soap.c (http) */
#define NAF_CONN_TYPE_SSH         0x00001000 /* src/ssh/ */

/* Endpoint connection mechanism (exactly one of these) */
#define NAF_CONN_TYPE_SOCKS       0x00010000 /* Connected using SOCKS */
#define NAF_CONN_TYPE_RAW         0x00020000 /* Connecting using raw protocol */
#define NAF_CONN_TYPE_LOCAL       0x00040000 /* Locally serviced connection */


/* Local or remote (exactly one of these) */
#define NAF_CONN_TYPE_CLIENT      0x00100000 /* Client connected to us */
#define NAF_CONN_TYPE_SERVER      0x00200000 /* Server we connected to */
#define NAF_CONN_TYPE_LISTENER    0x00400000 /* Listener opened by us */
#define NAF_CONN_TYPE_DATAGRAM    0x00800000

/* State (any number of these) */
#define NAF_CONN_TYPE_CONNECTING  0x01000000 /* during non-blocking connect */
#define NAF_CONN_TYPE_RAWWAITING  0x02000000 /* RAW waiting for recognition */
#define NAF_CONN_TYPE_READRAW     0x04000000

#define NAF_CONN_TYPE_PROROGUEDEATH 0x08000000 /* Don't kill when endpoint dies */


typedef naf_u32_t naf_conn_cid_t;

struct nafconn {
	nbio_fd_t *fdt;
	naf_u32_t type;
	naf_u32_t state;
	naf_u32_t flags;
	time_t lastrx; /* only used for connection type assumptions */
	time_t lastrx2; /* used for timing out the RAWWAITING condition */
	struct nafconn *endpoint;
	struct sockaddr_in remoteendpoint; /* address of remote host */
	struct sockaddr_in localendpoint; /* address of local interface */

	/*
	 * In some cases, there is a parental relationship between connections.
	 */
	struct nafconn *parent;

	int servtype; /* deprecated? */
	void *taglist;

	/* XXX this needs to go in a protocol-specific section */
	naf_u16_t nextseqnum;

	naf_u8_t *waitingbuf;
	int waitingbuflen;

	struct nafmodule *owner;

	/*
	 * Connection ID. This should be unique over a fairly long period,
	 * but should not be assumed monotonic.
	 *
	 */
	naf_conn_cid_t cid;
};

int naf_conn_schedulekill(struct nafconn *conn);
struct nafconn *naf_conn_getparent(struct nafconn *conn);
int naf_conn_reqread(struct nafconn *conn, unsigned char *buf, int buflen, int offset);
int naf_conn_reqwrite(struct nafconn *conn, unsigned char *buf, int buflen);
int naf_conn_takeread(struct nafconn *conn, unsigned char **bufp, int *buflenp);
int naf_conn_takewrite(struct nafconn *conn, unsigned char **bufp, int *buflenp);
int naf_conn_setdelim(struct nafmodule *mod, struct nafconn *conn, const unsigned char *delim, const unsigned char delimlen);
void naf_conn_setraw(struct nafconn *conn, int val);

/*
 * This iterates over the connection list, calling the provided matcher
 * function for each connection.  If the matcher returns nonzero, iteration
 * stops and the current connection is returned.  'data' is passed untouched
 * to the matcher function on each invocation.
 */
struct nafconn *naf_conn_find(struct nafmodule *mod, int (*matcher)(struct nafmodule *, struct nafconn *, const void *), const void *data);
struct nafconn *naf_conn_findbycid(struct nafmodule *mod, naf_conn_cid_t cid);

int naf_conn_startconnect(struct nafmodule *mod, struct nafconn *localconn, const char *host, int port);
struct nafconn *naf_conn_addconn(struct nafmodule *mod, nbio_sockfd_t sfd, naf_u32_t type);

char *naf_conn_getlocaladdrstr(struct nafmodule *mod, struct nafconn *conn);

/*
 * Tagging is a method for module to use when they want to attach
 * information to a connection without ever touching the connlist.  Tags are
 * indexed by a module/name pair, so that several module can have tags
 * with the same name attached to the same connection (this prevents modules
 * from having to guard against each other).
 *
 * Use naf_conn_tag_add() and naf_conn_tag_remove() to add and remove tags from
 * a connection.  The naf_conn_tag_ispresent() predicate will return whether or
 * not a tag is attached to a connection.  Finally, naf_conn_tag_fetch() will
 * retrieve a tag's data.
 *
 * Also, when a connection is being killed, the owner module of each tag will
 * have its ->freetag function called, at which point it should free any data
 * contained in the tag (provided as arguments to the function).
 *
 * Note that tagging should be considered a heavy-weight operation, and only
 * used when actually necessary.
 *
 * There are a few standard (but not enforced) conventions for tags.  First,
 * the name should specify what kind of object to which it is attached. For
 * example, a connection tag should have a name that starts with "conn.".
 * Similiarly, a module tag should have a name that starts with "module.".
 * Also, although tag types can be anything, this list should be considered
 * what other modules expect to see:
 *     'I'  -  Integer (not pointer to integer)
 *     'S'  -  NULL-terminated string
 *     'V'  -  Generic pointer
 * Some modules will rely on this for logging.
 *
 */
int naf_conn_tag_add(struct nafmodule *mod, struct nafconn *conn, const char *name, char type, void *data);
int naf_conn_tag_remove(struct nafmodule *mod, struct nafconn *conn, const char *name, char *typeret, void **dataret);
int naf_conn_tag_ispresent(struct nafmodule *mod, struct nafconn *conn, const char *name);
int naf_conn_tag_fetch(struct nafmodule *mod, struct nafconn *conn, const char *name, char *typeret, void **dataret);

#endif /* __NAFCONN_H__ */

