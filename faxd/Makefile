#! smake
#
# $Revision: 1.25 $
# $Date: 91/09/12 12:31:53 $
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
	FaxModem.c++ \
	Everex.c++ \
	EverexRecv.c++ \
	EverexSend.c++ \
	Class2.c++ \
	Class2Send.c++ \
	Class2Recv.c++ \
	Class2Tab.c++ \
	\
	FaxServer.c++ \
	FaxSend.c++ \
	FaxRecv.c++ \
	FaxRecvdData.c++ \
	FaxRequest.c++ \
	FaxRequestData.c++ \
	FaxMachineInfo.c++ \
	FIFOServer.c++ \
	RegExArray.c++ \
	faxServerApp.c++ \
	${NULL}
TARGETS=faxd

LC++INCS=\
	-I${TIFF} \
	-I${UTIL} \
	-I${ROOT}/usr/include/fxds \
	${NULL}
LIBS=\
	${UTIL}/libfaxutil.a \
	${ROOT}/usr/lib/libfxds.a \
	${TIFF}/libtiff.a \
	${NULL}
#LLDLIBS= $(LIBS) -lC -lsun -lbsd -lm -lc
LLDLIBS= $(LIBS) -lC -lsun -lbsd -lm

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

faxd:	${OBJECTS} ${LIBS}
	$(C++F) -g -o $@ ${OBJECTS} ${LDFLAGS}

install: default
	/etc/install -f ${ETC} -u fax -m 4755 -O faxd
