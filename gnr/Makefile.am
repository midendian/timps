
INCLUDES += -I. $(NAF_INCLUDES)
CFLAGS += -Wall -g

noinst_LIBRARIES = libgnr.a

libgnr_a_SOURCES = \
	core.c \
	msg.c \
	msg.h \
	node.c \
	node.h

libgnr_a_DEPENDENCIES = \
	../libnbio/src/libnbio.a \
	../libmx/src/libmx.la \
	../naf/libnaf.a

