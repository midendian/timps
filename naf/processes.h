
#ifndef __PROCESSES_H__
#define __PROCESSES_H__

#include <naf/nafconn.h>

void naf_childproc__sigchild_handler(void);
void naf_childproc_cleanconn(struct nafconn *conn);

#endif /* def __PROCESSES_H__ */

