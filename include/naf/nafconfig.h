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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#include <naf/nafmodule.h>

#include <stdlib.h> /* for atoi */

int naf_config_setparm(const char *parm, const char *data);
char *naf_config_getparmstr(const char *parm);
int naf_config_getparmbool(const char *parm);

char *naf_config_getmodparmstr(struct nafmodule *mod, const char *parm);
int naf_config_getmodparmbool(struct nafmodule *mod, const char *parm);

#define NAFCONFIG_UPDATEINTMODPARMDEF(_m, _p, _v, _d)               \
	do {                                                        \
		char *__str;                                        \
                                                                    \
		if ((__str = naf_config_getmodparmstr(_m, _p)))     \
			_v = atoi(__str);                           \
		if (_v == -1)                                       \
			_v = _d;                                    \
	} while (0)

/* this uses an extra variable in case _v is not a signed type */
#define NAFCONFIG_UPDATEBOOLMODPARMDEF(_m, _p, _v, _d)              \
	do {                                                        \
		int __i;                                            \
                                                                    \
		__i = naf_config_getmodparmbool(_m, _p);            \
		if (__i == -1)                                      \
			__i = _d;                                   \
		_v = __i;                                           \
	} while (0)

#define NAFCONFIG_UPDATESTRMODPARMDEF(_m, _p, _v, _d)               \
	do {                                                        \
		char *__str;                                        \
                                                                    \
		if (_v) {                                           \
			naf_free(_m, _v);                           \
			_v = NULL;                                  \
		}                                                   \
		__str = naf_config_getmodparmstr(_m, _p);           \
		if (__str)                                          \
			_v = naf_strdup(_m, __str);                 \
		if (!_v && _d)                                      \
			_v = naf_strdup(_m, _d);                    \
	} while (0)

#endif /* __NAFCONFIG_H__ */

