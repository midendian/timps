
#ifndef __NAFMODULE_H__
#define __NAFMODULE_H__

#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>

#include <naf/nafconn.h>
#include <naf/naftypes.h>
#include <naf/nafevents.h>

struct naf_childproc_s; /* defined later */
struct naf_conn_s; /* defined in nafconn.h, which also requires nafmodule.h */

/*
 * Module initialization order, roughly:
 *    First pass:  Logging
 *    Second pass: I/O (nafconn)
 *    Third pass: The rest of the resident modules
 *    Last pass: External modules 
 */
#define NAF_MODULE_PRI_MAX		5
#define NAF_MODULE_PRI_FIRSTPASS 	NAF_MODULE_PRI_MAX
#define NAF_MODULE_PRI_SECONDPASS 	4
#define NAF_MODULE_PRI_THIRDPASS	3
#define NAF_MODULE_PRI_LASTPASS 	0

/*
 * Each module has the ability to give the core a plain-text status message for
 * display through server management interfaces.  The module is responsible for
 * updating this data using naf_module_setstatusline(), and setting a timer to
 * make that call if needed (that is not required).
 *
 */
#define NAF_MODULE_STATUSLINE_MAXLEN  512

/*
 * The nafmodule context.
 *
 * Every external module must define a 'nafmodulemain' function which is
 * dynamically imported by the NAF framework's (nafsrc/modules.c).  
 *
 * When called, the nafmodulemain function is passed one of these structs,
 * which it must then fill in as appropriate, returning negative on failure
 * (which will cause the module to get unloaded immediatly), or zero on
 * success.
 *
 * After a successful first-stage load, the loader will then
 * call the function specified by the init member of the context
 * if it is non-NULL.  
 *
 * Any other of the event functions may be called by the core after that.
 *
 * The loader will call the shutdown member when it is asked to unload this
 * module, or if it is shutting down. 
 *
 */
struct nafmodule {
	/*
	 * Name of the module.  (Used by naflogging module as prefix
	 * for messages from this module.)
	 */
#define NAF_MODULE_NAME_MAX 128
	char name[NAF_MODULE_NAME_MAX+1];

	/* ---------- Initialization ---------- */
	/*
	 * Second-pass initialization.
	 */
	int (*init)(struct nafmodule *mod);

	/* ---------- Connection Handling ---------- */

	/*
	 * Detect if the connection should belong to this module.
	 *
	 * Return 1 for true, 0 for false, -1 on connection error.
	 *
	 */
	int (*protocoldetect)(struct nafmodule *mod, struct nafconn *conn);

	/*
	 * The module is being requested to take over control of a connection
	 * (usually called after a successful protocoldetect).
	 *
	 * Return -1 on error, anything else on success.
	 *
	 */
	int (*takeconn)(struct nafmodule *mod, struct nafconn *conn);

	/*
	 * The socket has become readable, writable, or connected.
	 *
	 * Return -1 on error, anything else for sucess.
	 *
	 */
#define NAF_CONN_READY_READ      0x0001
#define NAF_CONN_READY_WRITE     0x0002
#define NAF_CONN_READY_CONNECTED 0x0004
#define NAF_CONN_READY_DETECTTO  0x0008 /* protocol detection timeout */
	int (*connready)(struct nafmodule *mod, struct nafconn *conn, naf_u16_t what);

	/*
	 * Called when this module has registered a tag for an object that is
	 * being destroyed.  The object and tag information is given as
	 * arguments.
	 */
	void (*freetag)(struct nafmodule *mod, void *object, const char *tagname, char tagtype, void *tagdata);

	/*
	 * Connection killing function. Called by the core for each connection
	 * it kills.
	 */
	void (*connkill)(struct nafmodule *mod, struct nafconn *conn);

	/* ---------- Event handling ---------- */

	/*
	 * Event handler.
	 */
	int (*event)(struct nafmodule *mod, struct nafmodule *naf, naf_event_t event, va_list ap);

	/*
	 * NAF has received an operation system signal.
	 */
#define NAF_SIGNAL_SHUTDOWN    SIGINT
#define NAF_SIGNAL_RELOAD      SIGHUP
#define NAF_SIGNAL_INFO        SIGUSR1 /* XXX I want to use SIGINFO here */
#define NAF_SIGNAL_CONFCHANGE  SIGUSR2
	void (*signal)(struct nafmodule *mod, struct nafmodule *srcmod, int signum);

	/*
	 * How often (in seconds) to fire a signal, and the function
	 * to call to do it.
	 *
	 * Note that this timer only has NAF_TIMER_ACCURACY seconds accuracy!
	 */
#define NAF_TIMER_ACCURACY 2
	naf_u16_t timerfreq;
	void (*timer)(struct nafmodule *mod);

	/*
	 * A child that was forked by this module has died and its status is
	 * waiting to be processed (if necessary).
	 */
	void (*childexited)(struct nafmodule *mod, struct naf_childproc_s *childproc);

	/*
	 * Plugin is being unloaded.  Clean up.
	 */
	int (*shutdown)(struct nafmodule *mod);

	/*
	 * This can be used for whatever the module wants to use it for.
	 */
	void *priv;

	/*
	 * Tag list for naf_module_tag_add/fetch/remove/ispresent().
	 */
	void *taglist;

	char statusline[NAF_MODULE_STATUSLINE_MAXLEN+1];
	void *memorystats; /* used by memory allocator */
};

