
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
	$(GNR_LDADD) \
	$(NAF_LDADD)

timpsd_DEPENDENCIES = \
	../libnbio/src/libnbio.a \
	../libmx/src/libmx.a \
	../naf/libnaf.a \
	../gnr/libgnr.a \
	oscar/libtimpsoscar.a

