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
 * Configuration stuff.
 *
 * XXX I hate this.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "nafconfig_internal.h"
#include <naf/nafmodule.h>
#include <naf/nafevents.h>
#include <naf/nafrpc.h>

#include "module.h" /* for naf_module__registerresident() */

#define MAXLINELEN 512

#define isallow(x) ((strcasecmp((x), "allow") == 0)?1:0)
#define ison(x) ((strcasecmp((x), "yes") == 0) || \
		(strcasecmp((x), "on") == 0) || \
		(strcasecmp((x), "true") == 0) || \
		(strcasecmp((x), "1") == 0))
#define isoff(x) ((strcasecmp((x), "no") == 0) || \
		(strcasecmp((x), "off") == 0) || \
		(strcasecmp((x), "false") == 0) || \
		(strcasecmp((x), "0") == 0))

struct parm_s {
	char *parm;
	char *val;
	struct parm_s *next;
};

static struct parm_s *parmlist = NULL;
static char *conffilename = NULL;

static struct nafmodule *ourmodule = NULL;


static char *nextnonwhite(char *s)
{
	if (!s)
		return NULL;

	while (*s && ((*s == ' ') || (*s == '\t')))
		s++;

	return s;
}

#if 0
/* Utility functions... */
int naf_configutil_inlist(const char *list0, const char *name)
{
	int present = 0;
	char *list, *curval;

	if (!list0 || !(list = naf_strdup(ourmodule, NAF_MEM_TYPE_GENERIC, list0)))
		return present;

	if ((curval = strtok(list, ","))) {
		do {
			if (strlen((curval = nextnonwhite(curval)))) {
				if (aim_sncmp(name, curval) == 0) {
					present = 1;
					break;
				}
			}
		} while ((curval = strtok(NULL, ",")));

	} else if (strcasecmp(name, list) == 0)
		present = 1;

	naf_free(ourmodule, list);

	return present;
}
#endif


static struct parm_s *getparm(const char *parm)
{
	struct parm_s *cur;

	for (cur = parmlist; cur; cur = cur->next) {
		if (strcmp(cur->parm, parm) == 0)
			return cur;
	}

	return NULL;
}

static struct parm_s *remparm(const char *parm)
{
	struct parm_s *cur, **prev;

	for (prev = &parmlist; (cur = *prev); ) {

		if (strcmp(cur->parm, parm) == 0) {
			*prev = cur->next;
			return cur;
		}

		prev = &cur->next;
	}

	return NULL;
}

static void freeparm(struct parm_s *p)
{

	if (p)
		naf_free(ourmodule, p->val);
	naf_free(ourmodule, p);

	return;
}

static struct parm_s *allocparm(const char *parm)
{
	struct parm_s *np;

	if (!(np = naf_malloc(ourmodule, sizeof(struct parm_s))))
		return NULL;
	memset(np, 0, sizeof(struct parm_s));

	if (!(np->parm = naf_strdup(ourmodule, parm))) {
		naf_free(ourmodule, np);
		return NULL;
	}

	return np;
}

#define MODPREFIX "module."
int naf_config_setparm(const char *parm, const char *data)
{
	struct parm_s *p;

	if ((p = remparm(parm)))
		freeparm(p);

	if (!data)
		return 0; /* calling with NULL unsets it completly */

	if (!(p = allocparm(parm)))
		return -1;

	if (!(p->val = naf_strdup(ourmodule, data))) {
		freeparm(p);
		return -1;
	}

	p->next = parmlist;
	parmlist = p;

	/* XXX module-specific changes should only get sent to the module */
	nafevent(NULL, NAF_EVENT_MOD_VARCHANGE, parm);

	return 0;
}

char *naf_config_getparmstr(const char *parm)
{
	struct parm_s *cur;

	if (!(cur = getparm(parm)))
		return NULL;

	return cur->val;
}

int naf_config_getparmbool(const char *parm)
{
	char *val;

	if (!(val = naf_config_getparmstr(parm)))
		return -1;

	if (ison(val))
		return 1;
	else if (isoff(val))
		return 0;

	return -1;
}

static char *getprefixed(const char *modname)
{
	static char var[MAXLINELEN+1];

	snprintf(var, sizeof(var), "%s%s.", MODPREFIX, modname);

	return var;
}

char *naf_config_getmodparmraw(const char *modname, const char *parm)
{
	static char var[MAXLINELEN+1];

	if (!modname || !parm)
		return NULL;

	snprintf(var, sizeof(var), "%s%s.%s", MODPREFIX, modname, parm);

	return naf_config_getparmstr(var);
}

