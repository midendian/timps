
#ifndef __MODULE_H__
#define __MODULE_H__

#include <naf/nafmodule.h>
#include <naf/nafconn.h>

int naf_module__add(const char *fn);
int naf_module__add_last(const char *fn);
int naf_module__loadall(int minpri);
int naf_module__unloadall(void);
int naf_module__registerresident(const char *name, int (*firstproc)(struct nafmodule *), int startuppri);
void nafsignal(struct nafmodule *source, int signum);
void naf_module__timerrun(void);
int naf_module__protocoldetect(struct nafmodule *mod, struct nafconn *conn);
int naf_module__protocoldetecttimeout(struct nafmodule *mod, struct nafconn *conn);

void naf_module_iter(struct nafmodule *caller, int (*ufunc)(struct nafmodule *caller, struct nafmodule *curmod, void *udata), void *udata);

#endif /* __MODULE_H__ */

