
INCLUDES += -I. $(NAF_INCLUDES)
CFLAGS += -Wall -g

bin_PROGRAMS = naf

naf_SOURCES = \
	cache.c \
	cache.h \
	conn.c \
	conn.h \
	core.c \
	core.h \
	daemon.c \
	logging.c \
	logging.h \
	memory.c \
	memory.h \
	module.c \
	module.h \
	nafconfig.c \
	nafconfig_internal.h \
	processes.c \
	processes.h \
	rpc.c \
	rpc.h \
	sbuf.c \
	httpd.c \
	httpd.h \
	stats.c \
	stats.h \
	tags.c \
	tags.h \
	tlv.c

naf_LDFLAGS = -rdynamic 
naf_LDADD = $(NAF_LDADD)

naf_DEPENDENCIES = \
	../libnbio/src/libnbio.a \
	../libmx/src/libmx.la

