#! /bin/sh
#	$Header: /usr/people/sam/fax/util/RCS/faxrcvd.sh,v 1.12 1994/06/17 19:08:55 sam Exp $
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
SPOOL=/var/spool/fax
SENDMAIL=/usr/lib/sendmail
#
# faxrcvd qfile time protocol sigrate error-msg [devID]
#
if test $# != 5 -a $# != 6
then
    echo "Usage: $0 qfile time sigrate protocol error-msg [devID]"
    exit 1
fi
(
echo "Subject: received facsimile";
echo "";
$SPOOL/bin/faxinfo $1
cat<<EOF
TimeToRecv: $2 minutes
SignalRate: $3
DataFormat: $4
ReceivedOn: ${6:-Unspecified Device}
EOF
if [ "$5" ]; then
    echo ""
    echo "The full document was not received because:"
    echo ""
    echo "    $5"
fi
) | 2>&1 $SENDMAIL -ffax -oi FaxMaster
