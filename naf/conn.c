
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>

#include <naf/nafmodule.h>
#include <naf/nafrpc.h>
#include <naf/nafconfig.h>
#include <naf/nafconn.h>
#include <naf/naftag.h>

#include "processes.h" /* for naf_childproc_cleanconn() */
#include "module.h" /* naf_module__protocoldetect() */

#define CONN_DEBUG_DEFAULT 0
static int conndebug = CONN_DEBUG_DEFAULT;


/* 
 * Number of seconds to wait for data while in DETECTING
 * state (before calling protocoldetecttimeout()). Must
 * be >= NAF_TIMER_ACCURACY.
 */
#define NAF_CONN_DETECT_TIMEOUT NAF_TIMER_ACCURACY


static void naf_conn_free(struct nafconn *conn);
static int finishconnect(struct nafconn *conn);

static nbio_t gnb;

static struct nafmodule *ourmodule = NULL;

/*
 * Incremented for every new connection. Wraps itself eventually.
 */
static naf_conn_cid_t conn_nextcid = 0;



int naf_conn_tag_add(struct nafmodule *mod, struct nafconn *conn, const char *name, char type, void *data)
{

	if (conndebug)
		dvprintf(ourmodule, "naf_conn_tag_add: module = %s, conn = %d, name = %s, type = %c, data = %p\n", mod->name, conn ? conn->cid : -1, name, type, data);

	if (!conn)
		return -1;

	return naf_tag_add(&conn->taglist, mod, name, type, data);
}

int naf_conn_tag_remove(struct nafmodule *mod, struct nafconn *conn, const char *name, char *typeret, void **dataret)
{

	if (conndebug)
		dvprintf(ourmodule, "naf_conn_tag_remove: module = %s, conn = %d, name = %s\n", mod->name, conn ? conn->cid : -1, name);

	if (!conn)
		return -1;

	return naf_tag_remove(&conn->taglist, mod, name, typeret, dataret);
}

int naf_conn_tag_ispresent(struct nafmodule *mod, struct nafconn *conn, const char *name)
{

	if (conndebug)
		dvprintf(ourmodule, "naf_conn_tag_ispresent: module = %s, conn = %d, name = %s\n", mod->name, conn ? conn->cid : -1, name);

	if (!conn)
		return -1;

	return naf_tag_ispresent(&conn->taglist, mod, name);
}

int naf_conn_tag_fetch(struct nafmodule *mod, struct nafconn *conn, const char *name, char *typeret, void **dataret)
{

	if (conndebug)
		dvprintf(ourmodule, "naf_conn_tag_fetch: module = %s, conn = %d, name = %s\n", mod->name, conn ? conn->cid : -1, name);

	if (!conn)
		return -1;

	return naf_tag_fetch(&conn->taglist, mod, name, typeret, dataret);
}



static const char *getcidstr(struct nafconn *conn)
{
	static char buf[32];

	snprintf(buf, sizeof(buf), "%lu", conn->cid);

	return buf;
}

/* XXX this isn't suitable for exporting, really... */
void naf_conn_setraw(struct nafconn *conn, int val)
{

	nbio_setraw(&gnb, conn->fdt, val);

	return;
}

static void closefdt(nbio_fd_t *fdt)
{
	unsigned char *buf;

	if (!fdt)
		return;

	while ((buf = nbio_remtoprxvector(&gnb, fdt, NULL, NULL)))
		naf_free(NULL, buf);
	while ((buf = nbio_remtoptxvector(&gnb, fdt, NULL, NULL)))
		naf_free(NULL, buf);

	fdt->priv = NULL;
	nbio_closefdt(&gnb, fdt);

	return;
}

/* 
 * This doesn't handle the case where cur is pointed to by a
 * connection's endpoint other than dead... that would turn into a
 * recursive mess. It'll will never happen anyway.  
 */
static void remendpoint(struct nafconn *dead)
{
	nbio_fd_t *fdt;

	/* so we don't find ourself... */
	dead->endpoint = NULL;

	for (fdt = gnb.fdlist; fdt; fdt = fdt->next) {
		struct nafconn *conn = (struct nafconn *)fdt->priv;

		if (fdt->fd == -1)
			continue;

		if (conn->endpoint == dead) {

			conn->endpoint = NULL;

			if (!((conn->type & NAF_CONN_TYPE_PROROGUEDEATH) ||
					 (conn->type & NAF_CONN_TYPE_LOCAL)))
				naf_conn_free(conn);

		}
	}

	return;
}

