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

#ifndef __NAFSTATS_H__
#define __NAFSTATS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32 
#include <configwin32.h>
#endif

#include <naf/naftypes.h>

typedef naf_u32_t naf_longstat_t;
typedef naf_u16_t naf_shortstat_t;

int naf_stats_register_longstat(struct nafmodule *owner, const char *name, naf_longstat_t *statp);
int naf_stats_register_shortstat(struct nafmodule *owner, const char *name, naf_shortstat_t *statp);
int naf_stats_unregisterstat(struct nafmodule *owner, const char *name);

int naf_stats_getstatvalue(struct nafmodule *mod, const char *name, int *typeret, void *buf, int *buflen);

#endif /* __NAFSTATS_H__ */

