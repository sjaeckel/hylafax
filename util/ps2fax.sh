#!/bin/sh
# $Revision: 1.3 $
# $Date: 91/05/23 12:50:21 $
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
# Convert PostScript to facsimile.
#
# ps2fax [-o output] [-l pagelength] [-w pagewidth]
#	[-r resolution] [-*] file ...
#
# We need to process the arguments to extract the input
# files so that we can prepend a prologue file that defines
# LaserWriter-specific stuff as well as to insert and error
# handler that generates ASCII diagnostic messages when
# a problem is encountered in the interpreter.
#
# NB: this shell script is assumed to be run from the
#     top of the spooling hierarchy -- s.t. the etc directory
#     is present.
#
fil=
opt=
while test $# != 0
do case "$1" in
    -o)	shift; opt="$opt -o $1" ;;
    -l)	shift; opt="$opt -l $1" ;;
    -w)	shift; opt="$opt -w $1" ;;
    -r)	shift; opt="$opt -r $1" ;;
    -*)	opt="$opt $1" ;;
    *)	fil="$fil $1" ;;
    esac
    shift
done
/bin/cat etc/dpsprinter.ps $fil | /usr/local/bin/fax/ps2fax $opt