char *naf_config_getmodparmstr(struct nafmodule *mod, const char *parm)
{

	if (!mod)
		return NULL;

	return naf_config_getmodparmraw(mod->name, parm);
}

int naf_config_getmodparmbool(struct nafmodule *mod, const char *parm)
{
	char *val;

	if (!(val = naf_config_getmodparmstr(mod, parm)))
		return -1;

	if (ison(val))
		return 1;
	else if (isoff(val))
		return 0;

	return -1;
}

static int creadln(struct nafmodule *mod, FILE *file, char *buf, int buflen)
{
	int count;

	for (count = 0; ; ) {
		int ret;

		if (feof(file)) {
			count = -1;
			break;
		}
		if ((ret = fread(buf, 1, 1, file)) < 0) {
			*buf = '\0';
			return -1;
		} else if (ret == 0) {
			*buf = '\0';
			return count;
		} else if (*buf == '\0') {
			*buf = '\0';
			return -1; 
		} else if (*buf == '\r') {
			*buf = '\0';
		} else if (*buf == '\n') {
			*buf = '\0';
			return count;
		} else {
			count++;
			buf++;
		}
	}
	*buf = '\0';

	return count;
}

/* XXX error messages go to stderr and not to naflogging... erm */
static int readconfig(struct nafmodule *mod, const char *cfn, int including)
{
	FILE *cf;
	char line[MAXLINELEN+1];
	char secname[MAXLINELEN+1];
	char modname[MAXLINELEN+1];

	if (cfn && !including) {
		if (conffilename)
			naf_free(mod, conffilename);
		conffilename = naf_strdup(mod, cfn);
	}

	if (!including && !cfn)
		cfn = conffilename;

	if (!cfn)
		return -1;

	if (!(cf = fopen(cfn, "r"))) {
		dvprintf(mod, "could not open %s: %s\n", cfn, strerror(errno));
		return -1;
	}

	dvprintf(mod, "reading configuration from %s...\n", cfn);

	memset(secname, 0, sizeof(secname));
	memset(modname, 0, sizeof(modname));

	while (creadln(mod, cf, line, sizeof(line)) >= 0) {
		char *linestart, *parm, *val;

		if (!strlen(line))
			continue;
		if (!(linestart = nextnonwhite(line)))
			continue;
		if (*linestart == ';')
			continue;
		if (*linestart == '!') {
			linestart++;

			if (strncmp(linestart, "include ", 8) == 0) {
				linestart += 8;

				if (readconfig(mod, linestart, 1) == -1) {
					dvprintf(mod, "errors parsing included file %s\n", linestart);
				}
			} else
				dvprintf(mod, "unknown ! operand %s\n", linestart);

			continue;
		}

		if (linestart[0] == '[') {

			/* Leaving the current section, clear state */
			memset(secname, 0, sizeof(secname));
			memset(modname, 0, sizeof(modname));

			linestart++;
			if (!strchr(linestart, ']'))
				continue;
			*(strchr(linestart, ']')) = '\0';
			strncpy(secname, linestart, sizeof(secname));

			if (strchr(secname, '=')) {
				char *modstart;

				modstart = strchr(secname, '=');
				*modstart = '\0';
				modstart++;

				strncpy(modname, modstart, sizeof(modname));
			}

			continue;
		}

		if (!strchr(linestart, '=')) {
			dvprintf(mod, "invalid line: %s\n", linestart);
			continue;
		}

		parm = linestart;
		if (!(val = strchr(linestart, '=')+1))
			continue;
		*(val-1) = '\0';

		if (!parm || !strlen(parm))
			continue;

		if (strcmp(secname, "site") == 0) {
			char x[MAXLINELEN+1];

			/*
			 * [site] is a free-form section. Anything defined
			 * in it will be accesible as the definition prefixed
			 * by "site.".
			 *
			 */
			snprintf(x, sizeof(x), "%s%s", "site.", parm);

			naf_config_setparm(x, (val && strlen(val)) ? val : NULL);

		} else if ((strcmp(secname, "module") == 0) && strlen(modname)) {
			char varname[MAXLINELEN+1];

			snprintf(varname, sizeof(varname), "module.%s.%s", modname, parm);
			naf_config_setparm(varname, (val && strlen(val)) ? val : NULL);

		} else {
			dvprintf(mod, "unknown declaration outside section %s: %s\n", secname, linestart);
		}
	}

	fclose(cf);

	return 0;
}

/*
 * config->setvar()
 *   IN:
 *     [optional] string module;
 *     string varname;
 *     [optional] string value;
 *
 *   OUT:
 *
 */
