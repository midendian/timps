#ifndef __GNR_EVENTS_H__
#define __GNR_EVENTS_H__

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

