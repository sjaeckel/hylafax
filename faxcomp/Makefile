#! smake
# $Revision: 1.1 $
# $Date: 91/06/04 17:03:19 $
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
	faxcomp.c++ \
	promptWindow.c++ \
	${NULL}
TARGETS=faxcomp

LC++INCS=\
	-I${UTIL}\
	-I${ROOT}/usr/include/fxds\
	-I${ROOT}/usr/include/fxui
LIBS=\
	${UTIL}/libfaxutil.a \
	${LIBKIT}\
	${ROOT}/usr/lib/${LIBFXUI}.a\
	${ROOT}/usr/lib/libfxds.a\
	${NULL}
LLDLIBS= $(LIBS) -lfm_s -lgl_s ${WSLIB} -lsun -lC -lm -lmalloc -lC -lc_s
#LLDLIBS= $(LIBS) -lfm -lgl ${WSLIB} -lsun -lC -lm -lc -lmalloc -lC -lc

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

faxcomp: ${OBJECTS} ${LIBS}
	$(C++F) -o $@ ${OBJECTS} ${LDFLAGS}

install: default
	/etc/install -f ${BIN} -m 755 -O faxcomp
