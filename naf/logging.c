
#include <stdio.h>
#include <stdlib.h>
#include <config.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h> /* for struct timeval */
#include <syslog.h>

#include <naf/naf.h>
#include <naf/nafmodule.h>
#include <naf/nafevents.h>
#include <naf/nafconfig.h>

#include "module.h" /* for naf_module__registerresident() only */
#include "nafconfig_internal.h" /* for CONFIG_PARM_ */

/* LOGGING PLUGINS CAN NEVER USE DPRINTF WITH A NON-NULL FIRST ARG!! */
/* So we can't use the standard ones */
#undef dprintf
#undef dvprintf
#undef dperror
#define dprintf(p, x) nafevent(NULL, NAF_EVENT_GENERICOUTPUT, "%16.16s: " x, (p)->name)
#define dvprintf(p, x, y...) nafevent(NULL, NAF_EVENT_GENERICOUTPUT, "%16.16s: " x, p->name, y)

static struct nafmodule *ourmodule = NULL;

static int initializing = 1;

#define USESYSLOG_DEFAULT 0
static int outstreamsyslog = USESYSLOG_DEFAULT;
static char *outfilename = NULL;
static FILE *outstream = NULL;

#define STREAM_GENERIC 0
#define STREAM_DEBUG   1

static int logprintf(int stream, char *prefix, char *format, ...);
static int logvprintf(int stream, char *prefix, char *format, va_list ap);
static int logging_stop(void);
static int logging_restart(struct nafmodule *mod);


static int logging_start(struct nafmodule *mod, char *filename, char **fnret, FILE **streamret)
{

	*fnret = NULL;

	if (filename && (filename[0] == '-')) {

		*streamret = stderr;
		logprintf(STREAM_GENERIC, mod->name, "started logging to stderr (1)\n");

	} else {

		if (filename && (filename[0] != '/') && naf_config_getmodparmstr(mod, "logfilepath")) {
			char buf[512];

			snprintf(buf, sizeof(buf), 
					"%s/%s",
					naf_config_getmodparmstr(mod, "logfilepath"),
					filename);

			*fnret = naf_strdup(mod, buf);

		} else if (filename) {
			*fnret = naf_strdup(mod, filename);
		} else if (!filename && !*fnret) {
			*streamret = stderr;
		} 

		if (*fnret) {
			/* XXX make append/overwrite an option */
			if (!(*streamret = fopen(*fnret, "a+"))) {
				logprintf(STREAM_GENERIC, mod->name, "Unable to open log file %s: %s\n", *fnret, strerror(errno));
				*streamret = NULL;

				if (*fnret)
					naf_free(ourmodule, *fnret);
				*fnret = NULL;

				return -1;
			}
		}

		logprintf(STREAM_GENERIC, mod->name, "started logging to %s (2)\n", *fnret ? *fnret : "stderr");
	}

	return 0;
}

static int logging_start_wrapper(struct nafmodule *mod)
{
	char *logfile;
	int newsyslog;


	if (initializing)
		return 0;

	newsyslog = naf_config_getparmbool(CONFIG_PARM_USESYSLOG);
	if (newsyslog == -1)
		newsyslog = USESYSLOG_DEFAULT;

	logfile = naf_config_getmodparmstr(mod, "systemlogfile");
	outstreamsyslog = newsyslog;

	if (!logfile && !outstreamsyslog) { /* Make sure at least one of these set. */
		if (logging_start(mod, "-", &outfilename, &outstream) == -1) { 
			fprintf(stderr, "could not initiate logging\n");
			return -1;
		}

	} else {

		if (outstreamsyslog) {

			openlog(naf_curappinfo.nai_name ? naf_curappinfo.nai_name : "naf", LOG_CONS | LOG_NDELAY | LOG_PID, LOG_USER);
			outstream = NULL;
			outfilename = NULL;

		} else {

			if (logfile && (logging_start(mod, logfile, &outfilename, &outstream) == -1)) { 
				fprintf(stderr, "could not initiate logging\n");
				return -1;
			}
		}
	}

	return 0;
}

static int didconfigchange(struct nafmodule *mod)
{
	int yes = 0;
	char *logfile;
	int newsyslog;

	newsyslog = naf_config_getparmbool(CONFIG_PARM_USESYSLOG);
	if (newsyslog == -1)
		newsyslog = USESYSLOG_DEFAULT;

	if (newsyslog != outstreamsyslog)
		yes = 1;

	logfile = naf_config_getmodparmstr(mod, "systemlogfile");

	if ((logfile && !outfilename) || (outfilename && !logfile))
		yes = 1;
	else if (logfile && outfilename && (strcmp(logfile, outfilename) != 0))
		yes = 1;

	return yes;
}

