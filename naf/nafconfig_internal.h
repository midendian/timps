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
