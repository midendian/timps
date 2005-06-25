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

/* XXX ick. */
#if !defined(NBIO_USE_WINSOCK2)

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libnbio.h>
#include "impl.h"

int fdt_setnonblock(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;

	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int fdt_readfd(nbio_sockfd_t fd, void *buf, int count)
{
	return read(fd, buf, count);
}

int fdt_read(nbio_fd_t *fdt, void *buf, int count)
{
	return fdt_readfd(fdt->fd, buf, count);
}

int fdt_writefd(nbio_sockfd_t fd, const void *buf, int count)
{
	return write(fd, buf, count);
}

int fdt_write(nbio_fd_t *fdt, const void *buf, int count)
{
	return fdt_writefd(fdt->fd, buf, count);
}

int fdt_closefd(nbio_sockfd_t fd)
{
	return close(fd);
}

void fdt_close(nbio_fd_t *fdt)
{
	fdt_closefd(fdt->fd);
	return;
}

nbio_sockfd_t fdt_newsocket(int family, int type)
{
	return socket(family, type, 0);
}

int fdt_bindfd(nbio_sockfd_t fd, struct sockaddr *sa, int salen)
{
	return bind(fd, sa, salen);
}

int fdt_listenfd(nbio_sockfd_t fd)
{
	/* Queue length is pretty meaningless on most modern platforms... */
	return listen(fd, 1024);
}

/*
 * Create a new socket, bind it to the specified port, and start listening.
 *
 * IPv6 made this nice and complicated for us. Should probably actually support
 * IPv6 someday.
 */
nbio_sockfd_t fdt_newlistener(unsigned short portnum)
{
	nbio_sockfd_t sfd;
	const int on = 1;
	struct addrinfo hints, *res, *ressave;
	char serv[5];

	snprintf(serv, sizeof(serv), "%d", portnum);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(NULL /* any IP */, serv, &hints, &res) == -1)
		return -1;

	ressave = res;
	do {
		sfd = fdt_newsocket(res->ai_family, res->ai_socktype);
		if (sfd == -1)
			continue;
		setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if (fdt_bindfd(sfd, res->ai_addr, res->ai_addrlen) == 0)
			break; /* success */
		fdt_closefd(sfd);
	} while ((res = res->ai_next));
	if (!res) {
		errno = EINVAL;
		if (ressave)
			freeaddrinfo(ressave);
		return -1;
	}
	freeaddrinfo(ressave);

	if (fdt_listenfd(sfd) == -1)
		return -1;

	return sfd;
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
	socklen_t len = sizeof(error);

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

	if (getsockopt(fdt->fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1)
		error = errno;

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
	return connect(fd, addr, addrlen);
}

int fdt_connect(nbio_t *nb, const struct sockaddr *addr, int addrlen, nbio_handler_t handler, void *priv)
{
	int fd;
	struct connectinginfo *ci;
	nbio_fd_t *fdt;

	if (!nb || !addr) {
		errno = EINVAL;
		return -1;
	}

	if ((fd = fdt_newsocket(addr->sa_family, SOCK_STREAM)) == -1)
		return -1;

	if (fdt_setnonblock(fd) == -1) {
		close(fd);
		errno = EINVAL;
		return -1;
	}


	if ((fdt_connectfd(fd, (struct sockaddr *)addr, addrlen) == -1) &&
			(errno != EAGAIN) && (errno != EWOULDBLOCK) &&
			(errno != EINPROGRESS)) {
		close(fd);
		return -1;
	}

	if (!(ci = malloc(sizeof(struct connectinginfo)))) {
		close(fd);
		errno = ENOMEM;
		return -1;
	}

	ci->handler = handler;
	ci->handlerpriv = priv;

	if (!(fdt = nbio_addfd(nb, NBIO_FDTYPE_STREAM, fd, 0, fdt_connect_handler, (void *)ci, 0, 0))) {
		close(fd);
		free(ci);
		errno = EINVAL;
		return -1;
	}

	nbio_setraw(nb, fdt, 1);

	return 0;
}

nbio_sockfd_t fdt_acceptfd(nbio_sockfd_t fd, struct sockaddr *saret, int *salen)
{
	return accept(fd, saret, (socklen_t *)salen);
}

#endif

