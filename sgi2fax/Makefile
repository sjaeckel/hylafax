#! smake
# $Revision: 1.5 $
# $Date: 91/05/23 12:36:39 $
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
	imgtofax.c \
	hipass.c \
	izoom.c \
	lut.c \
	row.c \
	rand.c \
	${NULL}
TARGETS=sgi2fax

LCINCS=	-I${TIFF} -I/usr/include/gl
LIBS=	${TIFF}/libtiff.a
LLDLIBS= $(LIBS) -limage -lm

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

sgi2fax: ${OBJECTS} ${LIBS}
	${CC} -o $@ ${CFLAGS} ${OBJECTS} ${LLDLIBS}

install: default
	/etc/install -f ${BIN} -m 755 -O sgi2fax
