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

#ifndef __NAF_EVENTS_H__
#define __NAF_EVENTS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <naf/naftypes.h>
#include <naf/nafmodule.h>

/* NOTE: naf_event_t needs to be individual bits */

#define NAF_EVENT_GENERICOUTPUT          (naf_event_t) 0x00000001
#define NAF_EVENT_DEBUGOUTPUT            (naf_event_t) 0x00000002
#define NAF_EVENT_ADMINOUTPUT            (naf_event_t) 0x00000004

#define NAF_EVENT_MOD_VARCHANGE          (naf_event_t) 0x00000100

/*
 * Throw an event.
 */
int nafeventv(struct nafmodule *source, naf_event_t event, va_list inap);
int nafevent(struct nafmodule *mod, naf_event_t event, ...);

#define tprintf(p, x) nafevent(p, NAF_EVENT_GENERICOUTPUT, x)
#ifdef NOVAMACROS
int tvprintf(struct nafmodule *mod, ...);
#else
#define tvprintf(p, x, y...) nafevent(p, NAF_EVENT_GENERICOUTPUT, x, y)
#endif
#define tperror(p, x) nafevent(p, NAF_EVENT_DEBUGOUTPUT, "%s: %s\n", x, strerror(errno))

#endif /* __NAF_EVENTS_H__ */

