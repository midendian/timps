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

#ifndef __MODULE_H__
#define __MODULE_H__

#include <naf/nafmodule.h>
#include <naf/nafconn.h>

int naf_module__add(const char *fn);
int naf_module__add_last(const char *fn);
int naf_module__loadall(int minpri);
int naf_module__unloadall(void);
int naf_module__registerresident(const char *name, int (*firstproc)(struct nafmodule *), int startuppri);
void nafsignal(struct nafmodule *source, int signum);
void naf_module__timerrun(void);
int naf_module__protocoldetect(struct nafmodule *mod, struct nafconn *conn);
int naf_module__protocoldetecttimeout(struct nafmodule *mod, struct nafconn *conn);

void naf_module_iter(struct nafmodule *caller, int (*ufunc)(struct nafmodule *caller, struct nafmodule *curmod, void *udata), void *udata);

#endif /* __MODULE_H__ */

