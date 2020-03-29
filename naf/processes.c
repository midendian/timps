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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* for exit() */
#endif

#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <naf/naftag.h>

/* only children in the RUNNING state are listed */
static naf_childproc_t *naf__childrenlist = NULL;


static int getredirsock(struct nafmodule *mod, struct nafconn **connret, int *endpointfdret)
{
	int sv[2];

	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, sv) == -1)
		return -1;

	if (!(*connret = naf_conn_addconn(mod, sv[1], NAF_CONN_TYPE_LOCAL | NAF_CONN_TYPE_CLIENT))) {
		close(sv[0]);
		close(sv[1]);
		return -1;
	}
	*endpointfdret = sv[0];

	return 0;
}

static void naf_childproc_free_real(naf_childproc_t *cp)
{

	/* XXX no way to explicitly free the conn_t... hope this works... */
	/* Only do this if the child hasn't been launched yet... */
	if (cp->status == NAF_CHILDPROC_STATUS_NOTSTARTED) {
		if (cp->stdstreams.in)
			close(cp->stdstreams.in->fdt->fd);
		if (cp->stdstreams.out)
			close(cp->stdstreams.out->fdt->fd);
		if (cp->stdstreams.err)
			close(cp->stdstreams.err->fdt->fd);
	}

	naf_tag_freelist(&cp->taglist, cp);

	naf_free(cp->owner, cp->cmdline);
	naf_free(cp->owner, cp);

	return;
}

naf_childproc_t *naf_childproc_create(struct nafmodule *mod, naf_u16_t streamflags)
{
	naf_childproc_t *cp;

	if (!(cp = naf_malloc(mod, sizeof(naf_childproc_t))))
		return NULL;
	memset(cp, 0, sizeof(naf_childproc_t));

	cp->owner = mod;
	cp->status = NAF_CHILDPROC_STATUS_NOTSTARTED;
	cp->pid = 0;
	cp->cmdline = NULL;
	cp->forktime = cp->exittime = 0;
	cp->taglist = NULL;


	cp->stdstreams.streamflags = streamflags;

	if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_REDIR_STDIN) {
		if (getredirsock(mod, &cp->stdstreams.in, &cp->stdstreams.inendpointfd) == -1)
			goto errout;
	}

	if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_REDIR_STDOUT) {
		if (getredirsock(mod, &cp->stdstreams.out, &cp->stdstreams.outendpointfd) == -1)
			goto errout;
	}

	if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_REDIR_STDERR) {
		if (getredirsock(mod, &cp->stdstreams.err, &cp->stdstreams.errendpointfd) == -1)
			goto errout;
	}


	return cp;
errout:
	naf_childproc_free_real(cp);
	return NULL;
}

static naf_childproc_t *naf_childproc__remove(pid_t pid)
{
	naf_childproc_t *cur, **prev;

	for (prev = &naf__childrenlist; (cur = *prev); ) {

		if (cur->pid == pid) {
			*prev = cur->next;
			return cur;
		}

		prev = &cur->next;
	}

	return NULL;
}

/* for when a stream dies before the process does... */
void naf_childproc_cleanconn(struct nafconn *conn)
{
	naf_childproc_t *cp;

	for (cp = naf__childrenlist; cp; cp = cp->next) {

		if (cp->stdstreams.in == conn)
			cp->stdstreams.in = NULL;
		if (cp->stdstreams.out == conn)
			cp->stdstreams.out = NULL;
		if (cp->stdstreams.err == conn)
			cp->stdstreams.err = NULL;
	}

	return;
}

void naf_childproc__sigchild_handler(void)
{
	pid_t pid;
	int status;
	naf_childproc_t *cp;

	pid = wait(&status);
	if (pid == -1)
		return;

	if (!(cp = naf_childproc__remove(pid))) {
		tvprintf(NULL, "unknown child died (pid %d)\n", pid);
		return;
	}

	cp->status = NAF_CHILDPROC_STATUS_EXITED;
	cp->exittime = time(NULL);
	if (WIFEXITED(status)) {
		cp->exitinfo.status = NAF_CHILDPROC_EXITSTATUS_EXITED;
		cp->exitinfo.value = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		cp->exitinfo.status = NAF_CHILDPROC_EXITSTATUS_SIGNALED;
		cp->exitinfo.value = WTERMSIG(status);
	}

	tvprintf(NULL, "child %d %s (%s %d) (owned by %s)\n",
			cp->pid,
			(cp->exitinfo.status == NAF_CHILDPROC_EXITSTATUS_EXITED) ? "exited" : "terminated by signal",
			(cp->exitinfo.status == NAF_CHILDPROC_EXITSTATUS_EXITED) ? "returned code" : "signal number",
			cp->exitinfo.value,
			cp->owner ? cp->owner->name : "unknown");

	/* Notify owner... */
	if (cp->owner && cp->owner->childexited)
		cp->owner->childexited(cp->owner, cp);

	naf_childproc_free_real(cp);

	return;
}

