#! /bin/sh
#	$Header: /usr/people/sam/fax/util/RCS/mkcover.sh,v 1.5 1994/07/04 05:06:49 sam Exp $
#
# FlexFAX Facsimile Software
#
# Copyright (c) 1994 Sam Leffler
# Copyright (c) 1994 Silicon Graphics, Inc.
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
# mkcover qfile cover
#
# Generate a PostScript continuation cover page for the specified job.
#
PATH=/bin:/usr/bin:
test -d /usr/ucb  && PATH=$PATH:/usr/ucb		# Sun and others
test -d /usr/bsd  && PATH=$PATH:/usr/bsd		# Silicon Graphics
test -d /usr/5bin && PATH=/usr/5bin:$PATH:/usr/etc	# Sun and others
test -d /usr/sbin && PATH=/usr/sbin:$PATH		# 4.4BSD-derived

if [ $# != 3 ]; then
    echo "Usage: $0 qfile template cover.ps"
    exit 1
fi
qfile=$1
TEMPLATE=${2:?'No cover sheet template file specified.'}
cover=$3

test -f $TEMPLATE || { echo "No cover sheet template $TEMPLATE."; exit 1; }

AWK=nawk
# look for an an awk: nawk, gawk, awk
($AWK '{}' </dev/null >/dev/null) 2>/dev/null ||
    { AWK=gawk; ($AWK '{}' </dev/null >/dev/null) 2>/dev/null || AWK=awk; }

DATE=`date +"%a %b %d %Y, %H:%M %Z"`
(cat <<'EOF'
%!PS-Adobe-3.0
%%Creator: mkcover
%%Title: FlexFAX Continuation Cover Sheet
%%Pages: 1 +1
%%EndComments
%%BeginProlog
/$coverdict 100 dict def $coverdict begin
EOF

$AWK -F: '
func emitDef(d, v)
{
    gsub("[()]", "\\&", v);
    printf "/%s (%s) def\n", d, v;
}

BEGIN		{ emitDef("todays-date", DATE);
		  jobid = FILENAME;
		  sub("^[^0-9]*", "", jobid);
		}
/^number/	{ number = $2; }
/^external/	{ number = $2; }		# override unprocessed number
/^sender/	{ emitDef("from", $2); }
/^mailaddr/	{ emitDef("mailaddr", $2); }
/^status/	{ comments = "This is the continuation of job#" jobid \
		     " which failed previously because:\n    " $2;
		  if (comments ~ /\\$/) {
		      sub("\\\\$", "", comments);
		      while (getline) {
			  comments = comments $0;
			  sub("\\\\$", "", comments);
			  if ($0 !~ /\\$/)
			      break;
		      }
		  }
		  emitDef("comments", comments);
		}
/^npages/	{ emitDef("page-count", $2); }
/^ntries/	{ emitDef("ntries", $2); }
/^ndials/	{ emitDef("ndials", $2); }
/^pagewidth/	{ printf "/pageWidth %s def\n", $2; }
/^pagelength/	{ printf "/pageLength %s def\n", $2; }
/^receiver/	{ emitDef("to", $2); }
/^location/	{ emitDef("to-location", $2); }
/^company/	{ emitDef("to-company", $2); }
END		{ emitDef("to-fax-number", number); }
' DATE="$DATE" $qfile || {
    echo "Problem processing queue file $qfile."
    exit 1
}

cat <<'EOF'
end
%%EndProlog
%%Page: "1" 1
$coverdict begin
EOF
case $TEMPLATE in
*.Z)	zcat $TEMPLATE;;
*.gz)	zcat $TEMPLATE;;
*.p)	pcat $TEMPLATE;;
*)	cat $TEMPLATE;;
esac
cat <<'EOF'
end
%%Trailer
%%EOF
EOF
) > $cover
