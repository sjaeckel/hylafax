#! smake
#
# $Revision: 1.4 $
# $Date: 91/05/31 12:04:55 $
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
	alter.c \
	auth.c \
	jobs.c \
	main.c \
	remove.c \
	status.c \
	submit.c \
	${NULL}
TARGETS=faxd.recv

LCINCS=	-I${UTIL} -I${TIFF}
LIBS=\
	${TIFF}/libtiff.a \
	${NULL}
#LLDLIBS= $(LIBS) -lsun -lbsd -lc
LLDLIBS= $(LIBS) -lsun -lbsd

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

faxd.recv: ${OBJECTS} ${LIBS}
	${CC} -o $@ ${OBJECTS} ${LDFLAGS}

install: default
	/etc/install -f ${ETC} -m 755 -O faxd.recv
