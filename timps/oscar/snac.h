#ifndef __SNAC_H__
#define __SNAC_H__

#include <naf/nafmodule.h>
#include <naf/naftypes.h>
#include <naf/naftlv.h>
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
int toscar_newsnacsb(struct nafmodule *mod, naf_sbuf_t *sb, naf_u16_t group, naf_u16_t subtype, naf_u16_t flags, naf_u32_t id);

int toscar_auth_sendauthinforequest(struct nafmodule *mod, struct nafconn *conn, naf_u32_t snacid, naf_tlv_t *tlvh);


struct touserinfo {
	char *sn;
	naf_u16_t evillevel;
	naf_tlv_t *tlvh;
};
void touserinfo_free(struct nafmodule *mod, struct touserinfo *toui);
struct touserinfo *touserinfo_new(struct nafmodule *mod, const char *sn);
struct touserinfo *touserinfo_extract(struct nafmodule *mod, naf_sbuf_t *sb);
int touserinfo_render(struct nafmodule *mod, struct touserinfo *toui, naf_sbuf_t *sb);

#endif /* __SNAC_H__ */

