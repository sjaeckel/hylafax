#!smake
#	$Header: /usr/people/sam/fax/etc/RCS/Makefile,v 1.42 1994/07/05 02:59:14 sam Exp $
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

CONFIGFILES=\
	config.skel \
	config.class1 \
	config.att-dataport \
	config.att-dataport-2 \
	config.att-dataport-2.0 \
	config.class2 \
	config.class2.0 \
	config.dsi-scout+ \
	config.e-tech \
	config.everex-2496d \
	config.intel-400e \
	config.mt-1432 \
	config.nuvo-voyager \
	config.ppi-pm14400fxmt \
	config.ppi-pm14400fxsa \
	config.rc32acl \
	config.rc32acl-class1 \
	config.rockwell \
	config.rockwell-class1 \
	config.telebit-wb \
	config.tricom2842 \
	config.usr-courier \
	config.usr-courier-2.0 \
	config.usr-sportster \
	config.zyxel-1496e \
	config.zyxel-1496e-2.0 \
	${NULL}
DIALRULES=\
	dialrules \
	dialrules.sf-ba \
	dialrules.europe \
	${NULL}
SRCS=\
	faxd \
	faxaddmodem.sh \
	${CONFIGFILES} \
	${DIALRULES} \
	${NULL}
TARGETS=\
	config.fax \
	hosts \
	xferlog \
	${NULL}

default all::
	@${MAKE} targets

targets: ${TARGETS}

include $(COMMONRULES)

hosts::
	${ECHO} "127.0.0.1" >hosts
xferlog::
	${ECHO} >xferlog
config.fax::
	${ECHO} on >config.fax

PUTCLIENT=	${INSTALL} -m 644 -u ${FAXUSER} -g ${FAXGROUP}
PUTSERVER=${INSTALL} -idb ${PRODUCT}.sw.server
PUTSAMPLE=${PUTCLIENT} -idb "${PRODUCT}.sw.server config(noupdate)"

install: default ${CONFIGFILES}
	${PUTSERVER} -F ${SPOOL}/bin -m 755 -src faxaddmodem.sh -O faxaddmodem
	${PUTSAMPLE} -F ${SPOOL}/etc -o hosts xferlog
	${PUTSERVER} -F ${SPOOL}/config -o -m 444 ${CONFIGFILES}
	${PUTSERVER} -F ${SPOOL}/etc -o -m 444 ${DIALRULES}

installInit: faxd
	${PUTSERVER} -F /etc/init.d -m 755 -O faxd
	${PUTSERVER} -F /etc/config -m 644 -src config.fax -O fax
	${PUTSERVER} -F /etc/rc2.d -lns /etc/init.d/faxd S80faxd
	${PUTSERVER} -F /etc/rc0.d -lns /etc/init.d/faxd K80faxd
