#!smake
#	$Header: /usr/people/sam/fax/man/RCS/Makefile,v 1.31 1994/06/28 15:29:36 sam Exp $
#
# FlexFAX Facsimile Software
#
# Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994 Sam Leffler
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

#
# Manual page targets that are installed.
#
# These may be formatted or unformatted versions of the sources
# above.  The transformation is controlled by definitions in the
# system-specific defs files.
#
MANCAPP=\
	cman.apps/flexfax.1	\
	cman.apps/fax2ps.1	\
	cman.apps/faxalter.1	\
	cman.apps/faxcover.1	\
	cman.apps/faxmail.1	\
	cman.apps/faxrm.1	\
	cman.apps/faxstat.1	\
	cman.apps/sendfax.1	\
	cman.apps/sgi2fax.1	\
	cman.apps/textfmt.1	\
	${NULL}
MANCFILE=\
	cman.files/pagesizes.4f	\
	cman.files/typerules.4f	\
	${NULL}
MANSAPP=\
	sman.apps/dialtest.1m	\
	sman.apps/faxabort.1m	\
	sman.apps/faxaddmodem.1m\
	sman.apps/faxanswer.1m	\
	sman.apps/faxcron.1m	\
	sman.apps/faxd.1m	\
	sman.apps/faxd.recv.1m	\
	sman.apps/faxquit.1m	\
	sman.apps/faxrcvd.1m	\
	sman.apps/mkcover.1m	\
	sman.apps/notify.1m	\
	sman.apps/pollrcvd.1m	\
	sman.apps/ps2fax.1m	\
	sman.apps/recvstats.1m	\
	sman.apps/tagtest.1m	\
	sman.apps/transcript.1m	\
	sman.apps/tsitest.1m	\
	sman.apps/wedged.1m	\
	sman.apps/xferstats.1m	\
	${NULL}
MANSFILE=\
	sman.files/flexfax.4f	\
	sman.files/cinfo.4f	\
	sman.files/config.4f	\
	sman.files/dialrules.4f	\
	sman.files/hosts.4f	\
	sman.files/info.4f	\
	sman.files/log.4f	\
	sman.files/recvq.4f	\
	sman.files/sendq.4f	\
	sman.files/status.4f	\
	sman.files/tsi.4f	\
	sman.files/xferlog.4f	\
	${NULL}
TARGETS=cman.apps ${MANCAPP}	\
	cman.files ${MANCFILE}	\
	sman.apps ${MANSAPP}	\
	sman.files ${MANSFILE}	\
	${NULL}

#
# System-specific manual page formatting commands should
# apply this sed command to the manual page source before
# handing it to the formatter.  This sets any pathnames
# in the man pages to reflect local conventions.
#
MANSED=${SED}\
	-e 's;\$${SPOOL};${SPOOL};g'\
	-e 's;\$${BIN};${BIN};g'\
	-e 's;\$${LIBDATA};${LIBDATA};g'\
	-e 's;\$${LIBEXEC};${LIBEXEC};g'\
	-e 's;\$${USRETC};${USRETC};g'\
	-e 's;\$${AFMDIR};${AFMDIR};g'\
	${MANSEDLOCAL}\
	${NULL}

default all targets:: ${TARGETS}

include ${COMMONRULES}

cman.apps: ${_FORCE}; test -d $@ || ${MKDIR} $@
cman.files: ${_FORCE}; test -d $@ || ${MKDIR} $@
sman.apps: ${_FORCE}; test -d $@ || ${MKDIR} $@
sman.files: ${_FORCE}; test -d $@ || ${MKDIR} $@

