#ifndef __CONFIGWIN32_H__
#define __CONFIGWIN32_H__

#define HAVE_STDLIB_H
#define HAVE_STDIO_H
#define HAVE_STRING_H

#define NOXML 1 /* until the expat problem is solved... */

#define NODYNAMICLOADING 1 /* for now... */
#define NOUNIXDAEMONIZATION 1
#define NOUNIXRLIMITS 1
#define NOSIGNALS 1
#define SIGUSR1 0xffffff
#define SIGUSR2 0xfffffe
#define SIGINT  0xfffffd
#define SIGHUP  0xfffffc
#define SIGCHLD 0xfffffb

#define NOVAMACROS /* le sigh. */

#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp

#define NBIO_USE_WINSOCK2 1
#include <winsock2.h>

#endif /* ndef __CONFIGWIN32_H__ */

