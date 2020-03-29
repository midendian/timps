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

/*
 * Most useless naf module evar.
 */

#include <naf/nafmodule.h>

static struct nafmodule *ourmodule = NULL;

static int modinit(struct nafmodule *mod)
{

	tprintf(mod, "module initializing!\n");

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	tprintf(mod, "module shutting down!\n");

	return 0;
}

int nafmodulemain(struct nafmodule *mod)
{

	naf_module_setname(mod, "nafbasicmodule");
	mod->init = modinit;
	mod->shutdown = modshutdown;

	return 0;
}

