#! smake
#	$Header: /usr/people/sam/fax/util/RCS/Makefile,v 1.77 1994/06/28 00:26:01 sam Exp $
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

C++FILES=\
    Array.c++ \
    BoolArray.c++ \
    Dictionary.c++ \
    Obj.c++ \
    PageSize.c++ \
    RegEx.c++ \
    RegExArray.c++ \
    StackBuffer.c++ \
    Str.c++ \
    StrArray.c++ \
    \
    Dispatcher.c++ \
    IOHandler.c++ \
    \
    AtSyntax.c++ \
    DialRules.c++ \
    FaxDB.c++ \
    FaxClient.c++ \
    SendFaxClient.c++ \
    TypeRules.c++ \
    Fatal.c++ \
    ${NULL}
CFILES=\
    fxassert.c \
    ${NULL}
REGEXOBJS=\
    regexec.o \
    regcomp.o \
    regfree.o \
    regerror.o \
    ${NULL}
TARGETS=libfaxutil.a faxmsg faxinfo textfmt ondelay dialtest typetest

default all::
	@${MAKE} incdepend
	@${MAKE} targets

targets: $(TARGETS)

include ${COMMONRULES}

libfaxutil.a: ${OBJECTS} ${REGEXOBJS}
	@rm -f $@;
	@echo "${AR} ${AROPTS} $@ ${OBJECTS} ${REGEXOBJS}";\
	   ${AR} ${AROPTS} $@ ${OBJECTS} ${REGEXOBJS} $(C++FILT)
	${RANLIB} $@
../util/libfaxutil.a: libfaxutil.a

regexec.o: regex/regexec.c; ${CCF} -c -Iregex regex/regexec.c
regcomp.o: regex/regcomp.c; ${CCF} -c -Iregex regex/regcomp.c
regfree.o: regex/regfree.c; ${CCF} -c -Iregex regex/regfree.c
regerror.o: regex/regerror.c; ${CCF} -c -Iregex regex/regerror.c

faxmsg: faxmsg.c ${LIBS}
	${CCF} -o $@ faxmsg.c ${LDFLAGS}
#
# Load faxinfo w/ the C++ compiler since it uses the
# PageSize support in the utility library.
#
faxinfo: faxinfo.o ${LIBS}
	${C++F} -o $@ faxinfo.o ${LDFLAGS}

textfmt.o: textfmt.c++
	${C++F} -c ${C++FILE} \
	    -DFONTDIR=\"${AFMDIR}\" ${LPTOPSLCOPTS} \
	    textfmt.c++
textfmt: textfmt.o ${LIBS}
	${C++F} -o $@ textfmt.o ${LDFLAGS}
ondelay: ondelay.c
	${CCF} -o $@ ondelay.c

dialtest: dialtest.o DialRules.o RegExArray.o ${LIBS}
	${C++F} -o $@ dialtest.o DialRules.o RegExArray.o ${LDFLAGS}
typetest: typetest.o TypeRules.o ${LIBS}
	${C++F} -o $@ typetest.o TypeRules.o ${LDFLAGS}

LDIRT=	checkat
checkat: checkat.o libfaxutil.a
	${C++F} -o $@ checkat.o ${LDFLAGS}

PUTSERV=${INSTALL} -m 755 -idb ${PRODUCT}.sw.server
PUTSSUG=${INSTALL} -m 755 -idb "${PRODUCT}.sw.server config(suggest)"
PUTSSUG444=${INSTALL} -m 444 -idb "${PRODUCT}.sw.server config(suggest)"

install: default
	${INSTALL} -F ${LIBEXEC} -m 755 -idb ${PRODUCT}.sw.client -O textfmt
	${INSTALL} -F ${LIBDATA} -m 444 -idb ${PRODUCT}.sw.client -O \
	    pagesizes typerules
	${PUTSERV} -F ${SPOOL}/bin -O faxmsg
	${INSTALL} -idb ${PRODUCT}.sw.server -F ${SPOOL}/bin -O \
	    -ln ${SPOOL}/bin/faxmsg faxabort faxanswer faxquit
	${PUTSERV} -F ${SPOOL}/bin -O faxinfo ondelay dialtest typetest
	${PUTSERV} -F ${SPOOL}/bin -src ps2fax.gs.sh -O ps2fax.gs
	${PUTSERV} -F ${SPOOL}/bin -src ps2fax.dps.sh -O ps2fax.dps
	${PUTSSUG} -F ${SPOOL}/bin -src notify.sh -O notify
	${PUTSSUG444} -F ${SPOOL}/bin -O notify.awk
	${PUTSSUG} -F ${SPOOL}/bin -src faxrcvd.sh -O faxrcvd
	${PUTSSUG} -F ${SPOOL}/bin -src pollrcvd.sh -O pollrcvd
	${PUTSERV} -F ${SPOOL}/bin -src transcript.sh -O transcript
	${PUTSERV} -F ${SPOOL}/bin -src xferstats.sh -O xferstats
	${PUTSERV} -F ${SPOOL}/bin -src recvstats.sh -O recvstats
	${PUTSERV} -F ${SPOOL}/bin -src faxcron.sh -O faxcron
	${PUTSERV} -F ${SPOOL}/bin -src wedged.sh -O wedged
	${INSTALL} -F ${SPOOL}/etc -m 444 \
	    -idb ${PRODUCT}.sw.server -O dpsprinter.ps 
	${INSTALL} -F ${SPOOL}/bin -idb "${PRODUCT}.sw.server config(suggest)"\
	     -ln ${SPOOL}/bin/ps2fax.${PSPACKAGE} -O ps2fax
	${PUTSSUG} -F ${SPOOL}/bin -src mkcover.sh -O mkcover
	${PUTSSUG444} -F ${SPOOL}/etc -O cover.templ
