#	$Header: /usr/people/sam/fax/port/sgi/RCS/defs.cc,v 1.49 1994/07/03 03:40:54 sam Exp $
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

#
# These definitions are for building the software on an SGI 4D machine
# under IRIX 5.x using C++ 2.1 or later.  Note that some options to
# the C compiler may need to be changed depending on which version of
# IRIX you have installed.
#
# TARGET:	sgi
# COMPILER:	cc
#
SHELL	= /bin/sh
DESTDIR	= .
NULL	=
MAKEFILE= Makefile

#
# Location of auxiliary stuff needed for an SGI system
#
PORT	= ${DEPTH}/port/sgi
GENERIC	= ${DEPTH}/port/generic

C++	= ${TOOLROOT}/usr/bin/CC
CC	= ${TOOLROOT}/usr/bin/cc
NAWK	= ${TOOLROOT}/usr/bin/nawk
INSTALL	=${TOOLROOT}/etc/install
MKDEPEND=${TOOLROOT}/usr/sbin/mkdepend
SORT	= ${TOOLROOT}/bin/sort
GENDIST	=${TOOLROOT}/usr/sbin/gendist
AR	= ${TOOLROOT}/usr/bin/ar
RANLIB	= /bin/true
SED	= ${TOOLROOT}/bin/sed

#
# AR options for creating/updating an archive
#
AROPTS=crls

#
# Host bit order (FILLORDER_LSB2MSB or FILLORDER_MSB2LSB).
#
HOST_FILLORDER=FILLORDER_MSB2LSB

#
# Server tracing syslog facility.
#
LOG_FAX=LOG_DAEMON

#
# IRIX wants System V-style getty support.
#
SYSGETTY=GettySysV.c++

#
# IRIX wants System V-style UUCP lock support.
#
UUCP_LOCKDIR=/var/spool/locks
UUCP_LOCKTYPE=0
UUCP_LOCKMODE=0444

#
# Location of Adobe Font Metrics for lptops.
#
AFMDIR=/usr/lib/DPS/AFM

#
# PostScript imager for server.  PSPACKAGE is one of:
#
# dps	for Display Postscript
# gs	for Ghostscript
#
# and PSIMAGER is the pathname of the binary executable.
#
PSPACKAGE=dps
PSIMAGER=/var/spool/fax/bin/ps2fax.exe

#
# Mail program for sending notification messages.
#
FAX_MAILCMD=/bin/mail
SENDMAIL=/usr/lib/sendmail

#
# Default optimizer and prototype options
#
OPTIMIZER=-O
PROTOTYPES=-prototypes
CVERSION=-xansi

# default definitions for programs--overide them as desired
LIBS=	${LIBUTIL} ${LIBTIFF}
LLDLIBS=${LIBS} ${MACHDEPLIBS}
#
# Override this definition to eliminate shared library use.
#
SHDLIBC=-lc_s

#
# Libraries that may from machine to machine (especially if
# you use G++ instead of AT&T C++).
#
MACHDEPLIBS=-lmalloc -lsun -lC
#
# You must have v3.0 or newer of the TIFF library.  If
# you already have it installed, or if you have version 3.0
# or newer of InterViews installed (which includes the TIFF
# library), you can point these to those places.  Otherwise,
# you can just use the code provided here.
#
TIFF=	${DEPTH}/libtiff
LIBTIFF=${TIFF}/libtiff.a
# random utility functions and classes
UTIL=	${DEPTH}/util
LIBUTIL=${UTIL}/libfaxutil.a

# "product" name for SGI inst&co
PRODUCT=flexfax

#
# Uid&Gid for installing server stuff.  Note that the server process
# proper (/usr/etc/faxd or similar) must either run setuid root or
# setuid uucp (i.e. fax and uucp must have the same uid).  This is
# so that the fax server can participate in the UUCP locking scheme
# and thereby implement tty line sharing (i.e. share the same line
# and modem for both fax and data).
#
FAXUSER	= uucp
FAXGROUP= nuucp

