AC_INIT(naf/daemon.c)

AM_CONFIG_HEADER(include/config.h)

AM_INIT_AUTOMAKE(naf, 0.10)

AM_MAINTAINER_MODE

AC_PROG_CC
AC_ISC_POSIX
AC_HEADER_STDC
AC_ARG_PROGRAM
AM_PROG_LIBTOOL

FAIM_LIBS=
# this are only relevent for linking the core, not the plugins
NAF_LDADD="\$(top_srcdir)/libmx/src/libmx.la \$(top_srcdir)/libnbio/src/libnbio.a -lexpat"

# gcc3.x changed to C99, which has this lovely feature-bug that breaks NAF tags.
CFLAGS="-fno-strict-aliasing"

# XXX this is not at all up to date.
AC_CHECK_HEADERS(unistd.h sys/types.h sys/socket.h netinet/in.h netdb.h sys/time.h)

dnl AC_CHECK_LIB(faim, aim_session_init, 
dnl             FAIM_LIBS="-lfaim",
dnl             AC_MSG_ERROR([libfaim is required])
dnl)

FAIM_INCLUDES=
dnl XXX fix this (add --with-libfaim= option)
dnl FAIM_INCLUDES="-I/usr/include/libfaim -I/usr/local/include/libfaim"

AC_CHECK_LIB(dl, dlopen, NAF_LDADD="-ldl $NAF_LDADD")
AC_SUBST(NAF_LDADD)

NAF_INCLUDES="-I\$(top_srcdir)/libmx/include -I\$(top_srcdir)/libnbio/include -I\$(top_srcdir)/include "
AC_SUBST(NAF_INCLUDES)

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(FAIM_LIBS)
AC_SUBST(FAIM_INCLUDES)

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
	gnr/Makefile
])

