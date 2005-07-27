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
 * This allows you to interactively make in-process NAF RPC
 * calls.  It's great for debugging.
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
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#endif
#ifdef HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif

#include <naf/naf.h>
#include <naf/nafmodule.h>
#include <naf/nafrpc.h>
#include <naf/nafconfig.h>

#ifdef NAF_OLDREADLINE
#define rl_completion_matches completion_matches
#endif

static struct nafmodule *nafconsole__module = NULL;
static char *nafconsole__prompt = NULL;
#define NAFCONSOLE_DEBUG_DEFAULT 0
static int nafconsole__debug = NAFCONSOLE_DEBUG_DEFAULT;

#define ISWHITE(x) (((x) == ' ') || ((x) == '\t') || ((x) == '\n'))

typedef int (*rlfunc)(char *arg);

typedef struct {
	char *name;
	rlfunc func;
	const char *doc;
} cmd_t;

static int cmd_call(char *arg);

/*
 * XXX remove this.  There is only one "command", so it is pointless.
 * Should be able to do just:
 *	timps>core->listmodules()
 * Or, better:
 *	timps>cor<tab>li<tab>
 */
static cmd_t cmdlist[] = {
	{ "call", cmd_call, "Invoke a NAF RPC method"},
	{ (char *)NULL, (rlfunc)NULL, (char *)NULL }
};

static void dumpargs(naf_rpc_arg_t *head, int depth)
{
	naf_rpc_arg_t *arg;

	for (arg = head; arg; arg = arg->next) {

		{
			int i;

			for (i = 0; i < depth; i++)
				printf("\t");
		}

		if (arg->type == NAF_RPC_ARGTYPE_SCALAR)
			printf("[scalar] %s = %lu\n", arg->name, arg->data.scalar);
		else if (arg->type == NAF_RPC_ARGTYPE_GENERIC)
			printf("[generic] %s = [...]\n", arg->name);
		else if (arg->type == NAF_RPC_ARGTYPE_BOOL)
			printf("[bool] %s = %s\n", arg->name, arg->data.boolean ? "true" : "false");
		else if (arg->type == NAF_RPC_ARGTYPE_STRING)
			printf("[string] %s = '%s'\n", arg->name, arg->data.string);
		else if (arg->type == NAF_RPC_ARGTYPE_ARRAY) {
			printf("[array] %s...\n", arg->name);
			dumpargs(arg->data.children, depth + 1);
		}
	}

	return;
}

static const char *stripwhitefront(const char *c)
{
	while (c && ISWHITE(*c))
		c++;
	return c;
}

static int parsearg(naf_rpc_req_t *req, char *argspec, char *val)
{
	static const char *deftypestr = {"string"};
	const char *typestr = deftypestr;
	char *name;
	int type;

	name = argspec;
	if (index(name, '[') && index(name, ']')) {
		char *t;

		t = index(name, '[');
		*(t++) = '\0';
		*(index(t, ']')) = '\0';

		typestr = t;
	}

	name = (char *)stripwhitefront(name);
	typestr = stripwhitefront(typestr);

	if (!typestr || !strlen(typestr)) {
		printf("no type specified for argument '%s'\n", name);
		return -1;
	}

	/* XXX support generic and array */
	if (strcasecmp(typestr, "scalar") == 0)
		type = NAF_RPC_ARGTYPE_SCALAR;
	else if ((strcasecmp(typestr, "bool") == 0) ||
			(strcasecmp(typestr, "boolean") == 0))
		type = NAF_RPC_ARGTYPE_BOOL;
	else if (strcasecmp(typestr, "string") == 0)
		type = NAF_RPC_ARGTYPE_STRING;
	else {
		printf("unknown type '%s' specified for argument '%s'\n", typestr, name);
		return -1;
	}

	if (type == NAF_RPC_ARGTYPE_SCALAR) {

		if (naf_rpc_addarg_scalar(nafconsole__module, &req->inargs, name, (naf_rpcu32_t)atoi(val)) == -1) {
			printf("addarg_scalar failed\n");
			return -1;
		}

	} else if (type == NAF_RPC_ARGTYPE_BOOL) {
		int on;

		if ((strcasecmp(val, "true") == 0) ||
				(strcasecmp(val, "yes") == 0) ||
				(strcasecmp(val, "on") == 0) ||
				(strcasecmp(val, "1") == 0))
			on = 1;
		else if ((strcasecmp(val, "false") == 0) ||
				(strcasecmp(val, "no") == 0) ||
				(strcasecmp(val, "off") == 0) ||
				(strcasecmp(val, "0") == 0))
			on = 0;
		else {
			printf("invalid boolean value '%s'\n", val);
			return -1;
		}

		if (naf_rpc_addarg_bool(nafconsole__module, &req->inargs, name, (naf_rpcu8_t)on) == -1) {
			printf("addarg_bool failed\n");
			return -1;
		}

	} else if (type == NAF_RPC_ARGTYPE_STRING) {

		if (naf_rpc_addarg_string(nafconsole__module, &req->inargs, name, val) == -1) {
			printf("addarg_string failed\n");
			return -1;
		}
	}

	return 0;
}

