#ifndef __NODE_H__
#define __NODE_H__

int gnr_node__register(struct nafmodule *mod);
int gnr_node__unregister(struct nafmodule *mod);
void gnr_node__timeout(struct nafmodule *mod);

#endif /* ndef __NODE_H__ */

