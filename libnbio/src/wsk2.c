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

#if 1
#include <stdio.h>
#endif

#ifdef NBIO_USE_WINSOCK2

#include <winsock2.h>

#include <libnbio.h>
#include "impl.h"

static void wsa_seterrno(void)
{
	int err;

	err = WSAGetLastError();

	if (err == WSANOTINITIALISED)
		errno = EINVAL; /* sure, why not */
	else if (err == WSAENETDOWN)
		errno = ENETDOWN;
	else if (err == WSAEFAULT)
		errno = EFAULT;
	else if (err == WSAENOTCONN)
		errno = ENOTCONN;
	else if (err == WSAEINTR)
		errno = EINTR;
	else if (err == WSAEINPROGRESS)
		errno = EINPROGRESS;
	else if (err == WSAENETRESET)
		errno = ENETRESET;
	else if (err == WSAENOTSOCK)
		errno = ENOTSOCK;
	else if (err == WSAEOPNOTSUPP)
		errno = EOPNOTSUPP;
	else if (err == WSAESHUTDOWN)
		errno = ESHUTDOWN;
	else if (err == WSAEWOULDBLOCK)
		errno = EAGAIN;
	else if (err == WSAEMSGSIZE)
		errno = EMSGSIZE;
	else if (err == WSAEINVAL)
		errno = EINVAL;
	else if (err == WSAECONNABORTED)
		errno = ECONNABORTED;
	else if (err == WSAETIMEDOUT)
		errno = ETIMEDOUT;
	else if (err == WSAECONNRESET)
		errno = ECONNRESET;
	else
		errno = EINVAL;

	return;
}

nbio_sockfd_t fdt_newsocket(nbio_t *nb, int family, int type)
{
	nbio_sockfd_t fd;

	if ((fd = WSASocket(family, type, 0, NULL, 0, 0 /* WSA_FLAG_OVERLAPPED */)) == SOCKET_ERROR) {
		wsa_seterrno();
		return -1;
	}

	return fd;
}

int fdt_readfd(nbio_sockfd_t fd, void *buf, int count)
{
	int ret;

	if ((ret = recv(fd, buf, count, 0)) == SOCKET_ERROR) {
		wsa_seterrno();
		return -1;
	}

	return ret;
}

int fdt_read(nbio_fd_t *fdt, void *buf, int count)
{
	return fdt_readfd(fdt->fd, but, count);
}

int fdt_writefd(nbio_sockfd_t fd, const void *buf, int count)
{
	int ret;

	if ((ret = send(fd, buf, count, 0)) == SOCKET_ERROR) {
		wsa_seterrno();
		return -1;
	}

	return ret;
}

int fdt_write(nbio_fd_t *fdt, const void *buf, int count)
{
	return fdt_writefd(fdt->fd, buf, count);
}

int fdt_closefd(nbio_sockfd_t fd)
{
	return closesocket(fd);
}

void fdt_close(nbio_fd_t *fdt)
{
	fdt_closefd(fdt->fd);
	return;
}

int fdt_setnonblock(nbio_sockfd_t fd)
{
	unsigned long val = 1;

	if (ioctlsocket(fd, FIONBIO, &val) == SOCKET_ERROR) {
		wsa_seterrno();
		return -1;
	}

	return 0;
}

static int wsainit(void)
{
	WORD reqver;
	WSADATA wsadata;

	reqver = MAKEWORD(2,2);

	if ((WSAStartup(reqver, &wsadata)) != 0)
		return -1;

	if ((LOBYTE(wsadata.wVersion) != 2) ||
			(HIBYTE(wsadata.wVersion) != 2)) {
		WSACleanup();
		return -1;
	}

	return 0;
}


/* nbio_t->intdata */
struct nbdata {
	int maxfd;
};

int pfdinit(nbio_t *nb, int pfdsize)
{
	struct nbdata *nbd;

	if (wsainit() == -1)
		return -1;

	if (!(nbd = nb->intdata = malloc(sizeof(struct nbdata)))) {
		WSACleanup();
		return -1;
	}

	nbd->maxfd = -1;

	return 0;
}

/* This kills the nbio_t, not a pfd -- confusing name. */
void pfdkill(nbio_t *nb)
{
	struct nbdata *nbd = (struct nbdata *)nb->intdata;

	free(nbd);
	nb->intdata = NULL;

	WSACleanup();

	return;
}

#define WANT_NONE 0x0000
#define WANT_READ 0x0001
#define WANT_WRITE 0x0002

/* nbio_fd_t->intdata */
struct fdtdata {
	unsigned short flags;
};

