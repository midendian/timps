
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
	$(EXPAT_LIBS) \
	$(NBIO_LIBS) \
	$(GNR_LDADD) \
	$(NAF_LDADD) 

timpsd_DEPENDENCIES = \
	../libmx/src/libmx.a \
	../naf/libnaf.a \
	../naf/ipv4/libnaf-ipv4.a \
	../gnr/libgnr.a \
	oscar/libtimpsoscar.a

