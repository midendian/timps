
#ifndef __NAFCONFIG_INTERNAL_H__
#define __NAFCONFIG_INTERNAL_H__

#include <naf/nafconfig.h>

#ifndef __PLUGINREGONLY

/* These are created from command line options, hence they are read-only. */
#define CONFIG_PARM_FILENAME        "__internal__.conffilename" /* -c */
#define CONFIG_PARM_USESYSLOG       "__internal__.usesyslog" /* -S */
#define CONFIG_PARM_DAEMONIZE       "__internal__.daemonize" /* -d */
#define CONFIG_PARM_DROPTOUSER      "__internal__.droptouser" /* -u */
#define CONFIG_PARM_DROPTOGROUP     "__internal__.droptogroup" /* -g */

#endif /* ndef __PLUGINREGONLY */

int naf_config__register(void);

#endif /* __CONFIG_H__ */
