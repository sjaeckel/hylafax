#! smake
#	$Header: /usr/people/sam/fax/faxd/RCS/Makefile,v 1.69 1994/06/22 04:51:45 sam Exp $
#
# FlexFAX Facsimile Software
#
# Copyright (c) 1990, 1991, 1992, 1993, 1994 Sam Leffler
# Copyright (c) 1991, 1992, 1993, 1994 Silicon Graphics, Inc.
# 
# Permission to use, copy, modify, distribute, and sell this software and 
# its documentation for any purpose is hereby granted without fee, provided
# that (i) the above copyright notices and this permission notice appear in
# all copies of the software and related documentation, and (ii) the names of
# Sam Leffler and Silicon Graphics may not be used in any advertising or
# publicity relating to the software without the specific, prior written
# permission of Sam Leffler and Silicon Graphics.
# 
# THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
# EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
# WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
# 
# IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
# ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
# OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
# WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
# LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
# OF THIS SOFTWARE.
#
DEPTH=..
include ${DEPTH}/defs

CFILES=\
	tif_compress.c \
	${NULL}
C++FILES=\
	FaxModem.c++ \
	HDLCFrame.c++ \
	ModemConfig.c++ \
	\
	G3Base.c++ \
	G3Decoder.c++ \
	G3Encoder.c++ \
	CopyQuality.c++ \
	\
	FaxFont.c++ \
	PCFFont.c++ \
	TagLine.c++ \
	\
	Class1.c++ \
	Class1Poll.c++ \
	Class1Send.c++ \
	Class1Recv.c++ \
	Class2.c++ \
	Class2Params.c++ \
	Class2Poll.c++ \
	Class2Recv.c++ \
	Class2Send.c++ \
	\
	Class2Ersatz.c++ \
	Class20.c++ \
	\
	FaxServer.c++ \
	FaxPoll.c++ \
	FaxRecv.c++ \
	FaxSend.c++ \
	FaxRecvInfo.c++ \
	FaxRequest.c++ \
	FaxMachineInfo.c++ \
	FaxMachineLog.c++ \
	UUCPLock.c++ \
	\
	Getty.c++ \
	${SYSGETTY} \
	\
	faxServerApp.c++ \
	${NULL}
TARGETS=faxd tsitest tagtest

LLDLIBS= ${LIBS} ${MACHDEPLIBS} -lm

default all::
	@${MAKE} incdepend
	@${MAKE} targets

targets: $(TARGETS)

include ${COMMONRULES}

faxd:	${OBJECTS} ${LIBS}
	${C++F} -o $@ ${OBJECTS} ${LDFLAGS}

UUCPLock.o: UUCPLock.c++
	${C++F} -c ${C++FILE} \
	    -DUUCP_LOCKTYPE=${UUCP_LOCKTYPE} \
	    -DUUCP_LOCKMODE=${UUCP_LOCKMODE} \
	    -DUUCP_LOCKDIR=\"${UUCP_LOCKDIR}\" \
	    UUCPLock.c++

TAGOBJS=\
	tagtest.o \
	PCFFont.o \
	FaxFont.o \
	G3Base.o \
	G3Decoder.o \
	G3Encoder.o \
	Class2Params.o \
	${NULL}
tagtest: ${TAGOBJS}
	${C++F} -o $@ ${TAGOBJS} ${LDFLAGS}
tsitest: tsitest.o
	${C++F} -o $@ tsitest.o ${LDFLAGS}

PUTSERV=${INSTALL} -idb ${PRODUCT}.sw.server

install: default
	${PUTSERV} -F ${USRETC} -u ${FAXUSER} -m 4755 -O faxd
	${PUTSERV} -F ${SPOOL}/bin -m 755 -O tsitest tagtest
