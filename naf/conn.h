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

#ifndef __CONN_H__
#define __CONN_H__

#ifndef __PLUGINREGONLY

#include <naf/nafmodule.h>
#include <naf/nafconn.h>

/* pulled in by daemon.c for the main loop */
int naf_conn__poll(int timeout);

#endif /* ndef __PLUGINREGONLY */

int naf_conn__register(void);

#endif /* __CONN_H__ */
