
#ifndef __NAFTLV_H__
#define __NAFTLV_H__

#include <naf/nafmodule.h>
#include <naf/naftypes.h>
#include <naf/nafbufutils.h>

typedef struct naf_tlv_s {
	naf_u16_t tlv_type;
	naf_u16_t tlv_length;
	naf_u8_t *tlv_value;
	struct naf_tlv_s *tlv_next;
} naf_tlv_t;

naf_tlv_t *naf_tlv_new(struct nafmodule *mod, naf_u16_t type, naf_u16_t length, naf_u8_t *value);
void naf_tlv_free(struct nafmodule *mod, naf_tlv_t *tlvhead);
naf_tlv_t *naf_tlv_parse(struct nafmodule *mod, naf_sbuf_t *sbuf);
int naf_tlv_render(struct nafmodule *mod, naf_tlv_t *tlv, naf_sbuf_t *destsbuf);
int naf_tlv_gettotallength(struct nafmodule *mod, naf_tlv_t *tlv);
int naf_tlv_getrenderedsize(struct nafmodule *mod, naf_tlv_t *tlv);

naf_tlv_t *naf_tlv_get(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type);
char *naf_tlv_getasstring(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type);
naf_u8_t naf_tlv_getasu8(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type);
naf_u16_t naf_tlv_getasu16(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type);
naf_u32_t naf_tlv_getasu32(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type);

int naf_tlv_addraw(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_u16_t length, const naf_u8_t *value);
int naf_tlv_addstring(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, const char *str);
int naf_tlv_addnoval(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type);
int naf_tlv_addu8(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_u8_t value);
int naf_tlv_addu16(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_u16_t value);
int naf_tlv_addu32(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_u32_t value);
int naf_tlv_addtlv(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_tlv_t *tlv); /* renders it first */
int naf_tlv_addtlvraw(struct nafmodule *mod, naf_tlv_t **desthead, naf_tlv_t *srctlv); /* clones first tlv in srctlv and adds, unrendered */

#endif /* __NAFTLV_H__ */