static void remchildren(struct nafconn *dead)
{
	nbio_fd_t *fdt;

	dead->parent = NULL;

	for (fdt = gnb.fdlist; fdt; fdt = fdt->next) {
		struct nafconn *conn = (struct nafconn *)fdt->priv;
	
		if (fdt->fd == -1)
			continue;

		if (conn->parent == dead) {

			conn->parent = NULL;

			if (!((conn->type & NAF_CONN_TYPE_PROROGUEDEATH) ||
					 (conn->type & NAF_CONN_TYPE_LOCAL)))
				naf_conn_free(conn);

		}
	}

	return;
}

/* 
 *
 * NEVER call this recursively and/or within a list traversal.
 *
 * XXX is this comment or the two functions previous more correct?
 *
 */
static void naf_conn_free(struct nafconn *dead)
{

	if (conndebug)
		dvprintf(ourmodule, "naf_conn_free(%p [cid %d])\n", dead, dead->cid);

	/* This does the very important step of setting fdt->priv to NULL */
	closefdt(dead->fdt);

	/* Call after its been removed from the list */
	if (dead->owner && dead->owner->connkill)
		dead->owner->connkill(dead->owner, dead);

	naf_tag_freelist(&dead->taglist, (void *)dead);

	/* 
	 * XXX There should be an event broadcast that says this connection is
	 * being killed, in case someone is holding a pointer to it.  That
	 * would get rid of these random calls to other modules.
	 */
	naf_childproc_cleanconn(dead);
	remchildren(dead);
	remendpoint(dead);

	naf_free(ourmodule, dead);

	return;
}

/* XXX this is a hack... a little too "general" */
struct nafconn *naf_conn_find(struct nafmodule *mod, int (*matcher)(struct nafmodule *, struct nafconn *, const void *), const void *data)
{
	nbio_fd_t *fdt;

	if (!mod || !matcher)
		return NULL;

	for (fdt = gnb.fdlist; fdt; fdt = fdt->next) {
		struct nafconn *conn = (struct nafconn *)fdt->priv;

		if (fdt->fd == -1)
			continue; /* closed connection */

		if (matcher(mod, conn, data))
			return conn;
	}

	return NULL;
}

static int findconnbycid_matcher(struct nafmodule *mod, struct nafconn *conn, const void *data)
{
	naf_conn_cid_t cid = (naf_conn_cid_t)data;

	return (conn->cid == cid);
}

struct nafconn *naf_conn_findbycid(struct nafmodule *mod, naf_conn_cid_t cid)
{
	return naf_conn_find(mod, findconnbycid_matcher, (const void *)cid);
}

/*
 * Really, everyone wanting to close a connection should use this, 
 * and only the core should call naf_conn_kill.  schedulekill is
 * more what you want anyway, since _kill will not write anything
 * you've requested written since the last naf_conn_poll().  But don't
 * read that as you should call naf_conn_poll, because you definitly
 * don't want to do that.
 *
 */
int naf_conn_schedulekill(struct nafconn *conn)
{

	if (!conn || !conn->fdt)
		return -1;

	return nbio_setcloseonflush(conn->fdt, 1);
}

static void updaterawmode(struct nafconn *conn); /* later */

static int getconnection(struct nafconn *listenconn)
{
	struct sockaddr sa;
	socklen_t salen = sizeof(sa);
	int clientfd;
	struct nafconn *conn;

	if ((clientfd = accept(listenconn->fdt->fd, &sa, &salen)) == -1) {

		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		/*
		 * On Linux, accept() returns pending network errors. Ignore
		 * them.
		 */
		if ((errno == ENETDOWN) || 
				(errno == EPROTO) ||
				(errno == ENOPROTOOPT) || 
				(errno == EHOSTDOWN) ||
				(errno == ENONET) || 
				(errno == EHOSTUNREACH) ||
				(errno == EOPNOTSUPP) || 
				(errno == ENETUNREACH)) {
			return 0;
		}

		if (errno == EMFILE) { /* too many open fds */
			dperror(ourmodule, "accept");
			return 0;
		}

		dperror(ourmodule, "accept");
		return -1;
	}

	if (!(conn = naf_conn_addconn(NULL, clientfd, NAF_CONN_TYPE_CLIENT|NAF_CONN_TYPE_DETECTING))) {
		dprintf(ourmodule, "error in addconn\n");
		close(clientfd);
		return 0; /* not fatal */
	}

	if (listenconn->owner) {

		if (conndebug > 0) {
			dvprintf(ourmodule, "[fd %d, cid %lu] forcing ownership to %s\n", conn->fdt->fd, conn->cid, listenconn->owner);
		}
		conn->owner = listenconn->owner;

		/*
		 * Allow the new owner to set up the connection flags.
		 */
		if (conn->owner->takeconn) {
			conn->owner->takeconn(conn->owner, conn);
			updaterawmode(conn); /* might have changed raw flags */
		}

	} else {

		/*
		 * The next step after getconnection() if the listener was not
		 * owned by a module is protocol detection, which requires raw
		 * mode.
		 *
		 * However, detecting is read-only. Manually shut off POLLOUT.
		 *
		 */
		naf_conn_setraw(conn, 2); /* XXX #define */
	}

	return clientfd;
}