static int setmaxfd(nbio_t *nb)
{
	struct nbdata *nbd = (struct nbdata *)nb->intdata;
	nbio_sockfd_t maxfd = -1;
	nbio_fd_t *cur;

	for (cur = (nbio_fd_t *)nb->fdlist; cur; cur = cur->next) {
		if (cur->fd > maxfd)
			maxfd = cur->fd;
	}

	nbd->maxfd = maxfd;

	return maxfd;
}

int pfdadd(nbio_t *nb, nbio_fd_t *newfd)
{
	struct nbdata *nbd = (struct nbdata *)nb->intdata;
	struct fdtdata *data;

	if (!(data = malloc(sizeof(struct fdtdata))))
		return -1;
	memset(data, 0, sizeof(struct fdtdata));
	newfd->intdata = (void *)data;

	data->flags = WANT_NONE;

	return 0;
}

void pfdaddfinish(nbio_t *nb, nbio_fd_t *newfd)
{

	setmaxfd(nb);

	return;
}

void pfdrem(nbio_t *nb, nbio_fd_t *fdt)
{

	setmaxfd(nb);

	return;
}

void pfdfree(nbio_fd_t *fdt)
{
	struct fdtdata *data = (struct fdtdata *)fdt->intdata;

	free(data);
	fdt->intdata = NULL;

	return;
}

int pfdpoll(nbio_t *nb, int timeout)
{
	struct nbdata *nbd = (struct nbdata *)nb->intdata;
	int selret;
	nbio_fd_t *cur = NULL, **prev = NULL;
	fd_set rfds, wfds;
	struct timeval tv;

	if (!nb) {
		errno = EINVAL;
		return -1;
	}

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	memset(&tv, 0, sizeof(tv));
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	for (cur = (nbio_fd_t *)nb->fdlist; cur; cur = cur->next) {
		struct fdtdata *data = (struct fdtdata *)cur->intdata;

		if (cur->flags & NBIO_FDT_FLAG_CLOSED)
			continue;
		if (!data)
			continue;

		if (data->flags & WANT_READ)
			FD_SET(cur->fd, &rfds);
		if (data->flags & WANT_WRITE)
			FD_SET(cur->fd, &wfds);
	}

	errno = 0;
	if ((selret = select(nbd->maxfd+1, &rfds, &wfds, NULL, &tv)) == SOCKET_ERROR) {

		wsa_seterrno();

		/* Never return EINTR from nbio_poll... */
		if (errno == EINTR) {
			errno = 0;
			return 0;
		}

		return -1;

	}

	for (prev = (nbio_fd_t **)&nb->fdlist; (cur = *prev); ) {
		struct fdtdata *data = (struct fdtdata *)cur->intdata;

		if ((cur->flags & NBIO_FDT_FLAG_CLOSED) || !data) {
			*prev = cur->next;
			__fdt_free(cur);
			continue;
		}

		if (FD_ISSET(cur->fd, &rfds)) {
			if (__fdt_ready_in(nb, cur) == -1)
				return -1;
		}

		if (FD_ISSET(cur->fd, &wfds)) {
			if (__fdt_ready_out(nb, cur) == -1)
				return -1;
		}

		if (__fdt_ready_all(nb, cur) == -1)
			return -1;

		prev = &cur->next;
	}

	return 1;
}

void fdt_setpollin(nbio_t *nb, nbio_fd_t *fdt, int val)
{
	struct nbdata *nbd = (struct nbdata *)nb->intdata;
	struct fdtdata *data = (struct fdtdata *)fdt->intdata;

	if (val)
		data->flags |= WANT_READ;
	else
		data->flags &= ~WANT_READ;

	return;
}

void fdt_setpollout(nbio_t *nb, nbio_fd_t *fdt, int val)
{
	struct nbdata *nbd = (struct nbdata *)nb->intdata;
	struct fdtdata *data = (struct fdtdata *)fdt->intdata;

	if (val)
		data->flags |= WANT_WRITE;
	else
		data->flags &= ~WANT_WRITE;

	return;
}

void fdt_setpollnone(nbio_t *nb, nbio_fd_t *fdt)
{
	struct nbdata *nbd = (struct nbdata *)nb->intdata;
	struct fdtdata *data = (struct fdtdata *)fdt->intdata;

	data->flags &= ~(WANT_READ | WANT_WRITE);

	return;
}

struct connectinginfo {
	nbio_handler_t handler;
	void *handlerpriv;
};

