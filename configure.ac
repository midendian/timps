AC_INIT(naf/daemon.c)
AM_CONFIG_HEADER(include/config.h)
AM_INIT_AUTOMAKE(timps, 0.25)

AM_MAINTAINER_MODE

AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL

case "$ac_cv_host" in
	*-*-darwin*)
		[macosx="yes"]
		;;
esac

dnl gcc3.x changed to C99, which has this lovely feature-bug that breaks NAF tags.
CFLAGS="-fno-strict-aliasing"

AC_CHECK_LIB(nbio, nbio_init,
	NBIO_LIBS="-lnbio",
	AC_MSG_ERROR(unable to find libnbio)
)

dnl XXX this should use AC_DEFINE, not CFLAGS
EXPAT_CFLAGS=
AC_CHECK_LIB(expat, XML_Parse,
	[EXPAT_LIBS="-lexpat"
	 EXPAT_CFLAGS="-DHAVE_EXPAT_H"],
	EXPAT_CFLAGS="-DNOXML")

WBXML_LIBS=
AC_CHECK_LIB(wbxml2, wbxml_conv_xml2wbxml,
	[WBXML_LIBS="-lwbxml2"
	 AC_DEFINE(HAVE_WBXML2, 1, [Define if libwbxml2 is available])],
	[])
AC_SUBST(WBXML_LIBS)

dnl this is all for the nafconsole module
NAF_READLINELIBS=
AC_CHECK_HEADERS(readline/readline.h readline/history.h)
dnl GNU readline has rl_completion_matches, BSD readline has completion_matches
AC_CHECK_LIB(readline, rl_completion_matches,
	[rlhist="readline"],
	AC_CHECK_LIB(readline, completion_matches,
		[rlhist="readline"
		 AC_DEFINE(NAF_OLDREADLINE, 1, [Define if readline is BSD-style])],
		[rlhist="none"]))
if test "$rlhist" = "readline"; then
	AC_CHECK_LIB(history, add_history,
		[rlhist="both"],
		[rlhist="readline"])
fi
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

dnl used for getting the real destination address on linux
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

AC_CHECK_LIB(dl, dlopen, LIBS="-ldl $LIBS")

AC_SUBST(NBIO_LIBS)
AC_SUBST(EXPAT_LIBS)
AC_SUBST(EXPAT_CFLAGS)

AC_SUBST(READLINE_LIBS)
AC_SUBST(READLINE_CFLAGS)

dnl XXX this libnbio thing is lame
NAF_INCLUDES="-I\$(top_srcdir)/libmx/include -I\$(top_srcdir)/include -I/usr/local/include/libnbio -I/usr/include/libnbio"
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

