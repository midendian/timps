
#ifndef __NAFCACHE_H__
#define __NAFCACHE_H__

#include <naf/nafmodule.h>
#include <naf/naftypes.h>

#define NAF_CACHE_TIMER_FREQ 30

typedef naf_u16_t naf_cache_lid_t;
typedef void (*naf_cache_freepairfunc_t)(struct nafmodule *owner, naf_cache_lid_t lid, void *key, void *value, time_t addtime);

/*
 * This type is used in two different contexts:
 *
 *   - naf_cache_findpair: if the matcher returns 1, the search stops and 
 *                         the value is returned.
 *   - naf_cache_rempairs: if the matcher returns 1, the pair is freed,
 *                         iteration continues to end of list.
 */
typedef int (*naf_cache_matcherfunc_t)(struct nafmodule *plugin, naf_cache_lid_t lid, void *key, void *value, void *priv);

int naf_cache_register(struct nafmodule *mod);
void naf_cache_unregister(struct nafmodule *mod);
int naf_cache_addlist(struct nafmodule *mod, naf_cache_lid_t lid, int timeout, naf_cache_freepairfunc_t freepair);
int naf_cache_remlist(struct nafmodule *mod, naf_cache_lid_t lid);
int naf_cache_addpair(struct nafmodule *mod, naf_cache_lid_t lid, void *key, void *value);
int naf_cache_findpair(struct nafmodule *mod, naf_cache_lid_t lid, naf_cache_matcherfunc_t matcher, void *matcherdata, void **keyret, void **valueret, int hit);
int naf_cache_rempairs(struct nafmodule *mod, naf_cache_lid_t lid, naf_cache_matcherfunc_t matcher, void *matcherdata);

#endif /* __NAFCACHE_H__ */

