/*
 * gnr - Generic interNode message Routing
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * gnr is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * gnr is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __CORE_H__
#define __CORE_H__

#include <naf/nafmodule.h>
#include <gnr/gnrevents.h>

extern int gnr__debug;
extern struct nafmodule *gnr__module;

int gnr_event_throw(struct gnr_event_info *gei);

#endif /* ndef __CORE_H__ */

