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

#ifndef __NAFTAG_H__
#define __NAFTAG_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_CONFIGW32_H
#include <configwin32.h>
#endif

#include <naf/nafmodule.h>

/*
 * 'base class' API for tags.
 */
int naf_tag_add(void **taglistv, struct nafmodule *mod, const char *name, char type, void *data);
int naf_tag_remove(void **taglistv, struct nafmodule *mod, const char *name, char *typeret, void **dataret);
int naf_tag_ispresent(void **taglistv, struct nafmodule *mod, const char *name);
int naf_tag_fetch(void **taglistv, struct nafmodule *mod, const char *name, char *typeret, void **dataret);
void naf_tag_iter(void **taglistv, struct nafmodule *mod, int (*uf)(struct nafmodule *mod, void *ud, const char *tagname, char tagtype, void *tagdata), void *ud);
void naf_tag_freelist(void **taglistv, void *user);
int naf_tag_cloneall(void **desttaglistv, void **srctaglistv);

#endif /* __NAFTAG_H__ */

