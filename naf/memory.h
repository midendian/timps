#ifndef __MEMORY_H__
#define __MEMORY_H__

void naf_memory__module_free(struct nafmodule *mod); /* called in module.c */
#ifdef __NAFRPC_H__
void __rpc_core_modmemoryuse(struct nafmodule *mod, naf_rpc_req_t *req);
#endif

extern int naf_memory_debug; /* core.c */

#endif

