#ifndef __NAFCONFIG_H__
#define __NAFCONFIG_H__

#include <naf/nafmodule.h>

int naf_config_setparm(const char *parm, const char *data);
char *naf_config_getparmstr(const char *parm);
int naf_config_getparmbool(const char *parm);

char *naf_config_getmodparmstr(struct nafmodule *mod, const char *parm);
int naf_config_getmodparmbool(struct nafmodule *mod, const char *parm);

#endif /* __NAFCONFIG_H__ */

