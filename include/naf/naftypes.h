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

#ifndef __NAFTYPES_H__
#define __NAFTYPES_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_CONFIGW32_H
#include <configwin32.h>
#endif

typedef unsigned char naf_u8_t;
typedef unsigned short naf_u16_t;
typedef unsigned long naf_u32_t;

typedef char naf_s8_t;
typedef short naf_s16_t;
typedef long naf_s32_t;

typedef naf_u32_t naf_event_t;

#ifdef WIN32
typedef HANDLE naf_pid_t;
#else
typedef pid_t naf_pid_t;
#endif

#endif /* __NAFTYPES_H__ */