static int splitandparseargs(naf_rpc_req_t *req, char *args)
{
	char *start, *eq;
	char *c;
	int quoted;

	args = (char *)stripwhitefront(args);
	if (!args || !strlen(args))
		return 0;

	/*
	 * arg[string]="value",arg2[scalar]=42,arg3[type]="value"
	 */
	for (c = args, start = NULL, eq = NULL, quoted = 0; ; ) {

		if (!start)
			start = c;

		if (start && !eq && (*c == '=')) {

			eq = c++;
			*(eq++) = '\0';

		} else if (start && eq && (*c == '"')) {

			quoted = !quoted;
			*c = '\0';
			if (eq == c)
				eq++; /* move eq past quote */
			c++;

		} else if (!quoted && start && eq && ((*c == ',') || (*c == '\0'))) {

			if (*c == ',')
				*(c++) = '\0';

			if (parsearg(req, start, eq) == -1)
				return -1;

			eq = start = NULL;

			if (*c == '\0')
				break;

		} else {
			c++;
			if (c == '\0')
				break;
		}

	}

	return 0;
}

static int cmd_call(char *arg)
{
	char *target, *method, *args = NULL;
	naf_rpc_req_t *req;

	if (!(method = strstr((target = arg), "->"))) {
		printf("must specify method in target->method() syntax\n");
		return 0;
	}

	*method = '\0';
	method += strlen("->");

	if (index(method, '(')) {
		args = index(method, '(');
		*args = '\0';
		args++;

		if (index(args, ')'))
			*(index(args, ')')) = '\0';
	}

	if (!strlen(target) || !strlen(method)) {
		printf("no target and/or method specified\n");
		return 0;
	}

	printf("target=%s, method=%s, args=%s\n", target, method, args);

	if (!(req = naf_rpc_request_new(nafconsole__module, target, method))) {
		printf("naf_rpc_request_new failed\n");
		return 0;
	}

	if (args && (splitandparseargs(req, args) == -1)) {
		printf("argument parsing failed\n");
		naf_rpc_request_free(nafconsole__module, req);
		return 0;
	}

	printf("input arguments:\n");
	dumpargs(req->inargs, 1);

	if (naf_rpc_request_issue(nafconsole__module, req) == -1)  {
		printf("naf_rpc_request_issue failed\n");
		goto out;
	}

	if (req->status == NAF_RPC_STATUS_SUCCESS) {

		printf("rpc: request successful\n");

		printf("return values:\n");
		dumpargs(req->returnargs, 1);

	} else if (req->status == NAF_RPC_STATUS_INVALIDARGS)
		printf("rpc: invalid arguments\n");
	else if (req->status == NAF_RPC_STATUS_UNKNOWNFAILURE)
		printf("rpc: unknown failure\n");
	else if (req->status == NAF_RPC_STATUS_UNKNOWNTARGET)
		printf("rpc: unknown target\n");
	else if (req->status == NAF_RPC_STATUS_UNKNOWNMETHOD)
		printf("rpc: unknown method\n");
	else if (req->status == NAF_RPC_STATUS_PENDING)
		printf("rpc: request pending\n");
	else
		printf("rpc: unknown return status %d\n", req->status);

out:
	naf_rpc_request_free(nafconsole__module, req);

	return 0;
}


struct macro {
	char *ma_cmd; /* what the user types */
	char *ma_def; /* what it expands to */
	struct macro *ma__next;
};
static struct macro *nafconsole__macrolist;

static struct macro *macro_find(const char *cmd)
{
	struct macro *ma;

	for (ma = nafconsole__macrolist; ma; ma = ma->ma__next) {
		if (strcmp(ma->ma_cmd, cmd) == 0)
			return ma;
	}
	return NULL;
}

static void macro__free(struct macro *ma)
{

	naf_free(nafconsole__module, ma->ma_cmd);
	naf_free(nafconsole__module, ma->ma_def);
	naf_free(nafconsole__module, ma);

	return;
}

