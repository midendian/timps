
#ifndef __NAF_EVENTS_H__
#define __NAF_EVENTS_H__

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
int nafevent(struct nafmodule *mod, naf_event_t event, ...);

#define dprintf(p, x) nafevent(p, NAF_EVENT_GENERICOUTPUT, x)
#define dvprintf(p, x, y...) nafevent(p, NAF_EVENT_GENERICOUTPUT, x, y)
#define dperror(p, x) nafevent(p, NAF_EVENT_DEBUGOUTPUT, "%s: %s\n", x, strerror(errno))

#endif /* __NAF_EVENTS_H__ */

