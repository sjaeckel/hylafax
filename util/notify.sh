#! /bin/sh
# $Revision: 1.2 $
# $Date: 91/05/23 12:50:20 $
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
if test $# != 1
then
    echo "Usage: notify file"
    exit 1
fi
(
/bin/echo "Subject: received facsimile";
/bin/echo "";
/usr/spool/fax/bin/faxinfo $1
) | 2>&1 /usr/lib/sendmail -oi FaxMaster
