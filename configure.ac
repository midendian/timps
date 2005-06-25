AC_INIT(timps, 0.10)
AC_CONFIG_SRCDIR(naf/daemon.c)
AC_CONFIG_HEADERS(include/config.h)
AM_INIT_AUTOMAKE

AM_MAINTAINER_MODE

AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL

# this are only relevent for linking the core, not the plugins
NAF_LDADD="\$(top_srcdir)/naf/libnaf.a \$(top_srcdir)/naf/ipv4/libnaf-ipv4.a \$(top_srcdir)/libmx/src/libmx.a \$(top_srcdir)/libnbio/src/libnbio.a"
GNR_LDADD="\$(top_srcdir)/gnr/libgnr.a"

# gcc3.x changed to C99, which has this lovely feature-bug that breaks NAF tags.
CFLAGS="-fno-strict-aliasing"

# XXX this should use AC_DEFINE, not CFLAGS
EXPAT_LIBS=
EXPAT_CFLAGS=
AC_CHECK_LIB(expat, XML_Parse,
	EXPAT_LIBS="-lexpat",
	EXPAT_CFLAGS="-DNOXML")

# this is all for the nafconsole module
NAF_READLINELIBS=
AC_CHECK_HEADERS(readline/readline.h readline/history.h)
AC_CHECK_LIB(readline, rl_callback_handler_install,
	AC_CHECK_LIB(history, add_history,
		[rlhist="both"],
		[rlhist="readline"]),
	[rlhist="none"])
if test "$rlhist" = "both"; then
	NAF_READLINELIBS="-lcurses -lreadline -lhistory"
	AC_DEFINE(NAF_USEREADLINE, 1, [Define if readline is available])
	AC_DEFINE(NAF_USELIBHISTORY, 1, [Define if libhistory is available])
fi
if test "$rlhist" = "readline"; then
	NAF_READLINELIBS="-lcurses -lreadline"
	AC_DEFINE(NAF_USEREADLINE, 1, [Define if readline is available])
fi
AC_SUBST(NAF_READLINELIBS)

AC_FUNC_MMAP

AC_CHECK_HEADERS(unistd.h sys/types.h sys/socket.h netinet/in.h netdb.h sys/time.h ctype.h stdlib.h sys/stat.h string.h sys/resource.h pwd.h grp.h stdio.h errno.h syslog.h time.h sys/poll.h stdarg.h arpa/inet.h signal.h sys/mman.h sys/wait.h fcntl.h sys/ioctl.h)

# used for getting the real destination address on linux
AC_CHECK_HEADERS(linux/netfilter_ipv4.h) 

AC_CHECK_HEADERS(netinet/ip.h netinet/in.h, [enable_ipv4="yes"], [enable_ipv4="no"])
if test "$enable_ipv4" = "yes"; then
	AC_DEFINE(NAF_USEIPV4, 1, [Define if IPv4 enabled.])
	AC_CHECK_HEADERS(linux/netdevice.h linux/if_tun.h, [enable_ipv4_linuxtun="yes"], [enable_ipv4_linuxtun="no"])
	if test "$enable_ipv4_linuxtun" = "yes"; then
		AC_DEFINE(NAF_USEIPV4_LINUXTUN, 1, [Define if linuxtun enabled.])
	fi
fi

AC_DEFINE(NAF_MEM_ENABLED, 1, [Define to use NAF memory debugging])

AC_CHECK_LIB(dl, dlopen, NAF_LDADD="-ldl $NAF_LDADD")
AC_SUBST(NAF_LDADD)
AC_SUBST(GNR_LDADD)

AC_SUBST(EXPAT_LIBS)
AC_SUBST(EXPAT_CFLAGS)

AC_SUBST(READLINE_LIBS)
AC_SUBST(READLINE_CFLAGS)

NAF_INCLUDES="-I\$(top_srcdir)/libmx/include -I\$(top_srcdir)/libnbio/include -I\$(top_srcdir)/include "
AC_SUBST(NAF_INCLUDES)

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

dnl LIBTOOL="$LIBTOOL --silent"

AC_OUTPUT([
	Makefile
	include/Makefile
	include/naf/Makefile
	include/gnr/Makefile
	libnbio/Makefile
	libnbio/include/Makefile
	libnbio/src/Makefile
	libmx/Makefile
	libmx/include/Makefile
	libmx/src/Makefile
	modules/Makefile
	modules/nafbasicmodule/Makefile
	modules/nafconsole/Makefile
	naf/Makefile
	naf/ipv4/Makefile
	gnr/Makefile
	timps/oscar/Makefile
	timps/Makefile
])