static int macro_add(const char *cmd, const char *def)
{
	struct macro *ma;

	ma = macro_find(cmd);
	if (ma)
		return -1;

	if (!(ma = naf_malloc(nafconsole__module, sizeof(struct macro))))
		return -1;
	ma->ma_cmd = naf_strdup(nafconsole__module, cmd);
	ma->ma_def = naf_strdup(nafconsole__module, def);
	if (!ma->ma_cmd || !ma->ma_def) {
		macro__free(ma);
		return -1;
	}

	ma->ma__next = nafconsole__macrolist;
	nafconsole__macrolist = ma;

	return 0;
}

static void macro_adddefaults(void)
{
	macro_add("help", "call rpc->help");
	return;
}

static void macro_remall(void)
{
	struct macro *ma;

	for (ma = nafconsole__macrolist; ma; ) {
		struct macro *mat;

		mat = ma->ma__next;
		macro__free(ma);
		ma = mat;
	}
	nafconsole__macrolist = NULL;

	return;
}


static cmd_t *cmdfind(char *name)
{
	int i;

	for (i = 0; cmdlist[i].name; i++) {
		if (strcmp(name, cmdlist[i].name) == 0)
			return (&cmdlist[i]);
	}

	return ((cmd_t *)NULL);
}

static int cmdexec(char *line, int depth)
{
	int i;
	struct macro *ma;
	cmd_t *command;
	char *word;

	if (!line || (depth > 1))
		return -1;

	/* Isolate the command word. */
	i = 0;
	while (line[i] && ISWHITE(line[i]))
		i++;
	word = line + i;
	while (line[i] && !ISWHITE(line[i]))
		i++;
	if (line[i])
		line[i++] = '\0';

	if (depth == 0) {
		ma = macro_find(word);
		if (ma) {
			char *newcmd;
			int ret;

			/*
			 * readline is a good example of how not to do strings.
			 * I hate readline.  I think it was written by C++
			 * programmers to make C look bad.
			 */
			newcmd = naf_strdup(nafconsole__module, ma->ma_def);
			ret = cmdexec(newcmd, depth + 1);
			naf_free(nafconsole__module, newcmd);
			return ret;
		}
	}

	command = cmdfind(word);
	if (!command) {
		printf("%s: invalid command\n", word);
		return (-1);
	}

	/* Get argument to command, if any. */
	while (ISWHITE(line[i]))
		i++;

	word = line + i;

	/* Call the function. */
	return command->func(word);
}

static char *cmdgenerator(const char *text, int state)
{
	static int list_index, len, done;
	static struct macro *mac;

	if (!state) {
		done = 0;
		list_index = 0;
		len = strlen(text);
		mac = nafconsole__macrolist;
	}

	if (done)
		return NULL;

	/*
	 * Obviously we assume that readline calls this function repeatedly
	 * while doing nothing else -- ie, the macro list isn't changing.
	 */
	while (mac) {
		char *ret = NULL;

		if (strcmp(mac->ma_cmd, text) == 0) {
			/*
			 * If what they typed matches exactly, then expand the
			 * macro in-place.
			 */
			done = 1;
			ret = strdup(mac->ma_def); /* freed by rl */

		} else if (strncmp(mac->ma_cmd, text, len) == 0) {
			/*
			 * Partial match; provide possible completion.
			 */
			ret = strdup(mac->ma_cmd); /* freed by rl */
		}

		mac = mac->ma__next;
		if (ret)
			return ret;
	}
#if 0
	while ((name = cmdlist[list_index].name)) {
		list_index++;
		if (strncmp(name, text, len) == 0)
			return strdup(name); /* freed by rl */
	}
#endif
	return NULL; /* no matches */
}

static char **cmdcomplete(char *text, int start, int end)
{

	if (start != 0)
		return NULL;

	return rl_completion_matches(text, cmdgenerator);
}

static char *stripwhite(char *string)
{
	register char *s, *t;

	for (s = string; ISWHITE(*s); s++)
		;

	if (*s == 0)
		return (s);

	t = s + strlen (s) - 1;
	while (t > s && ISWHITE(*t))
		t--;
	*++t = '\0';

	return s;
}

static void fullline(void)
{
	char *stripped;

	stripped = stripwhite(rl_line_buffer);
	if (*stripped) {
		add_history(stripped);
		cmdexec(stripped, 0);
	}

	return;
}


