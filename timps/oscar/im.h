/*
 * timps - Transparent Instant Messaging Proxy Server
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * timps is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * timps is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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

