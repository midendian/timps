#ifndef __CKCACHE_H__
#define __CKCACHE_H__

#include <time.h>
#include <naf/nafmodule.h>
#include <naf/naftypes.h>

int toscar_ckcache_add(struct nafmodule *mod, naf_u8_t *ck, naf_u16_t cklen, const char *ip, const char *sn, naf_u16_t servtype);
int toscar_ckcache_rem(struct nafmodule *mod, naf_u8_t *ck, naf_u16_t cklen, char **ipret, char **snret, naf_u16_t *servtyperet);
void toscar_ckcache_timer(struct nafmodule *mod, time_t now);


#endif /* ndef __CKCACHE_H__ */

