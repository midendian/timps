#ifndef __SNAC_H__
#define __SNAC_H__

#include <naf/nafmodule.h>
#include <naf/naftypes.h>
#include <naf/nafbufutils.h>

#define HRET_ERROR -1
#define HRET_FORWARD 0
#define HRET_DIGESTED 1

struct toscar_snac {
	naf_u16_t group;
	naf_u16_t subtype;
	naf_u16_t flags;
	naf_u32_t id;
	naf_sbuf_t extinfo;
	naf_sbuf_t payload;
};

int toscar_flap_handlesnac(struct nafmodule *mod, struct nafconn *conn, naf_u8_t *buf, naf_u16_t buflen);

#endif /* __SNAC_H__ */

