#! smake
# $Revision: 1.8 $
# $Date: 91/05/23 14:54:57 $
#
# FlexFAX Facsimile System
#
# Copyright (c) 1991 by Sam Leffler.
# All rights reserved.
#
# This file is provided for unrestricted use provided that this
# legend is included on all tape media and as a part of the
# software program in whole or part.  Users may copy, modify or
# distribute this file at will.
#
include ../defs

CFILES=\
	fax2ps.c \
	faxdecode.c \
	${NULL}
TARGETS=fax2ps

LDIRT=	g3codes.h g32dtab.h mkg3tab mk2dtab
LCINCS=	-I${TIFF}
LIBS=	${TIFF}/libtiff.a
LLDLIBS= $(LIBS) -lsun -lbsd -lm

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

fax2ps:	${OBJECTS} ${LIBS}
	${CC} -o $@ ${OBJECTS} ${LDLAGS} ${LIBS}

faxdecode.o: faxdecode.c g3codes.h g32dtab.h
	${CC} ${CFLAGS} -c faxdecode.c
g3codes.h: mkg3tab.c ../libtiff/t4.h
	${CC} -o mkg3tab ${CFLAGS} mkg3tab.c
	./mkg3tab > g3codes.h
g32dtab.h: mk2dtab.c
	${CC} -o mk2dtab ${CFLAGS} mk2dtab.c
	./mk2dtab > g32dtab.h

install: default
	/etc/install -f ${BIN} -m 755 fax2ps
