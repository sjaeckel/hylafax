#! smake
# $Revision: 1.4 $
# $Date: 91/05/23 12:33:49 $
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

C++FILES=\
	sendfax.c++ \
	${NULL}
TARGETS=sendfax

LC++INCS=-g\
	-I${UTIL} \
	-I${TIFF} \
	-I${ROOT}/usr/include/fxds \
	${NULL}
LIBS=\
	${UTIL}/libfaxutil.a \
	${TIFF}/libtiff.a \
	${ROOT}/usr/lib/libfxds.a \
	${NULL}
#LLDLIBS= $(LIBS) -lC -lsun -lbsd -lm -lc
LLDLIBS= $(LIBS) -lC -lsun -lbsd -lm

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

sendfax: sendfax.o ${LIBS}
	${C++F} -o $@ sendfax.o ${LDFLAGS}

install: default
	/etc/install -f ${BIN} -m 755 -O sendfax
