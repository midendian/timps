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

#ifndef __NAFCONFIG_H__
#define __NAFCONFIG_H__

#include <naf/nafmodule.h>

int naf_config_setparm(const char *parm, const char *data);
char *naf_config_getparmstr(const char *parm);
int naf_config_getparmbool(const char *parm);

char *naf_config_getmodparmstr(struct nafmodule *mod, const char *parm);
int naf_config_getmodparmbool(struct nafmodule *mod, const char *parm);

#endif /* __NAFCONFIG_H__ */

