#ifndef __OSCAR_INTERNAL_H__
#define __OSCAR_INTERNAL_H__

#include <naf/nafmodule.h>

extern int timps_oscar__debug;
extern struct nafmodule *timps_oscar__module;
extern char *timps_oscar__authorizer;

#define TIMPS_OSCAR_DEFAULTPORT 5190

#define TOSCAR_SERVTYPE_UNKNOWN 0x0000
#define TOSCAR_SERVTYPE_BOS     0x0009
#define TOSCAR_SERVTYPE_AUTH    0x0017

/* the service name we use for gnr nodes/messages */
#define OSCARSERVICE "AIM"

int toscar_sncmp(const char *sn1, const char *sn2);

#endif /* ndef __OSCAR_INTERNAL_H__ */

