/*
 * This allows you to interactively make in-process NAF RPC
 * calls.  It's great for debugging.
 *
 */

#include <naf/nafmodule.h>
#include <naf/nafrpc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

static struct nafmodule *ourmodule = NULL;

static const char prompt[] = "naf>";

typedef struct {
	char *name;
	Function *func;
	char *doc;
} cmd_t;

static int cmd_help(char *arg);
static int cmd_call(char *arg);

static cmd_t cmdlist[] = {
	{ "help", cmd_help, "Help"},
	{ "call", cmd_call, "Invoke a NAF RPC method"},
	{ (char *)NULL, (Function *)NULL, (char *)NULL }
};

static int cmd_help(char *arg)
{
	int i;

	for (i = 0; cmdlist[i].name; i++)
		printf("\t%s\t%s\n", cmdlist[i].name, cmdlist[i].doc);

	return 0;
}

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
	while ((*c == ' ') || (*c == '\t'))
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

		if (naf_rpc_addarg_scalar(ourmodule, &req->inargs, name, (naf_rpcu32_t)atoi(val)) == -1) {
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

		if (naf_rpc_addarg_bool(ourmodule, &req->inargs, name, (naf_rpcu8_t)on) == -1) {
			printf("addarg_bool failed\n");
			return -1;
		}

	} else if (type == NAF_RPC_ARGTYPE_STRING) {

		if (naf_rpc_addarg_string(ourmodule, &req->inargs, name, val) == -1) {
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

	if (!(req = naf_rpc_request_new(ourmodule, target, method))) {
		printf("naf_rpc_request_new failed\n");
		return 0;
	}

	if (args && (splitandparseargs(req, args) == -1)) {
		printf("argument parsing failed\n");
		naf_rpc_request_free(ourmodule, req);
		return 0;
	}

	printf("input arguments:\n");
	dumpargs(req->inargs, 1);

	if (naf_rpc_request_issue(ourmodule, req) == -1)  {
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
	naf_rpc_request_free(ourmodule, req);

	return 0;
}


static cmd_t *cmdfind(char *name)
{
	int i;

	for (i = 0; cmdlist[i].name; i++)
		if (strcmp (name, cmdlist[i].name) == 0)
			return (&cmdlist[i]);

	return ((cmd_t *)NULL);
}

static int cmdexec(char *line)
{
	register int i;
	cmd_t *command;
	char *word;

	/* Isolate the command word. */
	i = 0;
	while (line[i] && whitespace (line[i]))
		i++;
	word = line + i;

	while (line[i] && !whitespace (line[i]))
		i++;

	if (line[i])
		line[i++] = '\0';

	command = cmdfind(word);

	if (!command) {
		printf("%s: invalid command\n", word);
		return (-1);
	}

	/* Get argument to command, if any. */
	while (whitespace (line[i]))
		i++;

	word = line + i;

	/* Call the function. */
	return ((*(command->func)) (word));
}

static char *cmdgenerator(char *text, int state)
{
	static int list_index, len;
	char *name;

	if (!state) {
		list_index = 0;
		len = strlen (text);
	}

	while ((name = cmdlist[list_index].name)) {
		list_index++;
		if (strncmp (name, text, len) == 0)
			return (strdup(name));
	}

	/* If no names matched, then return NULL. */
	return ((char *)NULL);
}

static char **cmdcomplete(char *text, int start, int end)
{

	if (start != 0)
		return NULL;

	return completion_matches(text, cmdgenerator);
}

static char *stripwhite(char *string)
{
	register char *s, *t;

	for (s = string; whitespace (*s); s++)
		;

	if (*s == 0)
		return (s);

	t = s + strlen (s) - 1;
	while (t > s && whitespace (*t))
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
		cmdexec(stripped);
	}

	return;
}


static int modinit(struct nafmodule *mod)
{
	struct nafconn *in;

	ourmodule = mod;


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

	rl_attempted_completion_function = (CPPFunction *)cmdcomplete;

	rl_callback_handler_install(prompt, &fullline);
	rl_clear_signals();

	return 0;
}

static int modshutdown(struct nafmodule *mod)
{

	rl_callback_handler_remove();

	ourmodule = NULL;

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

int nafmodulemain(struct nafmodule *mod)
{

	naf_module_setname(mod, "nafconsole");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->connready = connready;

	return 0;
}