static int eventhandler(struct nafmodule *mod, struct nafmodule *source, naf_event_t event, va_list ap)
{
	char *prefix = NULL;

	if (source && strlen(source->name))
		prefix = source->name;

	if (event == NAF_EVENT_GENERICOUTPUT) {
		char *format;

		format = va_arg(ap, char *);

		logvprintf(STREAM_GENERIC, prefix, format, ap);

	} else if (event == NAF_EVENT_DEBUGOUTPUT) {
		char *format;

		format = va_arg(ap, char *);

		logvprintf(STREAM_DEBUG, prefix, format, ap);

	}

	va_end(ap);

	return 0;
}

static int logging_stop(void)
{

	logprintf(STREAM_GENERIC, ourmodule->name, "stopped logging to %s\n", outfilename?outfilename:"stderr");

	/* don't close stderr... heh. */
	if (outstream && (outstream != stderr))
		fclose(outstream);

	outstream = NULL;

	if (outfilename) {
		naf_free(ourmodule, outfilename);
		outfilename = NULL;
	}

	if (outstreamsyslog)
		closelog();

	return 0;
}

static int logging_restart(struct nafmodule *mod)
{

	logging_stop();
	logging_start_wrapper(mod);

	return 0;
}

static char *myctime(void)
{	
	static char retbuf[64];
	struct tm *lt;
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	lt = localtime((time_t *)&tv.tv_sec);
	strftime(retbuf, 64, "%a %b %e %H:%M:%S %Z %Y", lt);

	return retbuf;
}

/*
 * syslog() doesn't allow us to easily prefix a string to it and still
 * keep it in the same message. 
 */
#define OUTBUFSZ 8192
static char outbuf[OUTBUFSZ];

static int logprintf(int stream, char *prefix, char *format, ...)
{
	va_list ap;

	if ((stream == STREAM_GENERIC) && outstreamsyslog) {

		outbuf[0] = '\0';
		if (prefix)
			snprintf(outbuf, sizeof(outbuf), "%16.16s: ", prefix);
		va_start(ap, format);
		vsnprintf(outbuf + strlen(outbuf), sizeof(outbuf) - strlen(outbuf), format, ap);
		va_end(ap);

		syslog(LOG_INFO, outbuf);

	} else {
		FILE *f = NULL;

		f = outstream;
		if (!f)
			return 0;

		if (prefix)
			fprintf(f, "%s  %s: %16.16s: ", myctime(), naf_curappinfo.nai_name, prefix);
		else
			fprintf(f, "%s  %s: ", myctime(), naf_curappinfo.nai_name);
		va_start(ap, format);
		vfprintf(f, format, ap);
		va_end(ap);

		fflush(f);
	}

	return 0;
}

static int logvprintf(int stream, char *prefix, char *format, va_list ap)
{

	if ((stream == STREAM_GENERIC) && outstreamsyslog) {

		outbuf[0] = '\0';
		if (prefix)
			snprintf(outbuf, sizeof(outbuf), "%16.16s: ", prefix);
		vsnprintf(outbuf + strlen(outbuf), sizeof(outbuf) - strlen(outbuf), format, ap);

		syslog(LOG_INFO, outbuf);

	} else {
		FILE *f = NULL;

		f = outstream;
		if (!f)
			return 0;

		if (prefix)
			fprintf(f, "%s  %s: %16.16s: ", myctime(), naf_curappinfo.nai_name, prefix);
		else
			fprintf(f, "%s  %s: ", myctime(), naf_curappinfo.nai_name);
		vfprintf(f, format, ap);

		fflush(f);
	}

	return 0;
}


static void signalhandler(struct nafmodule *mod, struct nafmodule *source, int signum)
{

	if (signum == NAF_SIGNAL_INFO) {

		dprintf(mod, "Logging module info:\n");
		dvprintf(mod, "  Output file: %s\n", outfilename ? outfilename : "stderr");

	} else if (signum == NAF_SIGNAL_RELOAD) {

		dprintf(mod, "reopening logfile...\n");
		logging_restart(mod);

	} else if (signum == NAF_SIGNAL_SHUTDOWN) {

		dprintf(mod, "deferring shutdown of logging module...\n");

	} else if (signum == NAF_SIGNAL_CONFCHANGE) {

		initializing = 0;

		if (didconfigchange(mod))
			logging_restart(mod);
	}

	return;
}

static int modinit(struct nafmodule *mod)
{

	ourmodule = mod;

	return logging_start_wrapper(mod);
}

static int modshutdown(struct nafmodule *mod)
{

	logging_stop();

	ourmodule = NULL;

	return 0;
}

static int modfirst(struct nafmodule *mod)
{

	naf_module_setname(mod, "logging");
	mod->init = modinit;
	mod->shutdown = modshutdown;
	mod->event = eventhandler;
	mod->signal = signalhandler;

	return 0;
}

int naf_logging__register(void)
{
	return naf_module__registerresident("logging", modfirst, NAF_MODULE_PRI_FIRSTPASS);
}