static int connhandler_read(nbio_fd_t *fdt)
{
	struct nafconn *conn = (struct nafconn *)fdt->priv;

	if (conn->type & NAF_CONN_TYPE_CONNECTING) {
		/*
		 * XXX Check for connection errors.  Also, there could
		 * be real data waiting here, too, and since the poll()
		 * loop will call readability handlers before writability,
		 * there is a sort of race on connecting conn_t's when the
		 * connection goes through -- the readability handlers
		 * expect the code that transitions a nafconn from CONNECTING
		 * to normal (which is in the writability handler) to have
		 * been run already.  It's mostly safe just to ignore
		 * readability until a connecting socket has become writable.
		 */
		return 0;
	}

	if (conn->type & NAF_CONN_TYPE_LISTENER) {

		if (getconnection(conn) == -1) {
			dprintf(ourmodule, "getconn error\n");
			naf_conn_free(conn);
			/* XXX should try to reopen the port! */
		} 

	} else if (conn->type & NAF_CONN_TYPE_DETECTING) {
		int detected;

		if ((detected = naf_module__protocoldetect(NULL, conn)) == 1) {

			conn->type &= ~NAF_CONN_TYPE_DETECTING;
			naf_conn_setraw(conn, 0); /* get out of raw mode */

		} else {

			dvprintf(ourmodule, "no known protocol found on %d\n", conn->fdt->fd);
			naf_conn_free(conn);
		}

	} else if (conn->owner && conn->owner->connready) {

		if (conn->owner->connready(conn->owner, conn, NAF_CONN_READY_READ) == -1) {
			naf_conn_free(conn);
		}

	} else {
		dvprintf(ourmodule, "READ on %d\n", conn->fdt->fd);
		naf_conn_free(conn);
	}

	return 0;
}

static int connhandler_write(nbio_fd_t *fdt)
{
	struct nafconn *conn = (struct nafconn *)fdt->priv;

	if (conn->type & NAF_CONN_TYPE_CONNECTING) {

		if (finishconnect(conn) < 0)
			naf_conn_free(conn);

	} else if (conn->owner && conn->owner->connready) {

		if (conn->owner->connready(conn->owner, conn, NAF_CONN_READY_WRITE) == -1)
			naf_conn_free(conn);

	} else {
		dvprintf(ourmodule, "WRITE on %d\n", conn->fdt->fd);
		naf_conn_free(conn);
	}

	return 0;
}

static int connhandler_eof(nbio_fd_t *fdt)
{
	struct nafconn *conn = (struct nafconn *)fdt->priv;
	int die = 0;

	if (conn->type & NAF_CONN_TYPE_LISTENER)
		die = 1;

	if (conndebug) {
		dvprintf(ourmodule, "connhandler_eof(%p [fd %d, cid %d]) -- %s|%s|%s\n",
				fdt, fdt->fd, conn->cid,
				(conn->type & NAF_CONN_TYPE_SERVER) ? "SERVER" : "",
				(conn->type & NAF_CONN_TYPE_CLIENT) ? "CLIENT" : "",
				(conn->type & NAF_CONN_TYPE_CONNECTING) ? "CONNECTING" : "");
	}

	naf_conn_free(conn);

	return die ? -1 : 0;
}

static int connhandler(void *nbv, int event, nbio_fd_t *fdt)
{

	if (!nbv || !fdt || !fdt->priv)
		return -1;

	if (conndebug)
		dvprintf(ourmodule, "connhandler(event = %d, fdt = %p [fd %d])\n", event, fdt, fdt->fd);

	if (event == NBIO_EVENT_READ)
		return connhandler_read(fdt);
	else if (event == NBIO_EVENT_WRITE)
		return connhandler_write(fdt);
	else if ((event == NBIO_EVENT_ERROR) || (event == NBIO_EVENT_EOF))
		return connhandler_eof(fdt);
	return -1;
}

static struct nafconn *naf_conn_alloc(void)
{
	struct nafconn *nc;

	if (!(nc = (struct nafconn *)naf_malloc(ourmodule, sizeof(struct nafconn))))
		return NULL;
	memset(nc, 0, sizeof(struct nafconn));

	nc->cid = conn_nextcid++;

	if (conndebug)
		dvprintf(ourmodule, "naf_conn_alloc: %p (cid %d)\n", nc, nc->cid);

	return nc;
}

