#ifndef __FLAP_H__
#define __FLAP_H__

#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <naf/naftypes.h>
#include <naf/nafbufutils.h>

int toscar_flap_prepareconn(struct nafmodule *mod, struct nafconn *conn);
int toscar_flap_handleread(struct nafmodule *mod, struct nafconn *conn);
int toscar_flap_handlewrite(struct nafmodule *mod, struct nafconn *conn);

int toscar_flap_sendconnclose(struct nafmodule *mod, struct nafconn *conn, naf_u16_t reason, const char *reasonurl);
int toscar_flap_puthdr(naf_sbuf_t *sb, naf_u8_t chan);
int toscar_flap_sendsbuf_consume(struct nafmodule *mod, struct nafconn *conn, naf_sbuf_t *sb);

#endif /* ndef __FLAP_H__ */

