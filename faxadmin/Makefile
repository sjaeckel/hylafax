#! smake
# $Revision: 1.8 $
# $Date: 91/05/23 12:13:14 $
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
	faxAdmin.c++ \
	Config.c++ \
	Help.c++ \
	RecvQ.c++ \
	RecvQList.c++ \
	RecvQueue.c++ \
	SendQ.c++ \
	SendQList.c++ \
	SendQueue.c++ \
	Server.c++ \
	\
	StrDialog.c++ \
	${NULL}
TARGETS=faxadmin

LC++INCS=\
	-I../faxd \
	-I${UTIL} \
	-I${TIFF} \
	-I${ROOT}/usr/include/fxds \
	-I${ROOT}/usr/include/fxui \
	${NULL}
LIBS=\
	${UTIL}/libfaxutil.a \
	${ROOT}/usr/lib/${LIBFXUI}.a \
	${ROOT}/usr/lib/libfxds.a \
	${TIFF}/libtiff.a \
	${NULL}
#LLDLIBS= $(LIBS) -lfm -lgl ${WSLIB} -lC -lsun -lbsd -lm -lc
LLDLIBS= $(LIBS) -lfm_s -lgl_s ${WSLIB} -lC -lsun -lbsd -lm

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

faxadmin: ${OBJECTS} ${LIBS}
	$(C++F) -o $@ ${OBJECTS} ${LDFLAGS}

install: default
	/etc/install -f /usr/lib/vadmin -u ${FAXUSER} \
	    -m 4755 -src faxadmin -O fax
	/etc/install -f /usr/lib/vadmin -m 444 fax.hlp
