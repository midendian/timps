/*
 * libnbio - Portable wrappers for non-blocking sockets
 * Copyright (c) 2000-2005 Adam Fritzler <mid@zigamorph.net>, et al
 *
 * libnbio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License (version 2.1) as published by
 * the Free Software Foundation.
 *
 * libnbio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <libnbio.h>
#include "impl.h"

/* XXX this should be elimitated by using more bookkeeping */
static void setmaxpri(nbio_t *nb)
{
	nbio_fd_t *cur;
	int max = 0;

	for (cur = (nbio_fd_t *)nb->fdlist; cur; cur = cur->next) {
		if (cur->flags & NBIO_FDT_FLAG_CLOSED)
			continue;
		if (cur->pri > max)
			max = cur->pri;
	}

	nb->maxpri = max;

	return;
}

int nbio_settimer(nbio_t *nb, nbio_fd_t *fdt, int interval)
{

	if (!nb || !fdt || (interval < 0)) {
		errno = EINVAL;
		return -1;
	}

	fdt->timerinterval = interval;
	fdt->timernextfire = time(NULL) + interval;

	return 0;
}

/* Since this skips INTERNAL, it is useful only for outside callers. */
nbio_fd_t *nbio_iter(nbio_t *nb, int (*matcher)(nbio_t *nb, void *ud, nbio_fd_t *fdt), void *userdata)
{
	nbio_fd_t *cur;

	if (!nb || !matcher) {
		errno = EINVAL;
		return NULL;
	}

	for (cur = (nbio_fd_t *)nb->fdlist; cur; cur = cur->next) {

		if (cur->flags & NBIO_FDT_FLAG_IGNORE)
			continue;
		if (cur->flags & NBIO_FDT_FLAG_INTERNAL)
			continue;

		if (matcher(nb, userdata, cur))
			return cur;

	}

	return NULL;
}

nbio_fd_t *nbio_getfdt(nbio_t *nb, nbio_sockfd_t fd)
{
	nbio_fd_t *cur;

	if (!nb) {
		errno = EINVAL;
		return NULL;
	}

	for (cur = (nbio_fd_t *)nb->fdlist; cur; cur = cur->next) {
		if (cur->flags & NBIO_FDT_FLAG_IGNORE)
			continue;
		if (cur->fd == fd)
			return cur;
	}

	errno = ENOENT;
	return NULL;
}

static int streamread_nodelim(nbio_t *nb, nbio_fd_t *fdt)
{
	nbio_buf_t *cur;
	int target, got;

	for (cur = fdt->rxchain; cur; cur = cur->next) {
		/* Find a non-zero buffer that still has space left in it */
		if (cur->len && (cur->offset < cur->len))
			break;
	}

	if (!cur) {
		fdt_setpollin(nb, fdt, 0);
		return 0;
	}

	target = cur->len - cur->offset;

	/* XXX should allow methods to override -- ie, WSARecv on win32 */
	if (((got = fdt_read(fdt, cur->data+cur->offset, target)) < 0) && (errno != EINTR) && (errno != EAGAIN)) {
		return fdt->handler(nb, NBIO_EVENT_ERROR, fdt);
	}

	if (got == 0)
		return fdt->handler(nb, NBIO_EVENT_EOF, fdt);

	if (got < 0)
		got = 0; /* a non-fatal error occured; zero bytes actually read */

	cur->offset += got;

	if (cur->offset >= cur->len)
		return fdt->handler(nb, NBIO_EVENT_READ, fdt);

	return 0; /* don't call handler unless we filled a buffer */
}

static int streamread_delim(nbio_t *nb, nbio_fd_t *fdt)
{
	nbio_buf_t *cur;
	int got, rr, target;

	rr = 0;
	for (cur = fdt->rxchain; cur; cur = cur->next) {
		/* Find a non-zero buffer that still has space left in it */
		if (cur->len && (cur->offset < cur->len))
			break;
	}

	if (!cur) {
		fdt_setpollin(nb, fdt, 0);
		return 0;
	}

	for (got = 0, target = 0; (got < (cur->len - cur->offset)) && !target; ) {
		nbio_delim_t *cd;

		if ((rr = fdt_read(fdt, cur->data+cur->offset, 1)) <= 0)
			break;

		cur->offset += rr;
		got += rr;

		for (cd = fdt->delims; cd; cd = cd->next) {
			if ((cur->offset >= cd->len) &&
				(memcmp(cur->data+cur->offset-cd->len,
						cd->data, cd->len) == 0)) {

				target++;

				if (!(fdt->flags & NBIO_FDT_FLAG_KEEPDELIM))
					memset(cur->data+cur->offset-cd->len, '\0', cd->len);
				break;
			}
		}

	}

	if ((rr < 0) && (errno != EAGAIN))
		return fdt->handler(nb, NBIO_EVENT_ERROR, fdt);

	if ((cur->offset >= cur->len) || target) {
		if ((got = fdt->handler(nb, NBIO_EVENT_READ, fdt)) < 0)
			return got;
	}

	if (rr == 0)
		return fdt->handler(nb, NBIO_EVENT_EOF, fdt);

	return 0; /* don't call handler unless we filled a buffer or got delim  */
}

