#! /bin/sh
#	$Header: /usr/people/sam/fax/util/RCS/notify.sh,v 1.9 1994/06/14 21:56:18 sam Exp $
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
SENDMAIL=/usr/lib/sendmail
#
PS2FAX=bin/ps2fax
TRANSCRIPT=bin/transcript
AWKSCRIPT=bin/notify.awk
TAR=tar
AWK=nawk
ENCODE=uuencode
DECODE=uudecode
COMPRESS=compress
DECOMPRESS=zcat
#
# notify qfile why jobtime server-pid number [nextTry]
#
# Return mail to the submitter of a job when notification is needed.
#

PATH=/bin:/usr/bin:
test -d /usr/ucb  && PATH=$PATH:/usr/ucb		# Sun and others
test -d /usr/bsd  && PATH=$PATH:/usr/bsd		# Silicon Graphics
test -d /usr/5bin && PATH=/usr/5bin:$PATH:/usr/etc	# Sun and others
test -d /usr/sbin && PATH=/usr/sbin:$PATH		# 4.4BSD-derived

if [ $# != 5 -a $# != 6 ]; then
    echo "Usage: $0 qfile why jobtime pid number [nextTry]"
    exit 1
fi
qfile=$1
nextTry=${6:-'??:??'}

# look for an an awk: nawk, gawk, awk
($AWK '{}' </dev/null >/dev/null) 2>/dev/null ||
    { AWK=gawk; ($AWK '{}' </dev/null >/dev/null) 2>/dev/null || AWK=awk; }

($AWK -F: -f $AWKSCRIPT why=$2 jobTime=$3 pid=$4 canon=$5 nextTry=$nextTry \
  ps2fax=$PS2FAX transcript=$TRANSCRIPT\
  tar=$TAR \
  encoder=$ENCODE decoder=$DECODE \
  compressor=$COMPRESS decompressor=$DECOMPRESS \
  $qfile || {
      echo ""
      echo "Sorry, there was a problem doing send notification;"
      echo "something went wrong in the shell script $0."
      echo ""
      exit 1;
  }
) | 2>&1 $SENDMAIL -t -ffax -oi
