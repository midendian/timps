/*
 * naf - Networked Application Framework
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * naf is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * naf is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __NAFRPC_H__
#define __NAFRPC_H__

#include <naf/naftypes.h>

#define NAF_RPC_ARGTYPE_SCALAR  0x0000
#define NAF_RPC_ARGTYPE_ARRAY   0x0001 /* actually a list, but eh. */
#define NAF_RPC_ARGTYPE_GENERIC 0x0002
#define NAF_RPC_ARGTYPE_BOOL    0x0003
#define NAF_RPC_ARGTYPE_STRING  0x0004

#define NAF_RPC_STATUS_SUCCESS        0x0000
#define NAF_RPC_STATUS_UNKNOWNTARGET  0x0001
#define NAF_RPC_STATUS_UNKNOWNMETHOD  0x0002
#define NAF_RPC_STATUS_INVALIDARGS    0x0003
#define NAF_RPC_STATUS_PENDING        0xfffe
#define NAF_RPC_STATUS_UNKNOWNFAILURE 0xffff

typedef naf_u8_t naf_rpcu8_t;
typedef naf_u16_t naf_rpcu16_t;
typedef naf_u32_t naf_rpcu32_t;

typedef struct naf_rpc_arg_s {
	char *name;
	naf_rpcu16_t type;
	naf_rpcu16_t length;
	union {
		naf_rpcu32_t scalar;
		struct naf_rpc_arg_s *children; /* head of a list */
		void *generic;
		naf_rpcu8_t boolean;
		char *string;
	} data;
	struct naf_rpc_arg_s *next;
} naf_rpc_arg_t;

typedef struct {
	char *target;
	char *method;
	naf_rpc_arg_t *inargs;
	naf_rpcu16_t status;
	naf_rpc_arg_t *returnargs;
} naf_rpc_req_t;

naf_rpc_req_t *naf_rpc_request_new(struct nafmodule *mod, const char *targetmod, const char *method);
int naf_rpc_request_issue(struct nafmodule *mod, naf_rpc_req_t *req);
void naf_rpc_request_free(struct nafmodule *mod, naf_rpc_req_t *req);

int naf_rpc_addarg_scalar(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name, naf_rpcu32_t val);
int naf_rpc_addarg_generic(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name, unsigned char *data, int datalen);
int naf_rpc_addarg_string(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name, const char *string);
int naf_rpc_addarg_bool(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name, naf_rpcu8_t val);
naf_rpc_arg_t **naf_rpc_addarg_array(struct nafmodule *mod, naf_rpc_arg_t **head, const char *name);

typedef void (*naf_rpc_method_t)(struct nafmodule *mod, naf_rpc_req_t *req);
int naf_rpc_register_method(struct nafmodule *mod, const char *name, naf_rpc_method_t func, const char *desc);
int naf_rpc_unregister_method(struct nafmodule *mod, const char *name);

naf_rpc_arg_t *naf_rpc_getarg(naf_rpc_arg_t *head, const char *name);

#endif /* __NAFRPC_H__ */

