
#ifndef __NAFBUFUTILS_H__
#define __NAFBUFUTILS_H__

#include <naf/nafmodule.h>
#include <naf/naftypes.h>

#define naf_byte_put8(buf, value) ( \
		(*((buf) + 0) = ((value) >>  0) & 0xff), \
		1)
#define naf_byte_put16(buf, value) ( \
		(*((buf) + 0) = ((value) >>  8) & 0xff), \
		(*((buf) + 1) = ((value) >>  0) & 0xff), \
		2)
#define naf_byte_put32(buf, value) ( \
		(*((buf) + 0) = ((value) >> 24) & 0xff), \
		(*((buf) + 1) = ((value) >> 16) & 0xff), \
		(*((buf) + 2) = ((value) >>  8) & 0xff), \
		(*((buf) + 3) = ((value) >>  0) & 0xff), \
		4)
#define naf_byte_get8(buf) ( \
		(((*((buf) + 0)) & 0xff) <<  0))
#define naf_byte_get16(buf) ( \
		(((*((buf) + 0)) & 0xff) <<  8) | \
		(((*((buf) + 1)) & 0xff) <<  0))
#define naf_byte_get32(buf) ( \
		(((*((buf) + 0)) & 0xff) << 24) | \
		(((*((buf) + 1)) & 0xff) << 16) | \
		(((*((buf) + 2)) & 0xff) <<  8) | \
		(((*((buf) + 3)) & 0xff) <<  0))


typedef struct naf_sbuf_s {
	struct nafmodule *sbuf_owner;
#define NAF_SBUF_FLAG_NONE       0x0000
#define NAF_SBUF_FLAG_FREEBUF    0x0001 /* free associated buffer upon destruction */
#define NAF_SBUF_FLAG_AUTORESIZE 0x0002
	naf_u16_t sbuf_flags;
	naf_u8_t *sbuf_buf;
	naf_u16_t sbuf_buflen;
	naf_u16_t sbuf_pos;
} naf_sbuf_t;


int naf_sbuf_init(struct nafmodule *mod, naf_sbuf_t *sbuf, naf_u8_t *buf, naf_u16_t buflen);
void naf_sbuf_free(struct nafmodule *mod, naf_sbuf_t *sbuf);
int naf_sbuf_prepend(naf_sbuf_t *sbuf, naf_u16_t bytesneeded);


/* cursor modifiers */
int naf_sbuf_getpos(naf_sbuf_t *sbuf);
int naf_sbuf_setpos(naf_sbuf_t *sbuf, naf_u16_t npos);
naf_u8_t *naf_sbuf_getposptr(naf_sbuf_t *sbuf);
int naf_sbuf_bytesremaining(naf_sbuf_t *sbuf);
int naf_sbuf_rewind(naf_sbuf_t *sbuf);
int naf_sbuf_advance(naf_sbuf_t *sbuf, naf_u16_t n);


/* data manipulators */
naf_u8_t naf_sbuf_get8(naf_sbuf_t *sbuf);
naf_u16_t naf_sbuf_get16(naf_sbuf_t *sbuf);
naf_u32_t naf_sbuf_get32(naf_sbuf_t *sbuf);
int naf_sbuf_getrawbuf(naf_sbuf_t *sbuf, naf_u8_t *outbuf, naf_u16_t len);
naf_u8_t *naf_sbuf_getraw(struct nafmodule *mod, naf_sbuf_t *sbuf, naf_u16_t len);
char *naf_sbuf_getstr(struct nafmodule *mod, naf_sbuf_t *sbuf, naf_u16_t len);

int naf_sbuf_put8(naf_sbuf_t *sbuf, naf_u8_t v);
int naf_sbuf_put16(naf_sbuf_t *sbuf, naf_u16_t v);
int naf_sbuf_put32(naf_sbuf_t *sbuf, naf_u32_t v);
int naf_sbuf_putraw(naf_sbuf_t *sbuf, const naf_u8_t *inbuf, int inbuflen);
int naf_sbuf_putstr(naf_sbuf_t *sbuf, const char *instr);

int naf_sbuf_cmp(naf_sbuf_t *sbuf, const naf_u8_t *cmpbuf, int cmpbuflen);

#endif /* __NAFBUFUTILS_H__ */