static int fillendpoints(struct nafconn *conn)
{

	/*
	 * Fetch the local and remote addresses of the socket, for use 
	 * later on.
	 */
	memset(&conn->remoteendpoint, 0, sizeof(conn->remoteendpoint));
	memset(&conn->localendpoint, 0, sizeof(conn->localendpoint));
	if (conn->fdt) {
		int remotesize = sizeof(conn->remoteendpoint);
		int localsize = sizeof(conn->localendpoint);

		if ((getsockname(conn->fdt->fd, (struct sockaddr *)&conn->localendpoint, &localsize) == 0)) {
			if (conndebug) {
				dvprintf(ourmodule, "[fd %d, cid %d] local = %s:%u\n", conn->fdt->fd, conn->cid, inet_ntoa(((struct sockaddr_in *)&conn->localendpoint)->sin_addr), ntohs(((struct sockaddr_in *)&conn->localendpoint)->sin_port));
			}
		}

		/* This won't work for listeners */
		if ((getpeername(conn->fdt->fd, (struct sockaddr *)&conn->remoteendpoint, &remotesize) == 0)) {
			if (conndebug) {
				dvprintf(ourmodule, "[fd %d, cid %d] remote = %s:%u\n", conn->fdt->fd, conn->cid, inet_ntoa(((struct sockaddr_in *)&conn->remoteendpoint)->sin_addr), ntohs(((struct sockaddr_in *)&conn->remoteendpoint)->sin_port));
			}
		}
	}

	return 0;
}

struct nafconn *naf_conn_addconn(struct nafmodule *mod, int fd, naf_u32_t type)
{
	struct nafconn *newconn;
	int nbtype;

	if (type & NAF_CONN_TYPE_LISTENER)
		nbtype = NBIO_FDTYPE_LISTENER;
	else if (type & NAF_CONN_TYPE_DATAGRAM)
		nbtype = NBIO_FDTYPE_DGRAM;
	else
		nbtype = NBIO_FDTYPE_STREAM;

	if (!(newconn = naf_conn_alloc()))
		return NULL;

	/*
	 * The Tx buffer length is huge so that we have enough to send out
	 * lots of buddy updates real fast.  This is necessary for the initial
	 * presence when resuming sessions, for example.  (Yes, I know this is
	 * a crappy solution.)
	 */
	if (!(newconn->fdt = nbio_addfd(&gnb, nbtype, fd, 0, connhandler, (void *)newconn, 1, 220))) {
		dperror(NULL, "nbio_addfd");
		naf_conn_free(newconn);
		return NULL;
	}
	newconn->type = type;
	newconn->endpoint = NULL;

	newconn->owner = mod;
	newconn->parent = NULL;

	newconn->lastrx = time(NULL);
	newconn->lastrx2 = 0;

	newconn->state = 0;

	newconn->flags = 0;
	newconn->nextseqnum = 0x0000;

	newconn->waitingbuf = NULL;
	newconn->waitingbuflen = 0;

	memset(&newconn->remoteendpoint, 0, sizeof(newconn->remoteendpoint));
	memset(&newconn->localendpoint, 0, sizeof(newconn->localendpoint));

	if ((newconn->type & NAF_CONN_TYPE_CONNECTING))
		naf_conn_setraw(newconn, 1);
	else if (newconn->type & NAF_CONN_TYPE_READRAW)
		naf_conn_setraw(newconn, 2); /* XXX #define */
	else
		fillendpoints(newconn);

	return newconn;
}

struct nafconn *naf_conn_getparent(struct nafconn *conn)
{
	if (!conn)
		return NULL;
	if (conn->parent)
		return conn->parent;
	if (conn->endpoint && conn->endpoint->parent)
		return conn->endpoint->parent;
	return NULL;
}

int naf_conn__poll(int timeout)
{
	return nbio_poll(&gnb, timeout);
}

static void dumpbox(struct nafmodule *mod, const char *prefix, naf_conn_cid_t cid, unsigned char *buf, int len)
{
	int z = 0, x, y;
	char tmpstr[256];

	while (z<len) {
		x = snprintf(tmpstr, sizeof(tmpstr), "%sput, %d bytes to cid %ld:      ", prefix, len, cid);
		for (y = 0; y < 8; y++) {
			if (z<len) {
				snprintf(tmpstr+x, sizeof(tmpstr)-strlen(tmpstr), "%02x ", buf[z]);
				z++;
				x += 3;
			} else
				break;
		}
		dvprintf(mod, "%s\n", tmpstr);
	}
}

int naf_conn_reqread(struct nafconn *conn, unsigned char *buf, int buflen, int offset)
{

	if (!conn || !conn->fdt || !buf || (buflen <= 0))
		return -1;

	if (conndebug > 2)
		dvprintf(ourmodule, "adding read buffer (%p) of length %d (offset %d) to cid %ld\n", buf, buflen, offset, conn->cid);

	return nbio_addrxvector(&gnb, conn->fdt, buf, buflen, offset);
}