static int modinit(struct nafmodule *mod)
{
	struct nafconn *in;

	nafconsole__module = mod;
	nafconsole__macrolist = NULL;

	if (naf_curappinfo.nai_name) {
		int plen;

		plen = strlen(naf_curappinfo.nai_name) + 1 + 1;
		if (!(nafconsole__prompt = naf_malloc(mod, plen)))
			return -1;
		snprintf(nafconsole__prompt, plen, "%s>", naf_curappinfo.nai_name);
	} else if (!(nafconsole__prompt = naf_strdup(mod, "naf>")))
		return -1;

	/*
	 * Yay for "everything's a file".
	 */
	if (!(in = naf_conn_addconn(mod, STDIN_FILENO, NAF_CONN_TYPE_RAW|NAF_CONN_TYPE_SERVER|NAF_CONN_TYPE_READRAW))) {
		dprintf(mod, "unable to create nafconn for stdin\n");
		return -1;
	}

	/*
	 * It seems that, on Linux at least, when you set stdin
	 * non-blocking, as naf_conn_addconn() does as a side-effect,
	 * that stdout also becomes non-blocking.  Which just seems
	 * broken to me, but whether it's right or not, the effect is
	 * that sometimes stdout gets corrupt, like while trying to
	 * print a long argument list.
	 *
	 * Manually set stdout blocking again.
	 */
	fcntl(STDOUT_FILENO, F_SETFL, 0);

	if (!nafconsole__macrolist)
		macro_adddefaults();

	rl_initialize();
	rl_attempted_completion_function = (CPPFunction *)cmdcomplete;

	rl_callback_handler_install(nafconsole__prompt, &fullline);
#if 0
	rl_clear_signals();
#endif

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	rl_callback_handler_remove();

	macro_remall();
	nafconsole__module = NULL;

	return 0;
}

static int connready(struct nafmodule *mod, struct nafconn *conn, naf_u16_t what)
{

	if (what & NAF_CONN_READY_CONNECTED)
		return -1;

	if (what & NAF_CONN_READY_DETECTTO)
		return -1;

	if (what & NAF_CONN_READY_WRITE)
		return -1;

	if (what & NAF_CONN_READY_READ) {
		rl_callback_read_char();
		return 1;
	}

	return -1;
}

static const char *macro_syncconf__getexpansion(const char *cmd)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "macro[%s][expansion]", cmd);
	return naf_config_getmodparmstr(nafconsole__module, buf);
}

static void macro_syncconf__addnew(const char *inlist)
{
	char *list, *c;

	if (!(list = naf_strdup(nafconsole__module, inlist)))
		return;
	c = strtok(list, ",");
	do {
		if (!c)
			break;

		if (!macro_find(c)) {
			const char *def;

			def = macro_syncconf__getexpansion(c);
			if (!def) {
				dvprintf(nafconsole__module, "configuration error: macro '%s' enabled but not defined\n", c);
				continue;
			}

			macro_add(c, def);
		}
	} while ((c = strtok(NULL, ",")));

	naf_free(nafconsole__module, list);
	return;
}

static int macro_syncconf__isconfigured(const char *inlist, const char *cmd)
{
	char *list, *c;
	int ret = 0;

	if (!(list = naf_strdup(nafconsole__module, inlist)))
		return 0;
	c = strtok(list, ",");
	do {
		if (!c)
			break;
		if (strcmp(c, cmd) == 0) {
			ret = 1;
			goto out;
		}
	} while ((c = strtok(NULL, ",")));

out:
	naf_free(nafconsole__module, list);
	return ret;
}

static void macro_syncconf__updateold(const char *inlist)
{
	struct macro *mac, **map;

	for (map = &nafconsole__macrolist; (mac = *map); ) {
		const char *def = NULL;

		if (macro_syncconf__isconfigured(inlist, mac->ma_cmd))
			def = macro_syncconf__getexpansion(mac->ma_cmd);

		if (def) {
			naf_free(nafconsole__module, mac->ma_def);
			mac->ma_def = naf_strdup(nafconsole__module, def);
			map = &mac->ma__next;
		} else {
			*map = mac->ma__next;
			macro__free(mac);
		}
	}
	return;
}

static void macro_syncconf(void)
{
	char *conflist;

	conflist = naf_config_getmodparmstr(nafconsole__module, "usemacros");
	if (!conflist) {
		macro_remall();
		macro_adddefaults();
		return;
	}
	macro_syncconf__addnew(conflist);
	macro_syncconf__updateold(conflist);

	return;
}

static void signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_CONFCHANGE) {

		NAFCONFIG_UPDATEINTMODPARMDEF(mod, "debug",
					      nafconsole__debug,
					      NAFCONSOLE_DEBUG_DEFAULT);

		macro_syncconf();
	}

	return;
}

int nafmodulemain(struct nafmodule *mod)
{

	naf_module_setname(mod, "nafconsole");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->connready = connready;
	mod->signal = signalhandler;

	return 0;
}