static int fdt_connect_handler(void *nbv, int event, nbio_fd_t *fdt)
{
	nbio_t *nb = (nbio_t *)nbv;
	struct connectinginfo *ci = (struct connectinginfo *)fdt->priv;
	int error = 0;
	int len = sizeof(error);

	if ((event != NBIO_EVENT_READ) && (event != NBIO_EVENT_WRITE)) {

		if (ci->handler)
			error = ci->handler(nb, NBIO_EVENT_CONNECTFAILED, fdt);

		free(ci);
		fdt->flags &= ~NBIO_FDT_FLAG_IGNORE;
		nbio_closefdt(nb, fdt);

		return error;
	}

	/* So it looks right to the user. */
	fdt->flags |= NBIO_FDT_FLAG_IGNORE; /* This is evil. */
	fdt->priv = ci->handlerpriv;
	nbio_setraw(nb, fdt, 0);

	if (getsockopt(fdt->fd, SOL_SOCKET, SO_ERROR, (char *)&error, &len) == -1) {
		wsa_seterrno();
		error = errno;
	}

	if (error) {

		if (ci->handler)
			error = ci->handler(nb, NBIO_EVENT_CONNECTFAILED, fdt);

		free(ci);
		fdt->flags &= ~NBIO_FDT_FLAG_IGNORE;
		nbio_closefdt(nb, fdt);

		return error;
	}

	if (!ci->handler ||
			(ci->handler(nb, NBIO_EVENT_CONNECTED, fdt) == -1)) {

		free(ci);
		fdt->flags &= ~NBIO_FDT_FLAG_IGNORE;
		nbio_closefdt(nb, fdt);

		return -1;
	}

	fdt->fd = -1; /* prevent it from being close()'d by closefdt */

	free(ci);
	fdt->flags &= ~NBIO_FDT_FLAG_IGNORE;
	nbio_closefdt(nb, fdt);

	return 0;
}

int fdt_connectfd(nbio_sockfd_t fd, const struct sockaddr *addr, int addrlen)
{
	int ret;

	ret = connect(fd, addr, addrlen);
	if (ret == SOCKET_ERROR)
		wsa_seterrno();

	return ret;
}

int fdt_connect(nbio_t *nb, const struct sockaddr *addr, int addrlen, nbio_handler_t handler, void *priv)
{
	nbio_sockfd_t fd;
	struct connectinginfo *ci;
	nbio_fd_t *fdt;

	if (!nb || !addr) {
		errno = EINVAL;
		return -1;
	}

	if ((fd = fdt_newsocket(nb, addr->sa_family, SOCK_STREAM)) == -1)
		return -1;

	if (fdt_setnonblock(fd) == -1) {
		closesocket(fd);
		errno = EINVAL;
		return -1;
	}


	if ((fdt_connectfd(fd, addr, addrlen) == SOCKET_ERROR) &&
						(errno != EAGAIN) &&
						(errno != EINPROGRESS)) {
		fprintf(stderr, "fdt_conenct: connect failed: %d, %d, %d, %s\n", fd, addrlen, errno, strerror(errno));
		fdt_closefd(fd);
		return -1;
	}

	if (!(ci = malloc(sizeof(struct connectinginfo)))) {
		fdt_closefd(fd);
		return -1;
	}

	ci->handler = handler;
	ci->handlerpriv = priv;

	if (!(fdt = nbio_addfd(nb, NBIO_FDTYPE_STREAM, fd, 0, fdt_connect_handler, (void *)ci, 0, 0))) {
		fprintf(stderr, "fdt_conenct: nbio_addfd failed: %s\n", strerror(errno));
		fdt_closefd(fd);
		free(ci);
		errno = EINVAL;
		return -1;
	}

	nbio_setraw(nb, fdt, 1);

	return 0;
}

nbio_sockfd_t fdt_acceptfd(nbio_sockfd_t fd, struct sockaddr *saret, int *salen)
{
	int ret;

	ret = accept(fd, saret, salen);
	if (ret == SOCKET_ERROR)
		wsa_seterrno();

	return ret;
}

int fdt_bindfd(nbio_sockfd_t fd, struct sockaddr *sa, int salen)
{
	int ret;

	ret = bind(fd, sa, salen);
	if (ret == SOCKET_ERROR)
		wsa_seterrno();

	return ret;
}

int fdt_listenfd(nbio_sockfd_t fd)
{
	int ret;

	ret = listen(fd, 1024);
	if (ret == SOCKET_ERROR)
		wsa_seterrno();

	return ret;
}

nbio_sockfd_t fdt_newlistener(unsigned short portnum)
{
	nbio_sockfd_t sfd;
	const int on = 1;
	struct sockaddr_in sin;

	/* bind all interfaces */
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_port = portnum;

	if ((sfd = fdt_newsocket(PF_INET, SOCK_STREAM)) == -1)
		return -1;

	/* XXX does this work on winsock? */
	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (fdt_bindfd(sfd, &sin, sizeof(sin)) == -1) {
		fdt_closefd(sfd);
		return -1;
	}

	if (fdt_listenfd(sfd) == -1) {
		fdt_closefd(sfd);
		return -1;
	}

	return sfd;
}

#endif /* NBIO_USE_WINSOCK2 */