static int streamread(nbio_t *nb, nbio_fd_t *fdt)
{

	if ((fdt->flags & NBIO_FDT_FLAG_RAW) ||
			(fdt->flags & NBIO_FDT_FLAG_RAWREAD))
		return fdt->handler(nb, NBIO_EVENT_READ, fdt);

	if (fdt->delims)
		return streamread_delim(nb, fdt);

	return streamread_nodelim(nb, fdt);
}

static int streamwrite(nbio_t *nb, nbio_fd_t *fdt)
{
	nbio_buf_t *cur;
	int target, wrote;
	int pollout = 0;
	time_t now;

	if (fdt->flags & NBIO_FDT_FLAG_RAW)
		return fdt->handler(nb, NBIO_EVENT_WRITE, fdt);

	now = time(NULL);

	for (cur = fdt->txchain; cur; cur = cur->next) {
		/* Find a non-zero buffer that still needs data written
			also checks for the trigger time */
		if (cur->len && (cur->offset < cur->len)) {
			if (cur->trigger <= now)
				break;
			else
				pollout = 1; /* keep checking on this fd */
		}
	}

	if (!cur) {
		fdt_setpollout(nb, fdt, pollout);
		return 0; /* nothing to do */
	}

	target = cur->len - cur->offset;

	if (((wrote = fdt_write(fdt, cur->data+cur->offset, target)) < 0) && (errno != EINTR) && (errno != EAGAIN)) {
		return fdt->handler(nb, NBIO_EVENT_ERROR, fdt);
	}

	if (wrote < 0)
		wrote = 0; /* a non-fatal error occured; zero bytes actually written */

	cur->offset += wrote;

	if (cur->offset >= cur->len) {
		int ret;

		ret = fdt->handler(nb, NBIO_EVENT_WRITE, fdt);

		if (!fdt->txchain && (fdt->flags & NBIO_FDT_FLAG_CLOSEONFLUSH))
			ret = fdt->handler(nb, NBIO_EVENT_EOF, fdt);

		return ret;
	}

	return 0; /* don't call handler unless we finished a buffer */
}

/*
 * DGRAM sockets are completely unbuffered.
 */
static int dgramread(nbio_t *nb, nbio_fd_t *fdt)
{
	return fdt->handler(nb, NBIO_EVENT_READ, fdt);
}

static int dgramwrite(nbio_t *nb, nbio_fd_t *fdt)
{
	return fdt->handler(nb, NBIO_EVENT_WRITE, fdt);
}

int nbio_init(nbio_t *nb, int pfdsize)
{

	if (!nb || (pfdsize <= 0))
		return -1;

	memset(nb, 0, sizeof(nbio_t));

	if (pfdinit(nb, pfdsize) == -1)
		return -1;

	setmaxpri(nb);

	return 0;
}

int nbio_kill(nbio_t *nb)
{
	nbio_fd_t *cur;

	if (!nb) {
		errno = EINVAL;
		return -1;
	}

	for (cur = (nbio_fd_t *)nb->fdlist; cur; cur = cur->next)
		nbio_closefdt(nb, cur);

	nbio_cleanuponly(nb); /* to clean up the list */

	pfdkill(nb);

	return 0;
}

/* XXX handle malloc==NULL case better here */
static int preallocchains(nbio_fd_t *fdt, int rxlen, int txlen)
{
	nbio_buf_t *newbuf;

	while (rxlen) {
		if (!(newbuf = malloc(sizeof(nbio_buf_t))))
			return -1;

		newbuf->next = fdt->rxchain_freelist;
		fdt->rxchain_freelist = newbuf;

		rxlen--;
	}

	while (txlen) {
		if (!(newbuf = malloc(sizeof(nbio_buf_t))))
			return -1;

		newbuf->next = fdt->txchain_freelist;
		fdt->txchain_freelist = newbuf;

		txlen--;
	}

	return 0;
}