int naf_module_setname(struct nafmodule *mod, const char *name);
void naf_module_setstatusline(struct nafmodule *mod, const char *line);
struct nafmodule *naf_module_findbyname(struct nafmodule *caller, const char *name);

int naf_module_tag_add(struct nafmodule *mod, struct nafmodule *target, const char *name, char type, void *data);
int naf_module_tag_remove(struct nafmodule *mod, struct nafmodule *target, const char *name, char *typeret, void **dataret);
int naf_module_tag_ispresent(struct nafmodule *mod, struct nafmodule *target, const char *name);
int naf_module_tag_fetch(struct nafmodule *mod, struct nafmodule *target, const char *name, char *typeret, void **dataret);


/*
 * Child process management.
 */
typedef struct naf_childproc_s {
	struct nafmodule *owner;
#define NAF_CHILDPROC_STATUS_NOTSTARTED 0 /* not yet forked */
#define NAF_CHILDPROC_STATUS_RUNNING    1 /* ->pid valid */
#define NAF_CHILDPROC_STATUS_EXITED     2 /* ->exitinfo valid */
	int status;
	pid_t pid;
	struct {
		/* default is to close all fds after fork (including stdin/out/err) */
#define NAF_CHILDPROC_STREAMFLAG_NONE         0x0000
#define NAF_CHILDPROC_STREAMFLAG_REDIR_STDIN  0x0001
#define NAF_CHILDPROC_STREAMFLAG_REDIR_STDOUT 0x0002
#define NAF_CHILDPROC_STREAMFLAG_NULL_STDOUT  0x0004 /* send stdout to /dev/null */
#define NAF_CHILDPROC_STREAMFLAG_REDIR_STDERR 0x0008
#define NAF_CHILDPROC_STREAMFLAG_NULL_STDERR  0x0010 /* send stderr to /dev/null */
#define NAF_CHILDPROC_STREAMFLAG_LEAVEALLOPEN 0x0100 /* don't close all fds on fork */
		naf_u16_t streamflags;
		struct nafconn *in; /* only with NAF_CHILDPROC_STREAMFLAG_REDIR_STDIN */
		int inendpointfd;
		struct nafconn *out; /* only with NAF_CHILDPROC_STREAMFLAG_REDIR_STDOUT */
		int outendpointfd;
		struct nafconn *err; /* only with NAF_CHILDPROC_STREAMFLAG_REDIR_STDERR */
		int errendpointfd;
	} stdstreams;
	char *cmdline; /* optional (only set after/if naf_childproc_system called) */
	struct {
#define NAF_CHILDPROC_EXITSTATUS_EXITED   0
#define NAF_CHILDPROC_EXITSTATUS_SIGNALED 1
		int status;
		naf_u8_t value; /* code from exit() or a signum */
	} exitinfo;
	time_t forktime;
	time_t exittime;
	void *taglist;
	struct naf_childproc_s *next;
} naf_childproc_t;

naf_childproc_t *naf_childproc_create(struct nafmodule *mod, naf_u16_t streamflags);
int naf_childproc_fork(naf_childproc_t *cp, void (*childmainfunc)(naf_childproc_t *cp));
int naf_childproc_system(naf_childproc_t *cp, const char *cmdline);
void naf_childproc_free(naf_childproc_t *cp);

int naf_childproc_tag_add(struct nafmodule *mod, naf_childproc_t *cp, const char *name, char type, void *data);
int naf_childproc_tag_remove(struct nafmodule *mod, naf_childproc_t *cp, const char *name, char *typeret, void **dataret);
int naf_childproc_tag_ispresent(struct nafmodule *mod, naf_childproc_t *cp, const char *name);
int naf_childproc_tag_fetch(struct nafmodule *mod, naf_childproc_t *cp, const char *name, char *typeret, void **dataret);


/*
 * Wrappers for memory allocation.  Used for debugging and run-time
 * memory statistics.
 */
#define NAF_MEM_TYPE_GENERIC 0
#define NAF_MEM_TYPE_NETBUF  1
void *naf_malloc_real(struct nafmodule *mod, int type, size_t size, const char *file, int line);
#define naf_malloc(x, y, z) naf_malloc_real(x, y, z, __FILE__, __LINE__)
void naf_free_real(struct nafmodule *mod, void *ptr, const char *file, int line);
#define naf_free(x, y) naf_free_real(x, y, __FILE__, __LINE__)
char *naf_strdup(struct nafmodule *mod, int type, const char *s);

#if 0
#define malloc(x) laksjdfj
#define free(x) alksdjfj
#define strdup(x) akljdsfkl
#endif

/* Pool of fixed-length buffers */
typedef struct naf_flmempool_s {
	naf_u32_t flmp_blklen; /* bytes per block */
	naf_u32_t flmp_blkcount; /* total blocks in pool */
	naf_u8_t *flmp_region; /* first address in buffer */
	naf_u32_t flmp_regionlen; /* size of buffer at flmp_region */
	naf_u8_t *flmp_allocmap; /* bitmap of allocated blocks */
	naf_u32_t flmp_allocmaplen;
} naf_flmempool_t;

naf_flmempool_t *naf_flmp_alloc(struct nafmodule *owner, int memtype, int blklen, int blkcount);
void naf_flmp_free(struct nafmodule *owner, naf_flmempool_t *flmp);
void *naf_flmp_blkalloc(struct nafmodule*owner, naf_flmempool_t *flmp);
void naf_flmp_blkfree(struct nafmodule *owner, naf_flmempool_t *flmp, void *block);

#endif /* __NAFMODULE_H__ */