int naf_conn_reqwrite(struct nafconn *conn, unsigned char *buf, int buflen)
{

	if (!conn || !conn->fdt || !buf || (buflen <= 0)) {
		errno = EINVAL;
		return -1;
	}

	if (conndebug > 2)
		dumpbox(ourmodule, "out", conn->cid, buf, buflen);

	return nbio_addtxvector(&gnb, conn->fdt, buf, buflen);
}

int naf_conn_takeread(struct nafconn *conn, unsigned char **bufp, int *buflenp)
{
	int offset;

	if (!conn || !conn->fdt || !bufp || !buflenp) {
		errno = EINVAL;
		return -1;
	}

	*bufp = nbio_remtoprxvector(&gnb, conn->fdt, buflenp, &offset);

	if (conndebug > 2)
		dumpbox(ourmodule, "in", conn->cid, *bufp, offset);

	return *bufp ? *buflenp : -1;
}

int naf_conn_takewrite(struct nafconn *conn, unsigned char **bufp, int *buflenp)
{
	int offset;

	if (!conn || !conn->fdt || !bufp || !buflenp) {
		errno = EINVAL;
		return -1;
	}

	*bufp = nbio_remtoptxvector(&gnb, conn->fdt, buflenp, &offset);

	if (conndebug > 2)
		dumpbox(ourmodule, "backout", conn->cid, *bufp, *buflenp);

	return *bufp ? *buflenp : -1;
}

int naf_conn_setdelim(struct nafmodule *mod, struct nafconn *conn, const unsigned char *delim, const unsigned char delimlen)
{

	if (!conn || !conn->fdt)
		return -1;

	if (!delim || !delimlen)
		return nbio_cleardelim(conn->fdt);

	return nbio_adddelim(&gnb, conn->fdt, delim, delimlen);
}

static void updaterawmode(struct nafconn *conn)
{

	if (conn->type & NAF_CONN_TYPE_DETECTING)
		return; /* could be anything */

	naf_conn_setraw(conn, (conn->type & NAF_CONN_TYPE_READRAW) ? 2 /* XXX #define */: 0);

	return;
}

static int finishconnect(struct nafconn *conn)
{
	unsigned char blah;

	if (!conn || !conn->fdt || !(conn->type & NAF_CONN_TYPE_CONNECTING))
		return -1;

	/*
	 * This flag means its a socket that would have
	 * blocked on a connect.  Call a read(,,0) on it
	 * to see if it connected.  If connected, it should
	 * return zero, anything else means it didn't connect.
	 *
	 * XXX calling read() directly is not portable (neither is the
	 * varied use of directly calling close() elsewhere)
	 * 
	 */
	if (read(conn->fdt->fd, &blah, 0) != 0) {
		dperror(NULL, "delayed connect (false alarm)");
		return -1;
	}

	conn->type &= ~NAF_CONN_TYPE_CONNECTING;
	updaterawmode(conn);
	fillendpoints(conn);

	if (conn->owner && conn->owner->connready) {
		if (conn->owner->connready(conn->owner, conn, 
					   NAF_CONN_READY_CONNECTED) == -1) {
			dvprintf(ourmodule, "error on %d (CONNECTED)\n", conn->fdt->fd);
			return -1;
		} 
	} else {
		dvprintf(ourmodule, "no owner for %d!\n", conn->fdt->fd);
		return -1;
	}

	return 0;
}

char *naf_conn_getlocaladdrstr(struct nafmodule *mod, struct nafconn *conn)
{
	char *ip, *ret = NULL;
	int port = -1;

	if ((ip = naf_config_getmodparmstr(mod, "extipaddr")) && strlen(ip))
		ret = naf_strdup(mod, ip);
	else
		ret = naf_strdup(mod, inet_ntoa(((struct sockaddr_in *)&conn->localendpoint)->sin_addr));

	if (!ret)
		return NULL;

	/* This is probably a safe bet, as NAT rarely mangles incoming ports */
	if (!index(ret, ':'))
		port = (int)ntohs(((struct sockaddr_in *)&conn->localendpoint)->sin_port);

	/*
	 * If we don't have a port, figure one out...
	 */
	if (!index(ret, ':')) { 
		char *newip;

		if ((newip = naf_malloc(mod, strlen(ret)+1+strlen("65535")+1))) {
			snprintf(newip, strlen(ret)+1+strlen("65535")+1,
					"%s:%u", ret, port);
			naf_free(mod, ret);
			ret = newip;
		}
	}

	return ret;
}

