#! /bin/sh
#	$Id: tiff2fax.sh,v 1.12 1996/08/29 23:22:16 sam Rel $
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
# Convert TIFF to fax as needed.
#
# tiff2fax [-o output] [-l pagelength] [-w pagewidth]
#	[-r resolution] [-m maxpages] [-1] [-2] file ...
#
# NB: This script uses the tiffcp program from the TIFF
#     software distribution to do certain format conversions.
#     The TIFF distribution is available by ftp at
#     ftp://ftp.sgi.com/graphics/tiff/; be sure to get
#     v3.4beta016 or later.
#

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

CHECK=$SBIN/tiffcheck			# program to check acceptability
PS2FAX=bin/ps2fax			# for hard conversions
TIFFCP=$TIFFBIN/tiffcp			# part of the TIFF distribution
TIFF2PS=$TIFFBIN/tiff2ps		# ditto

out=foo.tif				# default output filename
df=1d					# default output is 1D-encoded
fil=
opt=
while test $# != 0
do case "$1" in
    -o)	shift; out=$1 ;;
    -l)	shift; opt="$opt -l $1" ;;
    -w)	shift; opt="$opt -w $1" ;;
    -r)	shift; opt="$opt -r $1" ;;
    -1) opt="$opt $1"; df=1d ;;
    -2) opt="$opt $1"; df=2d ;;
    -m) shift;;				# NB: not implemented
    *)	fil="$fil $1" ;;
    esac
    shift
done
test -z "$fil" && {
    echo "$0: No input file specified."
    exit 255
}

#
# tiffcheck looks over a TIFF document and prints out a string
# that describes what's needed (if anything) to make the file
# suitable for transmission with the specified parameters (page
# width, page length, resolution, encoding).  This string may
# be followed by explanatory messages that can be returned to
# the user.  The possible actions are:
#
# OK		document is ok
# REJECT	something is very wrong (e.g. not valid TIFF)
# REFORMAT	data must be re-encoded
# REVRES	reformat to change vertical resolution
# RESIZE	scale or truncate the pages
# REIMAGE	image is not 1-channel bilevel data
#
# Note that these actions may be combined with "+";
# e.g. REFORMAT+RESIZE.  If we cannnot do the necessary work
# to prepare the document then we reject it here.
#
RESULT=`$CHECK $opt $fil 2>/dev/null`

ACTIONS=`echo "$RESULT" | $SED 1q`
case "$ACTIONS" in
OK)				# no conversion needed
    #
    # Two things to be aware of here.  We avoid using hard
    # links because it screws up faxqclean logic that assumes
    # the only hard links are used temporarily when document
    # files are being created during the job submission process.
    # Second, faxq creates the target file and locks it as
    # part of the logic used to insure only one job at a time
    # images a document.  Because we clobber the target file
    # in this case we potentially open a window where someone
    # may decide to image the same document.  We could depend
    # on $LN doing the clobber for us and not remove the file
    # first but on some systems this won't happen even though
    # we are not an interactive process (so there's no way to
    # prompt for a confirmation before clobbering the target).
    # An alternative to all this is to do something like copy
    # $fil to $out but then we would waste disk space.  We'll
    # leave it like this for now and see if problems arise in
    # which case we can always substitute a program that does
    # the right thing.
    #
    f=`echo $fil | $SED 's;.*/;;'`
    $RM -f $out; $LN -s $f $out
    exit 0			# successful conversion
    ;;
*REJECT*)			# document rejected out of hand
    echo "$RESULT" | $SED 1d
    exit 254			# reject document
    ;;
REFORMAT)			# only need format conversion (e.g. g4->g3)
    $TIFFCP -i -c g3:$df -f lsb2msb -r 9999 $fil $out || {
	$CAT<<EOF
Unexpected failure converting TIFF document; the command

    $TIFFCP -i -c g3:$df -f lsb2msb -r 9999 $fil $out

failed with exit status $?.  This conversion was done because:

EOF
	echo "$RESULT" | $SED 1d; exit 254
    }
    exit 0
    ;;
#
# REVRES|REFORMAT+REVRES	adjust vertical resolution (should optimize)
# *RESIZE			page size must be adjusted (should optimize)
# *REIMAGE			maybe should reject (XXX)
#
*REVRES|*RESIZE|*REIMAGE)
    ($TIFF2PS -a $fil | $PS2FAX -o $out $opt) || {
	$CAT<<EOF
Unexpected failure converting TIFF document; the command

    $TIFF2PS -a $fil | $PS2FAX $opt

failed with exit status $?.  This conversion was done because

EOF
	echo "$RESULT" | $SED 1d; exit 254
    }
    exit 0
    ;;
*)				# something went wrong
    echo "Unexpected failure in the TIFF format checker;"
    echo "the output of $CHECK was:"
    echo ""
    echo "$RESULT"
    echo ""
    exit 254			# no formatter
    ;;
esac
