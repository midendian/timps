/* -*- Mode: ab-c -*- */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <errno.h>

#include <libnbio.h>
#include "impl.h"

static nbio_buf_t *getrxbuf(nbio_fd_t *fdt)
{
	nbio_buf_t *ret;

	if (!fdt || !fdt->rxchain_freelist) {
		errno = EINVAL;
		return NULL;
	}

	ret = fdt->rxchain_freelist;
	fdt->rxchain_freelist = fdt->rxchain_freelist->next;

	return ret;
}

static nbio_buf_t *gettxbuf(nbio_fd_t *fdt)
{
	nbio_buf_t *ret;

	if (!fdt || !fdt->txchain_freelist) {
		errno = EINVAL;
		return NULL;
	}

	ret = fdt->txchain_freelist;
	fdt->txchain_freelist = fdt->txchain_freelist->next;

	return ret;
}

static void givebackrxbuf(nbio_fd_t *fdt, nbio_buf_t *buf)
{
	buf->next = fdt->rxchain_freelist;
	fdt->rxchain_freelist = buf;

	return;
}

static void givebacktxbuf(nbio_fd_t *fdt, nbio_buf_t *buf)
{
	buf->next = fdt->txchain_freelist;
	fdt->txchain_freelist = buf;

	return;
}

int nbio_addrxvector_time(nbio_t *nb, nbio_fd_t *fdt, unsigned char *buf, int buflen, int offset, time_t trigger)
{
	nbio_buf_t *newbuf, *cur;

	if (!(newbuf = getrxbuf(fdt))) {
		errno = ENOMEM;
		return -1;
	}

	newbuf->data = buf;
	newbuf->len = buflen;
	newbuf->offset = offset;
	newbuf->trigger = trigger;
	newbuf->next = NULL;

	if (fdt->rxchain) {
		for (cur = fdt->rxchain; cur->next; cur = cur->next)
			;
		cur->next = newbuf;
	} else
		fdt->rxchain = newbuf;

	if (fdt->rxchain)
		fdt_setpollin(nb, fdt, 1);

	return 0;
}

int nbio_addrxvector(nbio_t *nb, nbio_fd_t *fdt, unsigned char *buf, int buflen, int offset)
{
	return nbio_addrxvector_time(nb, fdt, buf, buflen, offset, 0); /* ASAP */
}

int nbio_remrxvector(nbio_t *nb, nbio_fd_t *fdt, unsigned char *buf)
{
	nbio_buf_t *cur = NULL;

	if (!fdt->rxchain)
		;
	else if (fdt->rxchain->data == buf) {
		cur = fdt->rxchain;
		fdt->rxchain = fdt->rxchain->next;
	} else {
		for (cur = fdt->rxchain; cur->next; cur = cur->next) {
			if (cur->next->data == buf) {
				nbio_buf_t *tmp;

				tmp = cur->next;
				cur->next = cur->next->next;
				cur = tmp;

				break;
			}
		}
	}

	if (!cur) {
		errno = ENOENT;
		return -1;
	}

	givebackrxbuf(fdt, cur);

	if (!fdt->rxchain)
		fdt_setpollin(nb, fdt, 0);

	return 0; /* caller must free the region */
}

unsigned char *nbio_remtoprxvector(nbio_t *nb, nbio_fd_t *fdt, int *len, int *offset)
{
	nbio_buf_t *ret;
	unsigned char *buf;

	if (!fdt) {
		errno = EINVAL;
		return NULL;
	}

	if (!fdt->rxchain) {
		errno = ENOENT;
		return NULL;
	}

	ret = fdt->rxchain;
	fdt->rxchain = fdt->rxchain->next;

	if (len)
		*len = ret->len;
	if (offset)
		*offset = ret->offset;
	buf = ret->data;

	givebackrxbuf(fdt, ret);

	if (!fdt->rxchain)
		fdt_setpollin(nb, fdt, 0);

	return buf;
}

int nbio_addtxvector_time(nbio_t *nb, nbio_fd_t *fdt, unsigned char *buf, int buflen, time_t trigger)
{
	nbio_buf_t *newbuf;

	if (!fdt || !buf || !buflen) {
		errno = EINVAL;
		return -1;
	}

	if (!(newbuf = gettxbuf(fdt))) {
		errno = ENOMEM;
		return -1;
	}

	newbuf->data = buf;
	newbuf->len = buflen;
	newbuf->offset = 0;
	newbuf->trigger = trigger;
	newbuf->next = NULL;

	if (fdt->txchain_tail) {
		fdt->txchain_tail->next = newbuf;
		fdt->txchain_tail = fdt->txchain_tail->next;
	} else
		fdt->txchain = fdt->txchain_tail = newbuf;

	if (fdt->txchain)
		fdt_setpollout(nb, fdt, 1);

	return 0;
}

int nbio_addtxvector(nbio_t *nb, nbio_fd_t *fdt, unsigned char *buf, int buflen)
{
	return nbio_addtxvector_time(nb, fdt, buf, buflen, 0);
}

int nbio_rxavail(nbio_t *nb, nbio_fd_t *fdt)
{
	return !!fdt->rxchain_freelist;
}

int nbio_txavail(nbio_t *nb, nbio_fd_t *fdt)
{
	return !!fdt->txchain_freelist;
}

int nbio_remtxvector(nbio_t *nb, nbio_fd_t *fdt, unsigned char *buf)
{
	nbio_buf_t *cur = NULL;

	if (!fdt->txchain)
		;
	else if (fdt->txchain->data == buf) {
		cur = fdt->txchain;
		fdt->txchain = fdt->txchain->next;
	} else {
		for (cur = fdt->txchain; cur->next; cur = cur->next) {
			if (cur->next->data == buf) {
				nbio_buf_t *tmp;

				tmp = cur->next;
				cur->next = cur->next->next;
				cur = tmp;

				break;
			}
		}
	}

	if (!cur) {
		errno = ENOENT;
		return -1;
	}

	if (cur == fdt->txchain_tail)
		fdt->txchain_tail = fdt->txchain_tail->next;
	if (!fdt->txchain_tail)
		fdt->txchain_tail = fdt->txchain;

	givebacktxbuf(fdt, cur);

	if (!fdt->txchain)
		fdt_setpollout(nb, fdt, 0);

	return 0; /* caller must free the region */
}

unsigned char *nbio_remtoptxvector(nbio_t *nb, nbio_fd_t *fdt, int *len, int *offset)
{
	nbio_buf_t *ret;
	unsigned char *buf;

	if (!fdt) {
		errno = EINVAL;
		return NULL;
	}

	if (!fdt->txchain) {
		errno = ENOENT;
		return NULL;
	}

	ret = fdt->txchain;
	fdt->txchain = fdt->txchain->next;

	if (ret == fdt->txchain_tail)
		fdt->txchain_tail = fdt->txchain;

	if (len)
		*len = ret->len;
	if (offset)
		*offset = ret->offset;
	buf = ret->data;

	givebacktxbuf(fdt, ret);

	if (!fdt->txchain)
		fdt_setpollout(nb, fdt, 0);

	return buf;
}

