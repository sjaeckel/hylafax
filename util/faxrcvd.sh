#! /bin/sh
#	$Id: faxrcvd.sh,v 1.33 1998/02/12 10:04:57 guru Rel $
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
# faxrcvd file devID commID error-msg
#
if [ $# != 4 ]; then
    echo "Usage: $0 file devID commID error-msg"
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
TOADDR=FaxMaster
#
# There is no good portable way to find out the fully qualified
# domain name (FQDN) of the host or the TCP port for the hylafax
# service so we fudge here.  Folks may want to tailor this to
# their needs; e.g. add a domain or use localhost so the loopback
# interface is used.
#
HOSTNAME=`hostname`			# XXX no good way to find FQDN
PORT=4559				# XXX no good way to lookup service

FILE="$1"
DEVICE="$2"
COMMID="$3"
MSG="$4"

if [ -f $FILE ]; then
    #
    # Check the sender's TSI and setup to dispatch
    # facsimile received from well-known senders.
    #
    SENDER="`$INFO $FILE | $AWK -F: '/Sender/ { print $2 }' 2>/dev/null`"
    SENDTO=
    if [ -f etc/FaxDispatch ]; then
	. etc/FaxDispatch	# NB: FaxDispatch sets SENDTO based on $SENDER
    fi
    (echo "To: $TOADDR"
     echo "From: The HylaFAX Receive Agent <fax>"
     echo "Subject: facsimile received from $SENDER";
     echo ""
     echo "$FILE (ftp://$HOSTNAME:$PORT/$FILE):"; $INFO -n $FILE
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
	    $SED -e '/-- data/d' \
		 -e '/start.*timer/d' -e '/stop.*timer/d' \
		 log/c$COMMID
	elif [ -n "$COMMID" ]; then
	    echo "    No transcript available (CommID c$COMMID)."
	else
	    echo "    No transcript available."
	fi
     else
	echo "CommID:     c$COMMID (ftp://$HOSTNAME:$PORT/log/c$COMMID)"
     fi
     if [ -n "$SENDTO" ]; then
	echo ""
	echo "The facsimile was automatically dispatched to: $SENDTO." 
     fi
    ) | 2>&1 $SENDMAIL -ffax -oi $TOADDR
    if [ -n "$SENDTO" ]; then
	(MIMEBOUNDARY="NextPart$$"
	 echo "Mime-Version: 1.0"
	 echo "Content-Type: Multipart/Mixed; Boundary=\"$MIMEBOUNDARY\""
	 echo "Content-Transfer-Encoding: 7bit"
	 echo "To: $SENDTO"
	 echo "From: The HylaFAX Receive Agent <fax>"
	 echo "Subject: facsimile received from $SENDER";
	 echo ""
	 echo "--$MIMEBOUNDARY"
	 echo "Content-Type: text/plain; charset=us-ascii"
	 echo "Content-Transfer-Encoding: 7bit"
	 echo ""
	 echo "$FILE (ftp://$HOSTNAME:$PORT/$FILE):"; $INFO -n $FILE
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
		$SED -e '/-- data/d' \
		     -e '/start.*timer/d' -e '/stop.*timer/d' \
		     log/c$COMMID
	    elif [ -n "$COMMID" ]; then
		echo "    No transcript available (CommID c$COMMID)."
	    else
		echo "    No transcript available."
	    fi
	 else
	    echo "CommID:     c$COMMID (ftp://$HOSTNAME:$PORT/log/c$COMMID)"
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
	) | 2>&1 $SENDMAIL -ffax -oi $SENDTO
    fi
else
    #
    # Generate notification mail for a failed attempt.
    #
    (echo "To: $TOADDR"
     echo "From: The HylaFAX Receive Agent <fax>"
     echo "Subject: facsimile not received"
     echo ""
     echo "An attempt to receive facsimile on $DEVICE failed because:"
     echo ""
     echo "    $MSG"
     echo ""
     echo "    ---- Transcript of session follows ----"
     echo ""
     if [ -f log/c$COMMID ]; then
	$SED -e '/-- data/d' \
	     -e '/start.*timer/d' -e '/stop.*timer/d' \
	    log/c$COMMID
     elif [ -n "$COMMID" ]; then
	echo "    No transcript available (CommID c$COMMID)."
     else
	echo "    No transcript available."
     fi
    ) | 2>&1 $SENDMAIL -ffax -oi $TOADDR
fi
