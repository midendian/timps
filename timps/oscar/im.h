#ifndef __IM_H__
#define __IM_H__

#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <gnr/gnrmsg.h>

#include "snac.h"

int toscar_snachandler_0004_0006(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac);
int toscar_snachandler_0004_0007(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac);

int toscar_icbm_sendincoming(struct nafmodule *mod, struct nafconn *conn, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi);
int toscar_icbm_sendoutgoing(struct nafmodule *mod, struct nafconn *conn, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi);

#endif /* ndef __IM_H__ */