nbio_fd_t *nbio_addfd(nbio_t *nb, int type, nbio_sockfd_t fd, int pri, nbio_handler_t handler, void *priv, int rxlen, int txlen)
{
	nbio_fd_t *newfd;

	if (!nb || (pri < 0) || (rxlen < 0) || (txlen < 0)) {
		errno = EINVAL;
		return NULL;
	}

	if ((type != NBIO_FDTYPE_STREAM) &&
			(type != NBIO_FDTYPE_LISTENER) &&
			(type != NBIO_FDTYPE_DGRAM)) {
		errno = EINVAL;
		return NULL;
	}

	if (nbio_getfdt(nb, fd)) {
		errno = EEXIST;
		return NULL;
	}

	if (fdt_setnonblock(fd) == -1)
		return NULL;

	if (!(newfd = malloc(sizeof(nbio_fd_t)))) {
		errno = ENOMEM;
		return NULL;
	}

	newfd->type = type;
	newfd->fd = fd;
	newfd->flags = NBIO_FDT_FLAG_NONE;
	newfd->pri = pri;
	newfd->delims = NULL;
	newfd->handler = handler;
	newfd->priv = priv;
	newfd->timerinterval = 0;
	newfd->rxchain = newfd->txchain = newfd->txchain_tail = NULL;
	newfd->rxchain_freelist = newfd->txchain_freelist = NULL;
	if (preallocchains(newfd, rxlen, txlen) < 0) {
		free(newfd);
		return NULL;
	}

	if (pfdadd(nb, newfd) == -1) {
		/* XXX free up chains */
		free(newfd);
		errno = ENOMEM;
		return NULL;
	}

	fdt_setpollnone(nb, newfd);

	if (newfd->type == NBIO_FDTYPE_STREAM) {
		/* will be set when buffers are added */
		fdt_setpollin(nb, newfd, 0);
		fdt_setpollout(nb, newfd, 0);
	} else if (newfd->type == NBIO_FDTYPE_LISTENER) {
		fdt_setpollin(nb, newfd, 1);
		fdt_setpollout(nb, newfd, 0);
	} else if (newfd->type == NBIO_FDTYPE_DGRAM) {
		fdt_setpollin(nb, newfd, 1);
		fdt_setpollout(nb, newfd, 0);
	}

	pfdaddfinish(nb, newfd);

	newfd->next = (nbio_fd_t *)nb->fdlist;
	nb->fdlist = (void *)newfd;

	setmaxpri(nb);

	return newfd;
}

int nbio_closefdt(nbio_t *nb, nbio_fd_t *fdt)
{

	if (!nb || !fdt) {
		errno = EINVAL;
		return -1;
	}

	if (fdt->flags & NBIO_FDT_FLAG_CLOSED)
		return 0;

#if 0
	if (fdt->rxchain || fdt->txchain)
		fprintf(stderr, "WARNING: unfreed buffers on closed connection (%d/%p)\n", fdt->fd, fdt);
#endif

	fdt_setpollnone(nb, fdt);

	fdt_close(fdt);
	fdt->fd = -1;
	fdt->flags |= NBIO_FDT_FLAG_CLOSED;

	pfdrem(nb, fdt);

	setmaxpri(nb);

	return 0;
}

void __fdt_free(nbio_fd_t *fdt)
{
	nbio_buf_t *buf, *tmp;

	nbio_cleardelim(fdt);

	for (buf = fdt->rxchain_freelist; buf; ) {
		tmp = buf;
		buf = buf->next;
		free(tmp);
	}

	for (buf = fdt->txchain_freelist; buf; ) {
		tmp = buf;
		buf = buf->next;
		free(tmp);
	}

	pfdfree(fdt);

	free(fdt);

	return;
}

/* Call the EOF callback on all connections in order to close them */
void nbio_alleofforce(nbio_t *nb)
{
	nbio_fd_t *cur;

	nbio_flushall(nb);

	for (cur = (nbio_fd_t *)nb->fdlist; cur; cur = cur->next) {
		if (cur->flags & NBIO_FDT_FLAG_CLOSED)
			continue;
		if (cur->handler)
			cur->handler(nb, NBIO_EVENT_EOF, cur);
	}

	return;
}

