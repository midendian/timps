/* -*- Mode: ab-c -*- */

#ifndef __IMPL_H__
#define __IMPL_H__

/* Common API */

/* provided by implementation */
nbio_sockfd_t fdt_newsocket(nbio_t *nb, int family, int type);
int fdt_connect(nbio_t *nb, const struct sockaddr *addr, int addrlen, nbio_handler_t handler, void *priv);
int fdt_read(nbio_fd_t *fdt, void *buf, int count);
int fdt_write(nbio_fd_t *fdt, const void *buf, int count);
void fdt_close(nbio_fd_t *fdt);
int fdt_setnonblock(nbio_sockfd_t fd);
int pfdinit(nbio_t *nb, int pfdsize);
void pfdkill(nbio_t *nb);
int pfdadd(nbio_t *nb, nbio_fd_t *newfd);
void pfdaddfinish(nbio_t *nb, nbio_fd_t *newfd);
void pfdrem(nbio_t *nb, nbio_fd_t *fdt);
void pfdfree(nbio_fd_t *fdt);
int pfdpoll(nbio_t *nb, int timeout);
void fdt_setpollin(nbio_t *nb, nbio_fd_t *fdt, int val);
void fdt_setpollout(nbio_t *nb, nbio_fd_t *fdt, int val);
void fdt_setpollnone(nbio_t *nb, nbio_fd_t *fdt);

/* provided by libnbio.c */
void __fdt_free(nbio_fd_t *fdt);
int __fdt_ready_in(nbio_t *nb, nbio_fd_t *fdt);
int __fdt_ready_out(nbio_t *nb, nbio_fd_t *fdt);
int __fdt_ready_eof(nbio_t *nb, nbio_fd_t *fdt);
int __fdt_ready_all(nbio_t *nb, nbio_fd_t *fdt);

#endif /* __IMPL_H__ */

