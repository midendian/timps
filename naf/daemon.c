
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>

#include <pwd.h> /* getpwnam() */
#include <grp.h> /* getgrnam() */

#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <naf/nafevents.h>

#include "conn.h" /* for naf_poll() */
#include "nafconfig_internal.h" /* for CONFIG_PARM_ constants */ 

#include "module.h"
#include "processes.h"

#define __PLUGINREGONLY
#include "core.h"
#include "logging.h"
#include "cache.h"
#include "stats.h"
#include "httpd.h"
#include "rpc.h"
#undef __PLUGINREGONLY

#define NAF_NOFILE_RLIMIT_DEFAULT 65536
#define NAF_CORE_RLIMIT_DEFAULT 1000000

static struct {
	char *cai_name;
	char *cai_version;
	char *cai_description;
	char *cai_copyright;
} naf_curappinfo = {NULL, NULL, NULL, NULL};

static void sighandler(int signum)
{

	if (signum == NAF_SIGNAL_SHUTDOWN) {

		nafevent(NULL, NAF_EVENT_GENERICOUTPUT, "recieved SIGINT, shutting down...\n");
		nafsignal(NULL, NAF_SIGNAL_SHUTDOWN);
		naf_module__unloadall();

		exit(2);

	} else if (signum == NAF_SIGNAL_RELOAD) {

		nafevent(NULL, NAF_EVENT_GENERICOUTPUT, "received SIGHUP\n");
		nafsignal(NULL, NAF_SIGNAL_RELOAD);

	} else if (signum == NAF_SIGNAL_INFO) {

		nafsignal(NULL, NAF_SIGNAL_INFO);

	} else if (signum == NAF_SIGNAL_CONFCHANGE) {

	} else if (signum == SIGCHLD) {

		naf_childproc__sigchild_handler();

	}

	signal(signum, sighandler);

	return;
}

static void dosetupenv(void)
{

	chdir("/");
	umask(0);

	return;
}

static int dodaemonize(void)
{
	pid_t newpid;

	if ((newpid = fork()) == -1)
		return -1;
	else if (newpid != 0)
		exit(0); /* parent dies */

	setsid();

	return 0;
}

static void dropuserperms(const char *newuser)
{
	struct passwd *pw;

	pw = getpwnam(newuser);
	if (!pw) {
		fprintf(stderr, "naf: unable to drop permissions to user %s: %s\n", newuser, strerror(errno));
		return;
	}

	if (setuid(pw->pw_uid) == -1) {
		fprintf(stderr, "naf: unable to drop permissions to user %s: %s\n", newuser, strerror(errno));
		return;
	}

	return;
}

static void dropgroupperms(const char *newgroup)
{
	struct group *gr;

	gr = getgrnam(newgroup);
	if (!gr) {
		fprintf(stderr, "naf: unable to drop permissions to group %s: %s\n", newgroup, strerror(errno));
		return;
	}

	if (setgid(gr->gr_gid) == -1) {
		fprintf(stderr, "naf: unable to drop permissions to group %s: %s\n", newgroup, strerror(errno));
		return;
	}

	return;
}

static int setnofilelimit(void)
{
	struct rlimit rl;

	memset(&rl, 0, sizeof(struct rlimit));

	rl.rlim_cur = rl.rlim_max = NAF_NOFILE_RLIMIT_DEFAULT;

	return setrlimit(RLIMIT_NOFILE, &rl);
}

static int setcorelimit(void)
{
	struct rlimit rl;

	memset(&rl, 0, sizeof(struct rlimit));

	rl.rlim_cur = rl.rlim_max = NAF_CORE_RLIMIT_DEFAULT;

	return setrlimit(RLIMIT_CORE, &rl);
}

static unsigned long getnofilelimit(void)
{
	unsigned long val = 0;
	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
		val = rl.rlim_cur;

	return val;
}

static unsigned long getcorelimit(void)
{
	unsigned long val = 0;
	struct rlimit rl;

	if (getrlimit(RLIMIT_CORE, &rl) == 0)
		val = rl.rlim_cur;

	return val;
}

/* Used for backing out of the init process */
static int naf_uninit(void)
{

	nafsignal(NULL, NAF_SIGNAL_SHUTDOWN);
	naf_module__unloadall();

	return 0;
}

/*
 * Register core NAF modules, do preliminary setup.
 */
int naf_init0(const char *appname, const char *appver, const char *appdesc, const char *appcopyright)
{

	if (appname && !(naf_curappinfo.cai_name = strdup(appname)))
		return -1;
	if (appver && !(naf_curappinfo.cai_version = strdup(appver)))
		return -1;
	if (appdesc && !(naf_curappinfo.cai_description = strdup(appdesc)))
		return -1;
	if (appcopyright && !(naf_curappinfo.cai_copyright = strdup(appcopyright)))
		return -1;

	/*
	 * This can happen during SIGHUP's reopening of the log files.
	 *
	 * Why was EPIPE turned into a signal? Or did SIGPIPE come first?
	 * The idea of SIGPIPE seems a bit absurd to me.  I/O operations
	 * should not result in a signal.  They should return an error
	 * value.  I/O operations don't even cause SIGSEGV, as they return
	 * EFAULT instead.  So why is writing to a closed descriptor 
	 * enough to warrant a signal?  A signal which is not even optional?
	 *
	 * Pontification on the subject from a few LJ people:
	 *   http://www.livejournal.com/community/unixhistory/4279.html
	 */
	signal(SIGPIPE, SIG_IGN);

	/* register resident modules */

	/* These are kind of special */
	naf_core__register();
	naf_logging__register();
	naf_config__register();
	naf_conn__register();

	/* Fairly independent utility stuff */
	naf_cache__register();
	naf_stats__register();
	naf_httpd__register();
	naf_rpc__register();

	return 0;
}

