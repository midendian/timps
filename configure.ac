AC_INIT(timps, 0.10)
AC_CONFIG_SRCDIR(naf/daemon.c)
AC_CONFIG_HEADERS(include/config.h)
AM_INIT_AUTOMAKE

AM_MAINTAINER_MODE

AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL

# this are only relevent for linking the core, not the plugins
NAF_LDADD="\$(top_srcdir)/naf/libnaf.a \$(top_srcdir)/libmx/src/libmx.a \$(top_srcdir)/libnbio/src/libnbio.a -lexpat"
GNR_LDADD="\$(top_srcdir)/gnr/libgnr.a"

# gcc3.x changed to C99, which has this lovely feature-bug that breaks NAF tags.
CFLAGS="-fno-strict-aliasing"

# XXX this is not at all up to date.
AC_CHECK_HEADERS(unistd.h sys/types.h sys/socket.h netinet/in.h netdb.h sys/time.h)

AC_CHECK_LIB(dl, dlopen, NAF_LDADD="-ldl $NAF_LDADD")
AC_SUBST(NAF_LDADD)
AC_SUBST(GNR_LDADD)

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
	modules/timpsotr/Makefile
	naf/Makefile
	gnr/Makefile
	timps/oscar/Makefile
	timps/Makefile
])