void naf_childproc_free(naf_childproc_t *cp)
{

	if (!cp)
		return;
	if (cp->status != NAF_CHILDPROC_STATUS_NOTSTARTED)
		return; /* if it's started, this is much more complicated -- done elsewhere */

	naf_childproc_free_real(cp);

	return;
}

int naf_childproc_fork(naf_childproc_t *cp, void (*childmainfunc)(naf_childproc_t *))
{
	int n;

	if (!cp || !childmainfunc)
		return -1;

	n = fork();
	if (n == -1)
		return -1;

	if (n == 0) { /* child */

		if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_REDIR_STDIN) {

			close(cp->stdstreams.in->fdt->fd); /* only for parent use */

			dup2(cp->stdstreams.inendpointfd, STDIN_FILENO);
			close(cp->stdstreams.inendpointfd);

		} else
			close(STDIN_FILENO);

		if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_REDIR_STDOUT) {

			close(cp->stdstreams.out->fdt->fd); /* only for parent use */

			dup2(cp->stdstreams.outendpointfd, STDOUT_FILENO);
			close(cp->stdstreams.outendpointfd);

		} else if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_NULL_STDOUT) {
			int errfd;

			if ((errfd = open("/dev/null", O_WRONLY)) != -1) {
				dup2(errfd, STDOUT_FILENO);
				close(errfd);
			}

		} else
			close(STDOUT_FILENO);

		if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_REDIR_STDERR) {

			close(cp->stdstreams.err->fdt->fd); /* only for parent use */

			dup2(cp->stdstreams.errendpointfd, STDERR_FILENO);
			close(cp->stdstreams.errendpointfd);

		} else if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_NULL_STDERR) {
			int errfd;

			if ((errfd = open("/dev/null", O_WRONLY)) != -1) {
				dup2(errfd, STDERR_FILENO);
				close(errfd);
			}

		} else
			close(STDERR_FILENO);

		if (!(cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_LEAVEALLOPEN)) {
			int i;

			/*
			 * XXX Find a better way to do this than making 64k
			 * system calls.
			 *
			 * Maybe set CLOSEEXEC on every file descriptor we
			 * open?  But that's a pain!
			 */
			for (i = 3; i <= 65535; i++)
				close(i);
		}

		childmainfunc(cp);

		exit(127);
		return 0; /* never reached */

	}

	/* parent */

	cp->pid = n;
	cp->forktime = time(NULL);
	cp->status = NAF_CHILDPROC_STATUS_RUNNING;

	/* Close redirected fd's in parent as necessary to prevent leakage... */
	if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_REDIR_STDIN) {
		close(cp->stdstreams.inendpointfd);
		cp->stdstreams.inendpointfd = -1;
	}
	if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_REDIR_STDOUT) {
		close(cp->stdstreams.outendpointfd);
		cp->stdstreams.outendpointfd = -1;
	}
	if (cp->stdstreams.streamflags & NAF_CHILDPROC_STREAMFLAG_REDIR_STDERR) {
		close(cp->stdstreams.errendpointfd);
		cp->stdstreams.errendpointfd = -1;
	}

	cp->next = naf__childrenlist;
	naf__childrenlist = cp;

	return 0;
}

static void naf_childproc_system_childmainfunc(naf_childproc_t *cp)
{

	execl("/bin/sh", "sh", "-c", cp->cmdline, NULL);

	exit(127); /* unreachable unless exec failed */

	return;
}

int naf_childproc_system(naf_childproc_t *cp, const char *cmdline)
{

	if (!cmdline)
		return -1;
	if (cp->status != NAF_CHILDPROC_STATUS_NOTSTARTED)
		return -1;

	if (!(cp->cmdline = naf_strdup(cp->owner, cmdline)))
		return -1;

	return naf_childproc_fork(cp, naf_childproc_system_childmainfunc);
}


int naf_childproc_tag_add(struct nafmodule *mod, naf_childproc_t *cp, const char *name, char type, void *data)
{

	if (!cp)
		return -1;

	return naf_tag_add(&cp->taglist, mod, name, type, data);
}

int naf_childproc_tag_remove(struct nafmodule *mod, naf_childproc_t *cp, const char *name, char *typeret, void **dataret)
{

	if (!cp)
		return -1;

	return naf_tag_remove(&cp->taglist, mod, name, typeret, dataret);
}

int naf_childproc_tag_ispresent(struct nafmodule *mod, naf_childproc_t *cp, const char *name)
{

	if (!cp)
		return -1;

	return naf_tag_ispresent(&cp->taglist, mod, name);
}

int naf_childproc_tag_fetch(struct nafmodule *mod, naf_childproc_t *cp, const char *name, char *typeret, void **dataret)
{

	if (!cp)
		return -1;

	return naf_tag_fetch(&cp->taglist, mod, name, typeret, dataret);
}


