#! /bin/sh
# $Revision: 1.2 $
# $Date: 91/05/23 12:50:24 $
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
# Convert text to PostScript for transmission as facsimile.
#
# text2fax [-m|-l] -o output input
#
fil=
output=
while test $# != 0
do	case "$1" in
	-m|-l)	;;
	-o)	shift; output=$1 ;;
	-*)	 ;;
	*)	fil="$fil $1" ;;
	esac
	shift
done
cat $fil | enscript -fCourier-Bold11 -p - >$output
