#! /bin/sh
# $Revision: 1.3 $
# $Date: 91/05/23 12:50:01 $
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
if test $# -lt 2
then
    echo "Usage: deliver file user1 user2 ..."
    exit 1
fi
FILE=$1; shift
(
/bin/echo "Subject: received facsimile";
/bin/echo "X-Doctype: TIFF/F";
/bin/echo "X-Docview: faxview -u";
/bin/echo "";
/bin/cat $FILE | /usr/bsd/uuencode fax.tif
) | /usr/lib/sendmail -oi $*
