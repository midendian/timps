
#ifndef __CORE_H__
#define __CORE_H__

#include <naf/nafmodule.h>
#include <gnr/gnrevents.h>

extern int gnr__debug;
extern struct nafmodule *gnr__module;

int gnr_event_throw(struct gnr_event_info *gei);

#endif /* ndef __CORE_H__ */