void nbio_flushall(nbio_t *nb)
{
	nbio_fd_t *cur;

	for (cur = (nbio_fd_t *)nb->fdlist; cur; cur = cur->next) {
		int i;

		if (cur->flags & NBIO_FDT_FLAG_CLOSED)
			continue;

		/* Call each ten times just to make sure */
		for (i = 10; i; i--) {
			if (cur->type == NBIO_FDTYPE_LISTENER)
				;
			else if (cur->type == NBIO_FDTYPE_STREAM)
				streamwrite(nb, cur);
			else if (cur->type == NBIO_FDTYPE_DGRAM)
				dgramwrite(nb, cur);
		}
	}

	return;
}

/* Do all cleanups that are normally done in nbio_poll */
int nbio_cleanuponly(nbio_t *nb)
{
	nbio_fd_t *cur = NULL, **prev = NULL;

	for (prev = (nbio_fd_t **)&nb->fdlist; (cur = *prev); ) {

		if (cur->flags & NBIO_FDT_FLAG_CLOSED) {
			*prev = cur->next;
			__fdt_free(cur);
			continue;
		}

		if ((cur->flags & NBIO_FDT_FLAG_CLOSEONFLUSH) && !cur->txchain)
			cur->handler(nb, NBIO_EVENT_EOF, cur);

		prev = &cur->next;
	}

	return 0;
}

int nbio_connect(nbio_t *nb, const struct sockaddr *addr, int addrlen, nbio_handler_t handler, void *priv)
{
	return fdt_connect(nb, addr, addrlen, handler, priv);
}

int __fdt_ready_in(nbio_t *nb, nbio_fd_t *fdt)
{

	if (fdt->type == NBIO_FDTYPE_LISTENER) {

		if (fdt->handler(nb, NBIO_EVENT_INCOMINGCONN, fdt) < 0)
			return -1;

	} else if (fdt->type == NBIO_FDTYPE_STREAM) {

		if (streamread(nb, fdt) < 0)
			return -1;

	} else if (fdt->type == NBIO_FDTYPE_DGRAM) {

		if (dgramread(nb, fdt) < 0)
			return -1;

	}

	return 0;
}

int __fdt_ready_out(nbio_t *nb, nbio_fd_t *fdt)
{

	if (fdt->type == NBIO_FDTYPE_LISTENER) {

		; /* invalid? */

	} else if (fdt->type == NBIO_FDTYPE_STREAM) {

		if (streamwrite(nb, fdt) < 0)
			return -1; /* XXX do something better */

	} else if (fdt->type == NBIO_FDTYPE_DGRAM) {

		if (dgramwrite(nb, fdt) < 0)
			return -1; /* XXX do something better */

	}

	return 0;
}

int __fdt_ready_eof(nbio_t *nb, nbio_fd_t *fdt)
{

	if ((fdt->fd != -1) && fdt->handler)
		fdt->handler(nb, NBIO_EVENT_EOF, fdt);

	return 0;
}

int __fdt_ready_all(nbio_t *nb, nbio_fd_t *fdt)
{

	if (fdt->timerinterval) {
		time_t now;

		now = time(NULL);

		if (fdt->timernextfire <= now) {

			fdt->handler(nb, NBIO_EVENT_TIMEREXPIRE, fdt);

			fdt->timernextfire = time(NULL) + fdt->timerinterval;
		}
	}

	if ((fdt->flags & NBIO_FDT_FLAG_CLOSEONFLUSH) && !fdt->txchain)
		fdt->handler(nb, NBIO_EVENT_EOF, fdt);

	return 0;
}

int nbio_poll(nbio_t *nb, int timeout)
{
	return pfdpoll(nb, timeout);
}

int nbio_setpri(nbio_t *nb, nbio_fd_t *fdt, int pri)
{

	if (!nb || !fdt || (pri < 0)) {
		errno = EINVAL;
		return -1;
	}

	fdt->pri = pri;

	setmaxpri(nb);

	return 0;
}

int nbio_setraw(nbio_t *nb, nbio_fd_t *fdt, int val)
{

	if (!fdt) {
		errno = EINVAL;
		return -1;
	}

	/* clear previous state */
	fdt->flags &= ~(NBIO_FDT_FLAG_RAW | NBIO_FDT_FLAG_RAWREAD);
	fdt_setpollnone(nb, fdt);

	if (val == 2) {

		fdt->flags |= NBIO_FDT_FLAG_RAWREAD;

		fdt_setpollin(nb, fdt, 1);

		/*
		 * RAWREAD mode still uses POLLOUT.
		 *
		 * Only clear POLLOUT if there are no pending buffers. In
		 * the case that there are, then set POLLOUT, wait for them
		 * to be sent, and pollout will be cleared by itself.
		 */
		fdt_setpollout(nb, fdt, fdt->txchain ? 1 : 0);

	} else if (val) {

		fdt->flags |= NBIO_FDT_FLAG_RAW;
		fdt_setpollin(nb, fdt, 1);
		fdt_setpollout(nb, fdt, 1);

	} else {
		if (fdt->rxchain)
			fdt_setpollin(nb, fdt, 1);
		if (fdt->txchain)
			fdt_setpollout(nb, fdt, 1);
	}

	return 0;
}

