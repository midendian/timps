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

#ifndef __NAF_H__
#define __NAF_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

int naf_init0(const char *appname, const char *appver, const char *appdesc, const char *appcopyright);
int naf_init1(int argc, char **argv);
int naf_init_final(void);
int naf_main(void);

struct naf_appinfo {
	char *nai_name;
	char *nai_version;
	char *nai_description;
	char *nai_copyright;
};
extern struct naf_appinfo naf_curappinfo;

#endif /* ndef __NAF_H__ */

