/*
 * timps - Transparent Instant Messaging Proxy Server
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * timps is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * timps is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __OSCAR_INTERNAL_H__
#define __OSCAR_INTERNAL_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#include <naf/nafmodule.h>

extern int timps_oscar__debug;
extern struct nafmodule *timps_oscar__module;
extern char *timps_oscar__authorizer;

#define TIMPS_OSCAR_DEFAULTPORT 5190

#define TOSCAR_SERVTYPE_UNKNOWN 0x0000
#define TOSCAR_SERVTYPE_BOS     0x0009
#define TOSCAR_SERVTYPE_AUTH    0x0017

/* the service name we use for gnr nodes/messages */
#define OSCARSERVICE "AIM"

int toscar_sncmp(const char *sn1, const char *sn2);

#endif /* ndef __OSCAR_INTERNAL_H__ */

