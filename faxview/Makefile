#! smake
# $Revision: 1.9 $
# $Date: 91/08/04 13:06:50 $
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

LDIRT=	mkbitcount bitcount.h
C++FILES=\
	FAXViewer.c++ \
	ImageView.c++ \
	Bitmap.c++ \
	Bitblt.c++ \
	Segment.c++ \
	${NULL}
TARGETS=faxview

LC++INCS=\
	-I${UTIL}\
	-I${TIFF}\
	-I${ROOT}/usr/include/fxds\
	-I${ROOT}/usr/include/fxui
LIBS=\
	${LIBKIT}\
	${ROOT}/usr/lib/${LIBFXUI}.a\
	${ROOT}/usr/lib/libfxds.a\
	${TIFF}/libtiff.a
LLDLIBS= $(LIBS) -lfm_s -lgl_s ${WSLIB} -lsun -lC -lm -lmalloc -lC -lc_s
#LLDLIBS= $(LIBS) -lfm -lgl ${WSLIB} -lsun -lC -lm -lc -lmalloc -lC -lc

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

faxview: ${OBJECTS} ${LIBS}
	$(C++F) -o $@ ${OBJECTS} ${LDFLAGS}
ImageView.o: bitcount.h

bitcount.h: mkbitcount.c
	${CC} -o mkbitcount mkbitcount.c
	rm -f bitcount.h; ./mkbitcount > bitcount.h

install: default
	/etc/install -f ${BIN} -m 755 -O faxview