static void __rpc_config_setvar(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t *modarg, *vararg, *valarg;
	char buf[MAXLINELEN+1] = {""};

	if ((modarg = naf_rpc_getarg(req->inargs, "module"))) {
		if (modarg->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	if ((vararg = naf_rpc_getarg(req->inargs, "varname"))) {
		if (vararg->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}

	} else {
		req->status = NAF_RPC_STATUS_INVALIDARGS;
		return;
	}

	/*
	 * If value isn't given, unset the variable.
	 */
	if ((valarg = naf_rpc_getarg(req->inargs, "value"))) {
		if (valarg->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	snprintf(buf, sizeof(buf), "%s%s%s%s",
			modarg ? MODPREFIX : "",
			modarg ? modarg->data.string : "",
			modarg ? "." : "",
			vararg->data.string);

	if (valarg)
		naf_config_setparm(buf, valarg->data.string);
	else
		naf_config_setparm(buf, NULL);

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}

/*
 * config->getvar()
 *   IN:
 *     [optional] string module;
 *     [optional] string varname;
 *
 *   OUT:
 *     array variables {
 *         array varname {
 *             [optional] string value;
 *         }
 *     }
 *
 */
static void __rpc_config_getvar(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t *modarg, *vararg;
	naf_rpc_arg_t **head;

	if ((modarg = naf_rpc_getarg(req->inargs, "module"))) {
		if (modarg->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	if ((vararg = naf_rpc_getarg(req->inargs, "varname"))) {
		if (vararg->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	if (!(head = naf_rpc_addarg_array(mod, &req->returnargs, "variables"))) {
		req->status = NAF_RPC_STATUS_UNKNOWNFAILURE;
		return;
	}

	if (vararg) {
		const char *val = NULL;
		naf_rpc_arg_t **varhead;

		if ((varhead = naf_rpc_addarg_array(mod, head, vararg->data.string))) {

			if (modarg) {

				naf_rpc_addarg_string(mod, varhead, "module", modarg->data.string);
				val = naf_config_getmodparmraw(modarg->data.string, vararg->data.string);
			} else
				val = naf_config_getparmstr(vararg->data.string);

			naf_rpc_addarg_string(mod, varhead, "varname", vararg->data.string);

			if (val)
				naf_rpc_addarg_string(mod, varhead, "value", val);
		}
	} else {
		struct parm_s *cur;

		for (cur = parmlist; cur; cur = cur->next) {
			naf_rpc_arg_t **varhead;

			if (modarg) {
				const char *pref;

				if (!(pref = getprefixed(modarg->data.string)))
					continue;

				if (strncasecmp(cur->parm, pref, strlen(pref)) != 0)
					continue;
			}

			if ((varhead = naf_rpc_addarg_array(mod, head, cur->parm))) {
				if (cur->val)
					naf_rpc_addarg_string(mod, varhead, "value", cur->val);
			}
		}
	}

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}

static int modinit(struct nafmodule *mod)
{
	char *cfn;

	ourmodule = mod;

	if (!(cfn = naf_config_getparmstr(CONFIG_PARM_FILENAME)))
		dprintf(mod, "no config file specified, using defaults\n");

	readconfig(mod, cfn, 0);

	naf_rpc_register_method(mod, "setvar", __rpc_config_setvar, "Set a config variable");
	naf_rpc_register_method(mod, "getvar", __rpc_config_getvar, "Get the value of a config variable");

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	ourmodule = NULL;

	return 0;
}

static void signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_INFO) {

		dprintf(mod, "NAF Config Info\n");
		if (conffilename)
			dvprintf(mod, "    Using config from %s\n", conffilename);
		else
			dprintf(mod, "    Using defaults (no conf file)\n");

		{
			struct parm_s *cur;

			for (cur = parmlist; cur; cur = cur->next) {
				/* only dump site parameters */
				if (1 /*strncmp(cur->parm, "site.", strlen("site.")) == 0*/) {
					dvprintf(mod, "    %s: %s\n", cur->parm, cur->val);
				}
			}
		}

	} else if (signum == NAF_SIGNAL_RELOAD) {

		readconfig(mod, NULL, 0);

		/* notify everyone that we changed their environment */
		nafsignal(NULL, NAF_SIGNAL_CONFCHANGE);

	} else if (signum == NAF_SIGNAL_CONFCHANGE) {
		/* This MUST be a nop */
	}

	return;
}

static int modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "config");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->signal = signalhandler;

	return 0;
}

int naf_config__register(void)
{
	return naf_module__registerresident("config", modfirst, NAF_MODULE_PRI_SECONDPASS);
}