#
# Directories where stuff gets placed:
#
# BIN		client-directed applications
# LIBDATA	client-directed application library data files
# LIBEXEC	client-directed application library executables
# SPOOL		spooling area for servers
# DOC		non-manual documentation
# USRETC	place for servers (faxd & faxd.recv)
#
BIN=	/usr/local/bin
LIBDATA=/usr/local/lib/fax
LIBEXEC=/usr/local/lib/fax
SPOOL=	/var/spool/fax
DOC=	/usr/local/doc/flexfax
USRETC=	/usr/etc

#
# Manual-related controls:
#
# MAN		head of manual tree
# MANAPP	subdirectory for client stuff in BIN
# MANSYS	subdirectory for server & server-related apps
# MANFILES	subdirectory for file format info 
#
MAN	= /usr/catman/local
MANAPPS	= man1
MANSYS	= man1
MANFILES= man4
#
# MANSEDLOCAL	additional sed commands to use during source transformation
#		(might want to transform section names here)
# MANCVT	command to convert source to installable target (can use
#		$? and $@, but not $<)
MANSEDLOCAL=
MANCVT=	${MANSED} $?| nroff -man | compress >$@
#
# The following macros are used during the install step to generate
# the filenames of the target manual pages.  They appear inside a
# shell for loop in a command of the form:
#
#	f=${MAN*NAME}
#
# where the variable ``i'' contains the filename of the formatted
# manual page (formatted according to the MANCVT macro).
#
# MANCAPPNAME	generate installed client app man page filename
# MANCFILENAME	generate installed client file format man page filename
# MANSAPPNAME	generate installed server app man page filename
# MANSFILENAME	generate installed server file format man page filename
#
#
MANCAPPNAME=`echo $$i | sed s/.1/.Z/`
MANCFILENAME=`echo $$i | sed s/.4f/.Z/`
MANSAPPNAME=`echo $$i | sed s/.1m/.Z/`
MANSFILENAME=`echo $$i | sed s/.4f/.Z/`

# programs used during installation
ECHO=	/bin/echo
GREP=	/bin/grep
RM=	/bin/rm
PWD=	/bin/pwd
MKDIR=	/bin/mkdir

#
# Definitions used by common rules.
#
COMMONRULES=${DEPTH}/rules
COMMONTARGS= clobber clean rmtargets depend incdepend

#
# C compiler flags are composed of variable (set on the command line),
# local (defined in the makefile), and global (defined in this file)
# parts, in that order.  This ordering is used so that variable or
# locally specified include directories are searched before the globally
# specified ones.
#
CFLAGS=	${CVERSION} ${VCFLAGS} ${LCFLAGS} ${GCFLAGS}

VCFLAGS=${VCDEFS} ${VCINCS} ${VCOPTS}
LCFLAGS=${LCDEFS} ${LCINCS} ${LCOPTS}
GCFLAGS=${GCDEFS} ${GCINCS} ${GCOPTS}

COPTS=	${VCOPTS} ${LCOPTS} ${GCOPTS}
CDEFS=	${VCDEFS} ${LCDEFS} ${GCDEFS}
CINCS=	${VCINCS} ${LCINCS} ${GCINCS}

GCOPTS=-float ${OPTIMIZER} ${PROTOTYPES}
GCDEFS=
GCINCS=-I${DEPTH} -I${PORT} -I${GENERIC} -I${UTIL} -I${TIFF}
#
# C++ flags are decomposed using the same hierarchy as C flags.
#
C++FLAGS=${CVERSION} ${VC++FLAGS} ${LC++FLAGS} ${GC++FLAGS}

VC++FLAGS=${VC++DEFS} ${VC++INCS} ${VC++OPTS}
LC++FLAGS=${LC++DEFS} ${LC++INCS} ${LC++OPTS}
GC++FLAGS=${GC++DEFS} ${GC++INCS} ${GC++OPTS}

C++OPTS=${VC++OPTS} ${LC++OPTS} ${GC++OPTS}
C++DEFS=${VC++DEFS} ${LC++DEFS} ${GC++DEFS}
C++INCS=${VC++INCS} ${LC++INCS} ${GC++INCS}

