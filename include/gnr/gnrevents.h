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

#ifndef __GNR_EVENTS_H__
#define __GNR_EVENTS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#include <naf/nafmodule.h>
#include <naf/naftypes.h>


typedef naf_u32_t gnr_event_t;
#define GNR_EVENT_UNDEFINED         (gnr_event_t) 0x00000000
#define    GNR_EVENTMASK_NONE       (gnr_event_t) (GNR_EVENT_UNDEFINED)
#define GNR_EVENT_NODEUP            (gnr_event_t) 0x00000001
#define GNR_EVENT_NODEDOWN          (gnr_event_t) 0x00000002
#define GNR_EVENT_NODEFLAGCHANGE    (gnr_event_t) 0x00000004
#define    GNR_EVENTMASK_NODE       (gnr_event_t) (GNR_EVENT_NODEUP | \
		                                     GNR_EVENT_NODEDOWN | \
		                                     GNR_EVENT_NODEFLAGCHANGE)
#define    GNR_EVENTMASK_ALL        (gnr_event_t) 0xffffffff

struct gnr_event_info {
	gnr_event_t gei_event;
	union {
		struct gnrnode *gei_obj_node;
	} gei_object;
	void *gei_extinfo;
#define gei_node gei_object.gei_obj_node
};

typedef void (*gnr_eventhandlerfunc_t)(struct nafmodule *, struct gnr_event_info *);

int gnr_event_register(struct nafmodule *mod, gnr_eventhandlerfunc_t evfunc, gnr_event_t evmask);
int gnr_event_unregister(struct nafmodule *mod, gnr_eventhandlerfunc_t func);


#endif /* ndef __GNR_EVENTS_H__ */