/*
 * Parse command line arguments, initialize base environment, load modules.
 *
 * XXX lots of missing error handling
 */
int naf_init1(int argc, char **argv)
{
	static const char yes[] = {"yes"};
	static const char no[] = {"no"};
	int n;
	int daemonize = 1;
	int setupenv = 1;
	int usesyslog = 0;
	char *conffn = NULL;
	const char *droptouser = NULL;
	const char *droptogroup = NULL;


	/* 
	 * Parameters read during getopt() are passed to modules via
	 * the naf_config_getparm/setparm interface.
	 *
	 * XXX Currently, parameters read from argv are overwritten
	 * by the values read from the config file.  That's counterintuitive.
	 *
	 */
	while ((n = getopt(argc, argv, "c:C:dhm:M:Su:g:D")) != EOF) {
		switch (n) {
		case 'd': daemonize = 0; break;
		case 'D': setupenv = 0; break;
		case 'C': {
			char *var, *val;

			if (!index(optarg, '='))
				goto usage;

			var = optarg;
			val = index(optarg, '=');
			*(val) = '\0';
			val++;

			naf_config_setparm(var, val);

			break;
		}
		case 'm': naf_module__add_last(optarg); /* XXX error code */ break;
		case 'M': naf_module__add(optarg); /* XXX error code */ break;
		case 'c': conffn = optarg; break;
		case 'S': usesyslog = 1; break;
		case 'g': droptogroup = optarg; break;
		case 'u': droptouser = optarg; break;
		usage:
		case 'h':
		default:
			printf("%s %s -- %s\n", naf_curappinfo.cai_name, naf_curappinfo.cai_version, naf_curappinfo.cai_description);
			printf("  %s\n", naf_curappinfo.cai_copyright);
			printf("\nUsage:\n");
			printf("\t%s [-h] [-d] [-C var=value] [-m module.so] [-M module.so] [-c file.conf] [-S] [-u username] [-u groupname]\n", argv[0]);
			printf("\n");
			if (n != 'h')
				fprintf(stderr, "invalid argument %c\n", n);
			naf_uninit();
			return -1;
		}
	}

	if (setnofilelimit() == -1)
		dprintf(NULL, "WARNING: unable to increase file descriptor ulimit\n");
	setcorelimit();

	if (setupenv)
		dosetupenv(); /* clear umask, etc */

	/* -- this has to be done after setuid() for log file permissions -- */
	/* initialize the first pass modules */
	naf_module__loadall(NAF_MODULE_PRI_FIRSTPASS); /* XXX errors */


	/*
	 * These parameters are special in that they're set explicitly and is
	 * not read from the config file.
	 */
	naf_config_setparm(CONFIG_PARM_FILENAME, conffn);
	naf_config_setparm(CONFIG_PARM_USESYSLOG, usesyslog ? yes : no);
	naf_config_setparm(CONFIG_PARM_DAEMONIZE, daemonize ? yes : no);
	if (droptouser)
		naf_config_setparm(CONFIG_PARM_DROPTOUSER, droptouser);
	if (droptogroup)
		naf_config_setparm(CONFIG_PARM_DROPTOGROUP, droptogroup);

	if (droptogroup)
		dropgroupperms(droptogroup);
	if (droptouser)
		dropuserperms(droptouser);

	if (!conffn) {
		dprintf(NULL, "no config file specified (use -c)\n");
		fprintf(stderr, "no config file specified (use -c)\n");
		naf_uninit();
		return -1;
	}

	/* initialize the rest of the modules */
	naf_module__loadall(NAF_MODULE_PRI_SECONDPASS); /* XXX errors */

	/* To jumpstart logging. */
	nafsignal(NULL, NAF_SIGNAL_CONFCHANGE);

	/* everything else */
	naf_module__loadall(NAF_MODULE_PRI_LASTPASS);

	/* install these earlier? */
	signal(NAF_SIGNAL_SHUTDOWN, sighandler);
	signal(NAF_SIGNAL_RELOAD, sighandler);
	signal(NAF_SIGNAL_INFO, sighandler);
	signal(NAF_SIGNAL_CONFCHANGE, sighandler);
	signal(SIGCHLD, sighandler);

	if (daemonize && (dodaemonize() == -1)) {
		dprintf(NULL, "unable to daemonize");
		naf_uninit();
		return -1;
	}

	return 0;
}

/*
 * Bootstrap config-dependent modules, check environment.
 */
int naf_init_final(void)
{

	/* make sure everything is sane. */
	nafsignal(NULL, NAF_SIGNAL_CONFCHANGE);

	dvprintf(NULL, "started (pid %d)\n", getpid());
	dvprintf(NULL, "maximum number of open file descriptors: %d\n", getnofilelimit());
	dvprintf(NULL, "maximum core file size: %d bytes\n", getcorelimit());

	return 0;
}

/*
 * Main loop.
 */
int naf_main(void)
{
	time_t lasttimerrun = 0;

	for (;;) {

		if ((time(NULL) - lasttimerrun) >= NAF_TIMER_ACCURACY) {
			naf_module__timerrun();
			lasttimerrun = time(NULL);
		}

		if (naf_conn__poll(NAF_TIMER_ACCURACY*1000) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
	}

	/* should be unreachable */
	dvprintf(NULL, "died for some reason (possibly because of %s)\n", strerror(errno));

	naf_uninit();

	return 0;
}

