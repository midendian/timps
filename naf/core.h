
#ifndef __CORE_H__
#define __CORE_H__

#ifndef __PLUGINREGONLY

void __rpc_core_listmodules(struct nafmodule *mod, naf_rpc_req_t *req);

#endif /* ndef __PLUGINREGONLY */

int naf_core__register(void);

#endif /* __CORE_H__ */
