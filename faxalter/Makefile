#! smake
# $Revision: 1.1 $
# $Date: 91/05/31 12:51:03 $
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
	faxalter.c++ \
	${NULL}
TARGETS=faxalter

LC++INCS=\
	-I${UTIL}\
	-I${ROOT}/usr/include/fxds\
	${NULL}
LIBS=\
	${UTIL}/libfaxutil.a \
	${ROOT}/usr/lib/libfxds.a\
	${NULL}
#LLDLIBS= $(LIBS) -lC -lsun -lbsd -lc
LLDLIBS= $(LIBS) -lC -lsun -lbsd

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

faxalter: faxalter.o ${LIBS}
	${C++F} -o $@ faxalter.o ${LDFLAGS}

install: default
	/etc/install -f ${BIN} -m 755 -O faxalter
