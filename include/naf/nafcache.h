/*
 * naf - Networked Application Framework
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * naf is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * naf is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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