/* port can be overridden if the hostname is in host:port syntax */
int naf_conn_startconnect(struct nafmodule *mod, struct nafconn *localconn, const char *host, int port)
{
	int s;
	struct hostent *h;
	char newhost[256];
	struct sockaddr_in sai;
	int status, inprogress = 0;

	if (!mod || !localconn || !host)
		return -1;

	strncpy(newhost, host, sizeof(newhost));
	if (index(newhost, ':')) {
		port = atoi(index(newhost, ':')+1);
		*(index(newhost, ':')) = '\0';
	} 

	if (conndebug)
		dvprintf(mod, "starting non-blocking connect to %s port %d\n", newhost, port);

	/* XXX XXX XXX this blocks!!! */
	if (!(h = gethostbyname(newhost))) {
		dvprintf(mod, "gethostbyname failed for %s\n", newhost);
		return -1;
	}
 	if (conndebug)
		dvprintf(mod, "gethostbyname finished (%s)\n", newhost);

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		dvprintf(mod, "socket() failed: %s\n", strerror(errno));
		return -1;
	}
	memcpy(&sai.sin_addr.s_addr, h->h_addr, 4);
	sai.sin_family = AF_INET;
	sai.sin_port = htons(port);

	if (fcntl(s, F_SETFL, O_NONBLOCK) < 0) {
		dperror(mod, "fcntl");
		close(s);
		return -1;
	}
  
	status = connect(s, (struct sockaddr *)&sai, sizeof(sai));
  
	if ((status == -1) && (errno != EINPROGRESS)) {
		dperror(mod, "connect");
		close(s);
		return -1;
	} else if (status == 0) {
		inprogress = 0;
	} else if ((status == -1) && (errno == EINPROGRESS)) {
		inprogress = NAF_CONN_TYPE_CONNECTING;
	}

	if (!(localconn->endpoint = naf_conn_addconn(NULL, s, (localconn->type ^ NAF_CONN_TYPE_CLIENT)|NAF_CONN_TYPE_SERVER|inprogress/*inherit client type*/))) {
		close(s);
		return -1;
	}

	if (localconn->owner && localconn->owner->takeconn)
		localconn->owner->takeconn(mod, localconn->endpoint);

	/* XXX RAWAITING is a FLAP-specific flag... this should NOT be here */
	if (localconn->type & NAF_CONN_TYPE_RAWWAITING) {
		localconn->type &= ~NAF_CONN_TYPE_RAWWAITING;
		localconn->endpoint->type &= ~NAF_CONN_TYPE_RAWWAITING;
	}

	/* endpoint inherits module owner */
	localconn->endpoint->owner = localconn->owner;

	/* loop them together */
	localconn->endpoint->endpoint = localconn;

	if (conndebug)	
		dvprintf(mod, "connection started (%d)\n", !!inprogress);


	return 0;
}

static const char *gettypestr(naf_u32_t type)
{
	static char buf[512];

	snprintf(buf, sizeof(buf),
			"%s%s%s%s"
			"%s%s%s%s"
			"%s%s%s%s"
			"%s%s%s%s"
			"%s%s%s%s",

			(type & NAF_CONN_TYPE_CLIENT) ? "client|" : "",
			(type & NAF_CONN_TYPE_SERVER) ? "server|" : "",
			(type & NAF_CONN_TYPE_LISTENER) ? "listener|" : "",
			(type & NAF_CONN_TYPE_DATAGRAM) ? "datagram|" : "",

			(type & NAF_CONN_TYPE_PROROGUEDEATH) ? "prorogue|" : "",
			(type & NAF_CONN_TYPE_LOCAL) ? "local|" : "",
			(type & NAF_CONN_TYPE_DETECTING) ? "detecting|" : "",
			(type & NAF_CONN_TYPE_FLAP) ? "flap|" : "",

			(type & NAF_CONN_TYPE_APIP) ? "apip|" : "",
			(type & NAF_CONN_TYPE_MSNP) ? "msnp|" : "",
			(type & NAF_CONN_TYPE_YAHOO) ? "yahoo|" : "",
			(type & NAF_CONN_TYPE_IRC) ? "irc|" : "",

			(type & NAF_CONN_TYPE_ABFLAP) ? "abflap|" : "",
			(type & NAF_CONN_TYPE_ABROUTER) ? "abrouter|" : "",
			(type & NAF_CONN_TYPE_ABDPCON) ? "abdpcon|" : "",
			(type & NAF_CONN_TYPE_SOCKS) ? "socks|" : "",

			(type & NAF_CONN_TYPE_RAW) ? "raw|" : "",
			(type & NAF_CONN_TYPE_CONNECTING) ? "connecting|" : "",
			(type & NAF_CONN_TYPE_RAWWAITING) ? "rawwaiting|" : "",
			(type & NAF_CONN_TYPE_READRAW) ? "readraw|" : "");

	return buf;
}

static const char *getdottedquad(struct sockaddr_in *sin)
{
	static char buf[512];

	snprintf(buf, sizeof(buf), "%s:%u",
			inet_ntoa(sin->sin_addr),
			ntohs(sin->sin_port));

	return buf;
}

