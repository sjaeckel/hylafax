#! /bin/sh
#	$Id: pollrcvd.sh,v 1.20 1996/06/24 03:06:23 sam Rel $
#
# HylaFAX Facsimile Software
#
# Copyright (c) 1990-1996 Sam Leffler
# Copyright (c) 1991-1996 Silicon Graphics, Inc.
# HylaFAX is a trademark of Silicon Graphics
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
# pollrcvd mailaddr faxfile devID commID error-msg
#
if [ $# != 5 ]
then
    echo "Usage: $0 mailaddr faxfile devID commID error-msg"
    exit 1
fi

test -f etc/setup.cache || {
    SPOOL=`pwd`
    cat<<EOF

FATAL ERROR: $SPOOL/etc/setup.cache is missing!

The file $SPOOL/etc/setup.cache is not present.  This
probably means the machine has not been setup using the faxsetup(1M)
command.  Read the documentation on setting up HylaFAX before you
startup a server system.

EOF
    exit 1
}
. etc/setup.cache

INFO=$SBIN/faxinfo
FAX2PS=$TIFFBIN/fax2ps

MAILADDR="$1"
FILE="$2"
DEVICE="$3"
COMMID="$4"
MSG="$5"

(MIMEBOUNDARY="NextPart$$"
 echo "To: $MAILADDR"
 echo "From: The HylaFAX Receive Agent <fax>"
 echo "Subject: facsimile document received by polling request"
 echo "Mime-Version: 1.0"
 echo "Content-Type: Multipart/Mixed; Boundary=\"$MIMEBOUNDARY\""
 echo "Content-Transfer-Encoding: 7bit"
 echo ""
 $INFO $FILE
 echo "ReceivedOn: $DEVICE"
 if [ "$MSG" ]; then
    echo ""
    echo "The full document was not received because:"
    echo ""
    echo "    $MSG"
    echo ""
    echo "    ---- Transcript of session follows ----"
    echo ""
    if [ -f log/c$COMMID ]; then
	sed -e '/-- data/d' \
	    -e '/start.*timer/d' -e '/stop.*timer/d' \
	    log/c$COMMID
    elif [ -n "$COMMID" ]; then
	echo "    No transcript available (CommID c$COMMID)."
    else
	echo "    No transcript available."
    fi
 fi
 echo ""
 echo "--$MIMEBOUNDARY"
 echo "Content-Type: application/postscript"
 echo "Content-Description: FAX document"
 echo "Content-Transfer-Encoding: 7bit"
 echo ""
 $FAX2PS $FILE 2>/dev/null
 echo ""
 echo "--$MIMEBOUNDARY--"
) | 2>&1 $SENDMAIL -ffax -oi $MAILADDR
