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

#ifdef NBIO_USE_KQUEUE

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/event.h>

#include <libnbio.h>
#include "impl.h"

static struct kevent *getnextchange(nbio_t *nb)
{
	struct kevent *ret = NULL;

	if (nb->kqchangecount >= nb->kqchangeslen)
		return NULL;

	ret = nb->kqchanges+nb->kqchangecount;
	nb->kqchangecount++;

	return ret;
}

/*
 * XXX this API generates superfluous calls to kevent...
 * should make use of kevent's changelist functionality.
 */
static void fdt_setpollin(nbio_t *nb, nbio_fd_t *fdt, int val)
{
	struct kevent *kev;

	if (!nb || !fdt)
		return;

	if (!(kev = getnextchange(nb))) {
		fprintf(stderr, "libnbio: fdt_setpollin: getnextchange failed!\n");
		return;
	}

	kev->ident = fdt->fd;
	kev->filter = EVFILT_READ;
	kev->flags = val ? EV_ADD : EV_DELETE;
	kev->fflags = 0;
	kev->udata = (void *)fdt;

	return;
}

static void fdt_setpollout(nbio_t *nb, nbio_fd_t *fdt, int val)
{
	struct kevent *kev;

	if (!nb || !fdt)
		return;

	if (!(kev = getnextchange(nb))) {
		fprintf(stderr, "libnbio: fdt_setpollout: getnextchange failed!\n");
		return;
	}

	kev->ident = fdt->fd;
	kev->filter = EVFILT_WRITE;
	kev->flags = val ? EV_ADD : EV_DELETE;
	kev->fflags = 0;
	kev->udata = (void *)fdt;

	return;
}

static void fdt_setpollnone(nbio_t *nb, nbio_fd_t *fdt)
{
	if (!nb || !fdt)
		return;

	fdt_setpollin(nb, fdt, 0);
	fdt_setpollout(nb, fdt, 0);

	return;
}

int pfdadd(nbio_t *nb, nbio_fd_t *newfd)
{
	return 0; /* not used for kqueue */
}

void pfdaddfinish(nbio_t *nb, nbio_fd_t *newfd)
{
	return; /* not used for kqueue */
}

void pfdfree(nbio_fd_t *fdt)
{
	return; /* not used */
}

int pfdinit(nbio_t *nb, int pfdsize)
{

	if (!nb || (pfdsize <= 0))
		return -1;

	nb->kqeventslen = pfdsize;
	nb->kqchangeslen = pfdsize*2;

	if (!(nb->kqevents = malloc(sizeof(struct kevent)*nb->kqeventslen)))
		return -1;
	if (!(nb->kqchanges = malloc(sizeof(struct kevent)*nb->kqchangeslen))) {
		free(nb->kqevents);
		return -1;
	}

	nb->kqchangecount = 0;

	if ((nb->kq = kqueue()) == -1) {
		int sav;

		sav = errno;
		free(nb->kqevents);
		free(nb->kqchanges);
		errno = sav;

		return -1;
	}

	return 0;
}

void pfdkill(nbio_t *nb)
{

	/* XXX I guess... the inverse of kqueue() isn't documented... */
	close(nb->kq);

	free(nb->kqevents);
	nb->kqeventslen = 0;

	free(nb->kqchanges);
	nb->kqchangeslen = 0;
	nb->kqchangecount = 0;

	return;
}

int pfdpoll(nbio_t *nb, int timeout)
{
	struct timespec to;
	int kevret, i;
	nbio_fd_t *fdt;

	if (!nb) {
		errno = EINVAL;
		return -1;
	}

	if (timeout > 0) {
		to.tv_sec = 0;
		to.tv_nsec = timeout*1000;
	}

	errno = 0;

	if ((kevret = kevent(nb->kq, nb->kqchanges, nb->kqchangecount, nb->kqevents, nb->kqeventslen, (timeout > 0)?&to:NULL)) == -1) {
		perror("kevent");
		return -1;
	}

	/* As long as it doesn't return -1, the changelist has been processed */
	nb->kqchangecount = 0;

	if (kevret == 0)
		return 0;

	for (i = 0; i < kevret; i++) {

		if (!nb->kqevents[i].udata) {
			fprintf(stderr, "no udata!\n");
			continue;
		}

		fdt = (nbio_fd_t *)nb->kqevents[i].udata;

		if (nb->kqevents[i].filter == EVFILT_READ) {

			if (fdt->type == NBIO_FDTYPE_LISTENER) {
				if (fdt->handler(nb, NBIO_EVENT_READ, fdt) == -1)
					return -1;
			} else if (fdt->type == NBIO_FDTYPE_STREAM) {
				if (streamread(nb, fdt) == -1)
					return -1;
			} else if (fdt->type == NBIO_FDTYPE_DGRAM) {
				if (dgramread(nb, fdt) == -1)
					return -1;
			}

		} else if (nb->kqevents[i].filter == EVFILT_WRITE) {

			if (fdt->type == NBIO_FDTYPE_LISTENER)
				; /* invalid */
			else if (fdt->type == NBIO_FDTYPE_STREAM) {
				if (streamwrite(nb, fdt) == -1)
					return -1;
			} else if (fdt->type == NBIO_FDTYPE_DGRAM) {
				if (dgramwrite(nb, fdt) == -1)
					return -1;
			}
		}

		if (nb->kqevents[i].flags & EV_EOF) {
			if (fdt->handler(nb, NBIO_EVENT_EOF, fdt) == -1)
				return -1;
		}

	}

	return nbio_cleanuponly(nb);
}

#endif /* def NBIO_USE_KQUEUE */