static int listconns_tag_matcher(struct nafmodule *mod, void *udv, const char *tagname, char tagtype, void *tagdata)
{
	naf_rpc_arg_t **head = (naf_rpc_arg_t **)udv;

	if (tagtype == 'S')
		naf_rpc_addarg_string(mod, head, tagname, (char *)tagdata);
	else if (tagtype == 'I')
		naf_rpc_addarg_scalar(mod, head, tagname, (int)tagdata);

	return 0; /* keep going */
}

struct listconnsinfo {
	naf_rpc_arg_t **head;
	naf_rpc_arg_t *cid;
	int wanttags;
};

static int listconns_matcher(struct nafmodule *mod, struct nafconn *conn, const void *ud)
{
	struct listconnsinfo *lci = (struct listconnsinfo *)ud;
	naf_rpc_arg_t **carg;
	int ret = 0;

	if (lci->cid) {
		if (conn->cid != lci->cid->data.scalar)
			return 0;
		else
			ret = 1;
	}

	if ((carg = naf_rpc_addarg_array(mod, lci->head, getcidstr(conn)))) {

		if (conn->owner)
			naf_rpc_addarg_string(mod, carg, "owner", conn->owner->name);
		naf_rpc_addarg_scalar(mod, carg, "type", conn->type);
		naf_rpc_addarg_string(mod, carg, "typestr", gettypestr(conn->type));
		naf_rpc_addarg_string(mod, carg, "localaddr", getdottedquad((struct sockaddr_in *)&conn->localendpoint));
		naf_rpc_addarg_string(mod, carg, "remoteaddr", getdottedquad((struct sockaddr_in *)&conn->remoteendpoint));
		if (conn->endpoint)
			naf_rpc_addarg_string(mod, carg, "endpoint", getcidstr(conn->endpoint));
		if (conn->parent)
			naf_rpc_addarg_string(mod, carg, "parent", getcidstr(conn->parent));
		naf_rpc_addarg_scalar(mod, carg, "servtype", conn->servtype);

		if (lci->wanttags) {
			naf_rpc_arg_t **tags;

			if ((tags = naf_rpc_addarg_array(mod, carg, "tags"))) 
				naf_tag_iter(&conn->taglist, NULL, listconns_tag_matcher, (void *)tags);
		}
	}

	return ret;
}

/*
 * conn->getconninfo()
 *   IN:
 *      [optional] scalar cid;
 *      [optional] boolean wanttags;
 *
 *   OUT:
 *      array cid {
 *      }
 *      ...
 */
static void __rpc_conn_getconninfo(struct nafmodule *mod, naf_rpc_req_t *req)
{
	struct listconnsinfo lci;
	naf_rpc_arg_t *cid, *wt;

	lci.head = &req->returnargs;

	if ((cid = naf_rpc_getarg(req->inargs, "cid"))) {
		if (cid->type != NAF_RPC_ARGTYPE_SCALAR) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}
	lci.cid = cid;

	lci.wanttags = 0;
	if ((wt = naf_rpc_getarg(req->inargs, "wanttags"))) {
		if (wt->type != NAF_RPC_ARGTYPE_BOOL) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
		lci.wanttags = wt->data.boolean;
	}
	
	naf_conn_find(mod, listconns_matcher, (void *)&lci);

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}

static int modinit(struct nafmodule *mod)
{

	ourmodule = mod;

	/* XXX some question about the correct value of this number. */
	nbio_init(&gnb, 32768);

	naf_rpc_register_method(mod, "getconninfo", __rpc_conn_getconninfo, "Retrieve connection information");

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	ourmodule = NULL;

	return 0;
}

static int timerhandler_matcher(struct nafmodule *mod, struct nafconn *conn, const void *data)
{

	if (!(conn->type & NAF_CONN_TYPE_DETECTING))
	       return 0;

	if ((time(NULL) - conn->lastrx) < NAF_CONN_DETECT_TIMEOUT)
		return 0;

	naf_conn_setraw(conn, 0); /* clear raw mode */

	if (naf_module__protocoldetecttimeout(mod, conn) <= 0) {
		dvprintf(mod, "no one wants to deal with timed out protocol detect on %d\n", conn->fdt->fd);
		naf_conn_schedulekill(conn);
	}

	return 0;
}

static void timerhandler(struct nafmodule *mod)
{

	naf_conn_find(mod, timerhandler_matcher, NULL);

	nbio_cleanuponly(&gnb);

	return;
}

static struct nafconn *listenestablish(struct nafmodule *mod, unsigned short portnum, struct nafmodule *nowner)
{
	int listenfd;
	const int on = 1;
	struct addrinfo hints, *res, *ressave;
	char serv[5];
	struct nafconn *retconn = NULL;