cman.apps/flexfax.1:: flexfax.1; ${MANCVT}
cman.apps/fax2ps.1:: fax2ps.1; ${MANCVT}
cman.apps/faxalter.1:: faxalter.1; ${MANCVT}
cman.apps/faxcover.1:: faxcover.1; ${MANCVT}
cman.apps/faxmail.1:: faxmail.1; ${MANCVT}
cman.apps/faxrm.1:: faxrm.1; ${MANCVT}
cman.apps/faxstat.1:: faxstat.1; ${MANCVT}
cman.apps/sendfax.1:: sendfax.1; ${MANCVT}
cman.apps/sgi2fax.1:: sgi2fax.1; ${MANCVT}
cman.apps/textfmt.1:: textfmt.1; ${MANCVT}
cman.files/pagesizes.4f:: pagesizes.4f; ${MANCVT}
cman.files/typerules.4f:: typerules.4f; ${MANCVT}
sman.apps/dialtest.1m:: dialtest.1m; ${MANCVT}
sman.apps/faxabort.1m:: faxabort.1m; ${MANCVT}
sman.apps/faxaddmodem.1m:: faxaddmodem.1m;  ${MANCVT}
sman.apps/faxanswer.1m:: faxanswer.1m; ${MANCVT}
sman.apps/faxcron.1m:: faxcron.1m; ${MANCVT}
sman.apps/faxd.1m:: faxd.1m; ${MANCVT}
sman.apps/faxd.recv.1m:: faxd.recv.1m; ${MANCVT}
sman.apps/faxquit.1m:: faxquit.1m; ${MANCVT}
sman.apps/faxrcvd.1m:: faxrcvd.1m; ${MANCVT}
sman.apps/mkcover.1m:: mkcover.1m; ${MANCVT}
sman.apps/notify.1m:: notify.1m; ${MANCVT}
sman.apps/pollrcvd.1m:: pollrcvd.1m; ${MANCVT}
sman.apps/ps2fax.1m:: ps2fax.1m; ${MANCVT}
sman.apps/recvstats.1m:: recvstats.1m; ${MANCVT}
sman.apps/tagtest.1m:: tagtest.1m; ${MANCVT}
sman.apps/transcript.1m:: transcript.1m;  ${MANCVT}
sman.apps/tsitest.1m:: tsitest.1m; ${MANCVT}
sman.apps/xferstats.1m:: xferstats.1m; ${MANCVT}
sman.apps/wedged.1m:: wedged.1m; ${MANCVT}
sman.files/flexfax.4f:: flexfax.4f; ${MANCVT}
sman.files/cinfo.4f:: cinfo.4f; ${MANCVT}
sman.files/config.4f:: config.4f; ${MANCVT}
sman.files/dialrules.4f:: dialrules.4f; ${MANCVT}
sman.files/hosts.4f:: hosts.4f; ${MANCVT}
sman.files/info.4f:: info.4f; ${MANCVT}
sman.files/log.4f:: log.4f; ${MANCVT}
sman.files/recvq.4f:: recvq.4f; ${MANCVT}
sman.files/sendq.4f:: sendq.4f; ${MANCVT}
sman.files/status.4f:: status.4f; ${MANCVT}
sman.files/tsi.4f:: tsi.4f; ${MANCVT}
sman.files/xferlog.4f:: xferlog.4f; ${MANCVT}

cman.apps/Makefile: ${MANCAPP}
	rm -f cman.apps/Makefile
	(echo '#!smake';						\
	 echo 'DEPTH=../..';						\
	 echo 'include $${DEPTH}/defs';					\
	 echo 'install:';						\
	 (cd cman.apps; for i in *.1; do				\
	    f=${MANCAPPNAME};						\
	    echo '	$${INSTALL} -m 444 -F $${MAN}/$${MANAPPS}	\
		-idb $${PRODUCT}.man.client -src' "$$i" '-O' "$$f";	\
	 done)								\
	)>cman.apps/Makefile
cman.files/Makefile: ${MANCFILE}
	rm -f cman.files/Makefile
	(echo '#!smake';						\
	 echo 'DEPTH=../..';						\
	 echo 'include $${DEPTH}/defs';					\
	 echo 'install:';						\
	 (cd cman.files; for i in *.4f; do				\
	    f=${MANCFILENAME};						\
	    echo '	$${INSTALL} -m 444 -F $${MAN}/$${MANFILES}	\
		-idb $${PRODUCT}.man.client -src' "$$i" '-O' "$$f";	\
	 done)								\
	)>cman.files/Makefile
sman.apps/Makefile: ${MANSAPP}
	rm -f smap.apps/Makefile
	(echo '#!smake';						\
	 echo 'DEPTH=../..';						\
	 echo 'include $${DEPTH}/defs';					\
	 echo 'install:';						\
	 (cd sman.apps; for i in *.1m; do				\
	    f=${MANSAPPNAME};						\
	    echo '	$${INSTALL} -m 444 -F $${MAN}/$${MANSYS}	\
		-idb $${PRODUCT}.man.client -src' "$$i" '-O' "$$f";	\
	 done)								\
	)>sman.apps/Makefile
sman.files/Makefile: ${MANSFILE}
	rm -f sman.files/Makefile
	(echo '#!smake';						\
	 echo 'DEPTH=../..';						\
	 echo 'include $${DEPTH}/defs';					\
	 echo 'install:';						\
	 (cd sman.files; for i in *.4f; do				\
	    f=${MANSFILENAME};						\
	    echo '	$${INSTALL} -m 444 -F $${MAN}/$${MANFILES}	\
		-idb $${PRODUCT}.man.client -src' "$$i" '-O' "$$f";	\
	 done)								\
	)>sman.files/Makefile

LDIRT=	cman.apps/Makefile \
	cman.files/Makefile \
	sman.apps/Makefile \
	sman.files/Makefile \
	${NULL}

installClientManPages:: cman.apps/Makefile cman.files/Makefile
	-${INSTALL} -m 755 -dir -idb flexfax.man.client \
	    ${MAN}/${MANAPPS} ${MAN}/${MANFILES}
	cd cman.apps; ${MAKE} install
	cd cman.files; ${MAKE} install

installServerManPages:: sman.apps/Makefile sman.files/Makefile
	-${INSTALL} -m 755 -dir -idb flexfax.man.server \
	    ${MAN}/${MANSYS} ${MAN}/${MANFILES}
	cd sman.apps; ${MAKE} install
	cd sman.files; ${MAKE} install

install: targets installClientManPages installServerManPages