int nbio_setcloseonflush(nbio_fd_t *fdt, int val)
{

	if (!fdt) {
		errno = EINVAL;
		return -1;
	}

	if (val)
		fdt->flags |= NBIO_FDT_FLAG_CLOSEONFLUSH;
	else
		fdt->flags &= ~NBIO_FDT_FLAG_CLOSEONFLUSH;

	return 0;
}

int nbio_adddelim(nbio_t *nb, nbio_fd_t *fdt, const unsigned char *delim, const unsigned char delimlen)
{
	nbio_delim_t *nd;

	if (!nb || !fdt || (fdt->type != NBIO_FDTYPE_STREAM)) {
		errno = EINVAL;
		return -1;
	}

	if (!delimlen || !delim || (delimlen > NBIO_MAX_DELIMITER_LEN)) {
		errno = EINVAL;
		return -1;
	}

	if (!(nd = malloc(sizeof(nbio_delim_t))))
		return -1;

	nd->len = delimlen;
	memcpy(nd->data, delim, delimlen);

	nd->next = fdt->delims;
	fdt->delims = nd;

	return 0;
}

int nbio_cleardelim(nbio_fd_t *fdt)
{
	nbio_delim_t *cur;

	if (!fdt || (fdt->type != NBIO_FDTYPE_STREAM)) {
		errno = EINVAL;
		return -1;
	}

	for (cur = fdt->delims; cur; ) {
		nbio_delim_t *tmp;

		tmp = cur->next;
		free(cur);
		cur = tmp;
	}

	fdt->delims = NULL;

	return 0;
}

int nbio_setkeepdelim(nbio_fd_t *fdt, int val)
{

	if (!fdt || (fdt->type != NBIO_FDTYPE_STREAM)) {
		errno = EINVAL;
		return -1;
	}

	if (val)
		fdt->flags |= NBIO_FDT_FLAG_KEEPDELIM;
	else
		fdt->flags &= ~NBIO_FDT_FLAG_KEEPDELIM;

	return 0;
}

int nbio_sfd_close(nbio_t *nb, nbio_sockfd_t fd)
{
	return fdt_closefd(fd);
}

nbio_sockfd_t nbio_sfd_new_stream(nbio_t *nb)
{
	return fdt_newsocket(PF_INET, SOCK_STREAM);
}

int nbio_sfd_setnonblocking(nbio_t *nb, nbio_sockfd_t fd)
{
	return fdt_setnonblock(fd);
}

int nbio_sfd_connect(nbio_t *nb, nbio_sockfd_t fd, struct sockaddr *sa, int salen)
{
	return fdt_connectfd(fd, sa, salen);
}

int nbio_sfd_bind(nbio_t *nb, nbio_sockfd_t fd, struct sockaddr *sa, int salen)
{
	return fdt_bindfd(fd, sa, salen);
}

int nbio_sfd_listen(nbio_t *nb, nbio_sockfd_t fd)
{
	return fdt_listenfd(fd);
}

int nbio_sfd_read(nbio_t *nb, nbio_sockfd_t fd, void *buf, int count)
{
	return fdt_readfd(fd, buf, count);
}

int nbio_sfd_write(nbio_t *nb, nbio_sockfd_t fd, const void *buf, int count)
{
	return fdt_writefd(fd, buf, count);
}

nbio_sockfd_t nbio_sfd_accept(nbio_t *nb, nbio_sockfd_t fd, struct sockaddr *saret, int *salen)
{
	return fdt_acceptfd(fd, saret, salen);
}

nbio_sockfd_t nbio_getincomingconn(nbio_t *nb, nbio_fd_t *fdt, struct sockaddr *saret, int *salen)
{

	if (!nb || !fdt || (fdt->type != NBIO_FDTYPE_LISTENER)) {
		errno = EINVAL;
		return -1;
	}

	return nbio_sfd_accept(nb, fdt->fd, saret, salen);
}

nbio_sockfd_t nbio_sfd_newlistener(nbio_t *nb, unsigned short port)
{
	return fdt_newlistener(port);
}

