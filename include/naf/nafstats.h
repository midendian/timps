
#ifndef __NAFSTATS_H__
#define __NAFSTATS_H__

#include <naf/naftypes.h>

typedef naf_u32_t naf_longstat_t;
typedef naf_u16_t naf_shortstat_t;

int naf_stats_register_longstat(struct nafmodule *owner, const char *name, naf_longstat_t *statp);
int naf_stats_register_shortstat(struct nafmodule *owner, const char *name, naf_shortstat_t *statp);
int naf_stats_unregisterstat(struct nafmodule *owner, const char *name);

int naf_stats_getstatvalue(struct nafmodule *mod, const char *name, int *typeret, void *buf, int *buflen);

#endif /* __NAFSTATS_H__ */