GC++OPTS=+a1 +w +p -float ${OPTIMIZER}
GC++INCS=-I${DEPTH} -I${PORT} -I${GENERIC} -I${UTIL} -I${TIFF} \
	-I${INCLDIR}/CC -I${INCLDIR}
GC++DEFS=-DfxSIGHANDLER=SIG_PF -DfxSIGACTIONHANDLER=SIG_PF

#
# Loader flags, composed of library (-l's) and option parts, with
# the libraries appearing last.  Both of these are divided into variable,
# local, and global parts.  The composition of LDFLAGS is done in the
# other "direction" from CFLAGS so that all the -L's, that are part of
# LDOPTS, appear before any of the -l's, which are part of LDLIBS.
# Another benefit of segregating the libraries from the remaining of the
# loader options is that the libraries alone can easily be given to
# another program, e.g., lint.
#
# Notes:
#   - If a program should not be linked with the shared version of libc,
#     then its make file should override the setting of SHDLIBC with a
#     line such as "SHDLIBC=".
#
LDFLAGS=${LDOPTS} ${LDLIBS}

LDOPTS=${VLDOPTS} ${LLDOPTS} ${GLDOPTS}
LDLIBS=${VLDLIBS} ${LLDLIBS} ${GLDLIBS}

GLDOPTS=-L -L${ROOT}/usr/lib
GLDLIBS=${SHDLIBC}

#
# Convenient command macros that include the flags macros.
#
C++F=	${C++} ${C++FLAGS}
CCF=	${CC} ${CFLAGS}

#
# Shell script for generating make dependencies.  MKDEPEND is a shorthand
# for the tool's absolute pathname.  MKDEPENDC adds MKDEPCFLAGS and the -c
# mkdepend option to this.  The other language's mkdepend variables try to
# include their language's name in the variable names.  Unfortunately, a
# lot of makefiles already use the nondescript LMKDEPFLAGS for C language
# mkdepend options, so we initialize LMKDEPCFLAGS with ${LMKDEPFLAGS}.
#
MKDEPENDC++	=${MKDEPEND} ${MKDEPC++FLAGS} -c "${C++F} ${C++FILE} -M"
MKDEPENDC	=${MKDEPEND} ${MKDEPCFLAGS} -c "${CCF} -M"

MKDEPC++FLAGS	=${VMKDEPC++FLAGS} ${LMKDEPC++FLAGS} ${GMKDEPC++FLAGS}
MKDEPCFLAGS	=${VMKDEPCFLAGS} ${LMKDEPCFLAGS} ${GMKDEPCFLAGS}
LMKDEPCFLAGS	=${LMKDEPFLAGS}

GMKDEPFLAGS	=-e 's@ ${INCLDIR}/@ $${INCLDIR}/@' -e 's@ ${ROOT}/@ $${ROOT}/@'
GMKDEPC++FLAGS	=${GMKDEPFLAGS} -s C++ -e 's@\.o++: @\.o: @'
GMKDEPCFLAGS	=${GMKDEPFLAGS}

#
# Macro to add to LMKDEPCFLAGS or LMKDEPC++FLAGS if your makefile builds
# single-source programs using null suffix rules (e.g., .c:}.  This option
# works for both C and C++ make depend.
#
NULLSUFFIX_MKDEPFLAG=-e 's@\.o+*:@:@'
#
# MKDEPFILE is the name of the dependency database, included by rules.
#
MKDEPFILE=Makedepend
#
# CDEPFILES lists all C or cc-compiled source files that depend on header
# files computable by ${MKDEPENDC}.  C++DEPFILES lists all C++ files having
# dependencies computable by ${MKDEPENDC++}.
#
C++DEPFILES=${C++FILES}
CDEPFILES=${CFILES}
DEPFILES=${C++DEPFILES} ${CDEPFILES}

#
# Directory shorthands, mainly for make depend (see GMKDEPFLAGS above).
#
INCLDIR	=${ROOT}/usr/include