	snprintf(serv, sizeof(serv), "%d", portnum);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(NULL/*any IP*/, serv, &hints, &res) != 0) {
		dperror(mod, "getaddrinfo");
		return NULL;
	}
	ressave = res;
	do {
		listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (listenfd < 0)
			continue;
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
			break; /* success */
		close(listenfd);
	} while ( (res = res->ai_next) );
	if (!res)
		return NULL;
	if (listen(listenfd, 1024)!=0) {
		dperror(mod, "listen");
		return NULL;
	}
	freeaddrinfo(ressave);

	if (!(retconn = naf_conn_addconn(nowner, listenfd, NAF_CONN_TYPE_LISTENER))) {
		dprintf(mod, "unable to add connection for listener\n");
		close(listenfd);
		return NULL;
	}

	return retconn;
}

static int findlistenport_matcher(struct nafmodule *mod, struct nafconn *conn, const void *data)
{
	unsigned short port;

	if (!(conn->type & NAF_CONN_TYPE_LISTENER))
		return 0;

	port = ntohs(((struct sockaddr_in *)&conn->localendpoint)->sin_port);

	if (port == *(unsigned short *)data)
		return 1;

	return 0;
}

static struct nafconn *findlistenport(struct nafmodule *mod, unsigned short port)
{
	return naf_conn_find(mod, findlistenport_matcher, (const void *)&port);
}

static int listenport_isconfigured(struct nafmodule *mod, unsigned short port)
{
	char *conf, *cur;

	if (!(conf = naf_strdup(mod, naf_config_getmodparmstr(mod, "listenports"))))
		return 0;

	cur = strtok(conf, ",");
	do {
		if (!cur)
			break;

		if (port == atoi(cur)) {
			naf_free(mod, conf);
			return 1;
		}

	} while ((cur = strtok(NULL, ",")));

	naf_free(mod, conf);

	return 0;
}

static int cleanlisteners_matcher(struct nafmodule *mod, struct nafconn *conn, const void *data)
{
	unsigned short port;

	if (!(conn->type & NAF_CONN_TYPE_LISTENER))
		return 0;

	port = ntohs(((struct sockaddr_in *)&conn->localendpoint)->sin_port);

	if (!listenport_isconfigured(mod, port)) {
		dvprintf(mod, "removing unconfigured listen port %u\n", port);
		naf_conn_free(conn);
	}

	return 0;
}

/*
 * Check open listening sockets against configured listen ports, remove and
 * add where necessary.
 */
static void cleanlisteners(struct nafmodule *mod)
{
	char *conf, *cur;


	/* Remove unconfigured ports */
	naf_conn_find(mod, cleanlisteners_matcher, NULL);


	/* Add newly configured ports */

	if (!(conf = naf_config_getmodparmstr(mod, "listenports"))) {
		dprintf(mod, "WARNING: no listen ports configured\n");
		return;
	}

	if (!(conf = naf_strdup(mod, conf)))
		return;

	cur = strtok(conf, ",");
	do {
		struct nafconn *nconn = NULL;
		struct nafmodule *nconnowner = NULL;
		const char *owner = NULL;
		int portnum = -1;
		int goahead = 1;

		if (!cur)
			break;

		portnum = atoi(cur);

		owner = index(cur, '/');
		if (owner) {
			owner++;
			nconnowner = naf_module_findbyname(mod, owner);
		}

		if (owner && !nconnowner) {
			dvprintf(mod, "unable to find module '%s' for listener port %d\n", owner, portnum);
			goahead = 0;
		}

		if (portnum == -1) {
			dprintf(mod, "invalid listener port specification\n");
			goahead = 0;
		}

		if (findlistenport(mod, atoi(cur))) {
			dvprintf(mod, "duplicate listener specified on port %d\n", portnum);
			goahead = 0;
		}

		if (goahead)
			nconn = listenestablish(mod, portnum, nconnowner);

		if (nconn) {
			dvprintf(mod, "listening on port %d (for %s)\n", 
					portnum,
					nconnowner ? nconnowner->name : "all");
		}

	} while ((cur = strtok(NULL, ",")));

	naf_free(mod, conf);

	return;
}

static void signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {
		char *extip, *debugstr;

		if ((extip = naf_config_getmodparmstr(mod, "extipaddr")))
			dvprintf(mod, "assuming incoming connections on %s\n", extip);

		if ((debugstr = naf_config_getmodparmstr(mod, "debug")))
			conndebug = atoi(debugstr);
		if (conndebug == -1)
			conndebug = CONN_DEBUG_DEFAULT;

		cleanlisteners(mod);

	}

	return;
}

static int modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "conn");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->signal = signalhandler;
	mod->timerfreq = NAF_CONN_DETECT_TIMEOUT;
	mod->timer = timerhandler;

	return 0;
}

int naf_conn__register(void)
{
	return naf_module__registerresident("conn", modfirst, NAF_MODULE_PRI_SECONDPASS);
}

