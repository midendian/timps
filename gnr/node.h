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

#ifndef __NODE_H__
#define __NODE_H__

int gnr_node__register(struct nafmodule *mod);
int gnr_node__unregister(struct nafmodule *mod);
void gnr_node__timeout(struct nafmodule *mod);

#endif /* ndef __NODE_H__ */

