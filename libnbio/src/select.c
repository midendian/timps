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

/*
 * Not all UNIXy platforms have a working poll (eg, OS X Tiger).  The select
 * support should be the lowest common denominator, so we avoid things like
 * FD_COPY() that would be helpful but are non-portable.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(NBIO_USE_KQUEUE) && !defined(NBIO_USE_WINSOCK2) && defined(NBIO_USE_SELECT)

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_SYS_POLL_H
#include <sys/select.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libnbio.h>
#include "impl.h"

#define NBIO_PFD_INVAL -1

/* nbio_t->intdata */
struct nbdata {
	int maxfd;
};

int pfdinit(nbio_t *nb, int pfdsize)
{
	struct nbdata *nbd;

	if (!(nbd = nb->intdata = malloc(sizeof(struct nbdata))))
		return -1;

	nbd->maxfd = -1;

	return 0;
}

/* This kills the nbio_t, not a pfd -- confusing name. */
void pfdkill(nbio_t *nb)
{
	struct nbdata *nbd = (struct nbdata *)nb->intdata;

	free(nbd);
	nb->intdata = NULL;

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
		struct fdtdata *data = (struct fdtdata *)cur->intdata;

		if (data->flags == WANT_NONE)
			continue;

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

	if (timeout != -1) {
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

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
	if ((selret = select(nbd->maxfd+1, &rfds, &wfds, NULL,
			     (timeout == -1) ? NULL : &tv)) == -1) {

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

	setmaxfd(nb);

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

	setmaxfd(nb);

	return;
}

void fdt_setpollnone(nbio_t *nb, nbio_fd_t *fdt)
{
	struct nbdata *nbd = (struct nbdata *)nb->intdata;
	struct fdtdata *data = (struct fdtdata *)fdt->intdata;

	data->flags &= ~(WANT_READ | WANT_WRITE);

	setmaxfd(nb);

	return;
}

#endif /* !def KQUEUE && !def WINSOCK2 */

