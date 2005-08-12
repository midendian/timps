
SUBDIRS = oscar

INCLUDES = -I. $(NAF_INCLUDES)
CFLAGS += -Wall -g

bin_PROGRAMS = timpsd

timpsd_SOURCES = \
	logging.c \
	logging.h \
	timps.c

timpsd_LDFLAGS = -rdynamic
timpsd_LDADD = \
	oscar/libtimpsoscar.a \
	../gnr/libgnr.a \
	../libmx/src/libmx.a \
	../naf/libnaf.a \
	../naf/ipv4/libnaf-ipv4.a \
	$(EXPAT_LIBS) \
	$(NBIO_LIBS)
