
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
