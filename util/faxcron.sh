#! /bin/sh
#	$Header: /usr/people/sam/fax/util/RCS/faxcron.sh,v 1.11 1994/06/17 19:08:55 sam Exp $
#
# FlexFAX Facsimile Software
#
# Copyright (c) 1993, 1994 Sam Leffler
# Copyright (c) 1993, 1994 Silicon Graphics, Inc.
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
# Script to run periodically from cron:
#
# 1. Purge info directory of old remote machine capabilities.
# 2. Truncate log files to delete old information.
# 3. Purge old files in the received facsimile queue.
# 4. Notify about sites that are currently having jobs rejected.
#
SPOOL=/var/spool/fax
JUNK=/tmp/faxcron$$
XFERLOG=$SPOOL/etc/xferlog
LAST=$SPOOL/etc/lastrun

AGEINFO=30			# purge remote info after 30 days inactivity
AGELOG=30			# keep log info for last 30 days
FAXUSER=fax			# owner of log files
LOGMODE=0644			# mode for log files

PATH=/bin:/usr/bin:/etc
test -d /usr/ucb  && PATH=$PATH:/usr/ucb		# Sun and others
test -d /usr/bsd  && PATH=$PATH:/usr/bsd		# Silicon Graphics
test -d /usr/5bin && PATH=/usr/5bin:$PATH:/usr/etc	# Sun and others
test -d /usr/sbin && PATH=/usr/sbin:$PATH		# 4.4BSD-derived

AWK=nawk
# look for an an awk: nawk, gawk, awk
($AWK '{}' </dev/null >/dev/null) 2>/dev/null ||
    { AWK=gawk; ($AWK '{}' </dev/null >/dev/null) 2>/dev/null || AWK=awk; }

RM="rm -f"
TEE=tee
CP=cp
MV=mv
CHOWN=chown
CHMOD=chmod
UPDATE="date +'%D %R' >$LAST"

while [ x"$1" != x"" ] ; do
    case $1 in
    -n)	    RM=true; TEE=true; CP=true; MV=true;
	    CHOWN=true; CHMOD=true
	    UPDATE=true
	    ;;
    -*)	    echo "Usage: $0 [-n]"; exit 1;;
    esac
    shift
done

trap "rm -f \$JUNK; exit 1" 0 1 2 15

cd $SPOOL

echo "Report failed calls and associated session logs:"
LASTRUN=`cat $LAST 2>/dev/null`
$AWK '
#
# Sort array a[l..r]
#
function qsort(a, l, r) {
    i = l;
    k = r+1;
    item = a[l];
    for (;;) {
	while (i < r) {
            i++;
	    if (a[i] >= item)
		break;
        }
	while (k > l) {
            k--;
	    if (a[k] <= item)
		break;
        }
        if (i >= k)
	    break;
	t = a[i]; a[i] = a[k]; a[k] = t;
    }
    t = a[l]; a[l] = a[k]; a[k] = t;
    if (k != 0 && l < k-1)
	qsort(a, l, k-1);
    if (k+1 < r)
	qsort(a, k+1, r);
}

func cleanup(s)
{
    gsub("\"", "", s);
    gsub("^ +", "", s);
    gsub(" +$", "", s);
    return s;
}

func setupToLower()
{
    upperRE = "[ABCDEFGHIJKLMNOPQRSTUVWXYZ]";
    upper["A"] = "a"; upper["B"] = "b"; upper["C"] = "c";
    upper["D"] = "d"; upper["E"] = "e"; upper["F"] = "f";
    upper["G"] = "g"; upper["H"] = "h"; upper["I"] = "i";
    upper["J"] = "j"; upper["K"] = "k"; upper["L"] = "l";
    upper["M"] = "m"; upper["N"] = "n"; upper["O"] = "o";
    upper["P"] = "p"; upper["Q"] = "q"; upper["R"] = "r";
    upper["S"] = "s"; upper["T"] = "t"; upper["U"] = "u";
    upper["V"] = "v"; upper["W"] = "w"; upper["X"] = "x";
    upper["Y"] = "y"; upper["Z"] = "z";
}

func toLower(s)
{
    if (match(s, upperRE) != 0) {
	do {
	    c = substr(s, RSTART, 1);
	    gsub(c, upper[c], s);
	} while (match(s, upperRE));
    }
    return s;
}

#
# Accumulate a statistics record.
#
func acct(dest, status, datetime)
{
    split(datetime, a, " ");
    split(a[1], b, "/");
    t = b[3] b[1] b[2] a[2];
    if (t < LASTt)
	return;
    status = cleanup(status);
    if (length(status) > 11) {
	msg = toLower(substr(status, 1, 11));
	if (callFailed[msg])
	    return;
	if (skipError[msg])
	    return;
    }
    if (status != "") {
	dest = cleanup(dest);
	datetime = cleanup(datetime);
	for (i = 0; i < nerrmsg; i++)
	    if (errmsg[i] == status)
		break;
	if (i == nerrmsg)
	    errmsg[nerrmsg++] = status;
	if (errinfo[dest] == "")
	    errinfo[dest] = datetime "@" i;
	else
	    errinfo[dest] = errinfo[dest] "|" datetime "@" i;
    }
}

func printTranscript(canon, datetime)
{
    gsub("[^0-9]", "", canon);
    split(datetime, parts, " ");
    split(parts[1], p, "/");
    cmd = sprintf(TRANSCRIPT, canon, months[p[1]], p[2], parts[2]);
    system(cmd);
}

BEGIN		{ FS="\t";
		  callFailed["busy signal"] = 1;
		  callFailed["unknown pro"] = 1;
		  callFailed["no carrier "] = 1;
		  callFailed["no local di"] = 1;
		  callFailed["no answer f"] = 1;
		   skipError["job aborted"] = 1;
		   skipError["invalid dia"] = 1;
		   skipError["can not loc"] = 1;
		  months["01"] = "Jan"; months["02"] = "Feb";
		  months["03"] = "Mar"; months["04"] = "Apr";
		  months["05"] = "May"; months["06"] = "Jun";
		  months["07"] = "Jul"; months["08"] = "Aug";
		  months["09"] = "Sep"; months["10"] = "Oct";
		  months["11"] = "Nov"; months["12"] = "Dec";

		  split(LASTRUN, a, " ");
		  split(a[1], b, "/");
		  LASTt = b[3] b[1] b[2] a[2];
		  setupToLower();
		}
$2 == "SEND" && NF == 9  { acct($4, $9, $1); }
$2 == "SEND" && NF == 11 { acct($5, $11, $1); }
END		{ nsorted = 0;
		  for (key in errinfo)
		      sorted[nsorted++] = key;
		  qsort(sorted, 0, nsorted-1);
		  for (k = 0; k < nsorted; k++) {
		      key = sorted[k];
		      n = split(errinfo[key], a, "|");
		      for (i = 1; i <= n; i++) {
			  m = split(a[i], b, "@");
			  if (m != 2)
			      continue;
			  printf "\n"
			  printf "To: %-16.16s  Date: %s\n", key, b[1]
			  printf "Error: %s\n\n", errmsg[b[2]]

			  printTranscript(key, b[1]);
		      }
		  }
		}
' LASTRUN="$LASTRUN" TRANSCRIPT="\
    LOGFILE=$SPOOL/log/%s;\
    TMP=/tmp/faxlog\$\$;\
    if [ -f \$LOGFILE ]; then\
	sed -n -e '/%s %s %s.*SESSION BEGIN/,/SESSION END/p' \$LOGFILE |\
	sed -e '/start.*timer/d'\
	    -e '/stop.*timer/d'\
	    -e '/-- data/d'\
	    -e 's/^/    /' >\$TMP;\
    fi;\
    if [ -s \$TMP ]; then\
	cat \$TMP;\
    else\
	echo '    No transcript available.';\
    fi;\
    rm -f \$TMP\
    " $XFERLOG
$RM $LAST; eval $UPDATE
echo ""

#
# Collect phone numbers that haven't been called
# in the last $AGE days.  We use this to clean up
# the info and log files.
#
find info -ctime +$AGEINFO -print >$JUNK

echo "Purge cache of fax machine capabilities:"
for i in `cat $JUNK`; do
    echo "    $i"
    $RM $i
done
echo ""

echo "Truncate old session logs:"
TODAY="`date +'%h %d %T'`"
for i in log/*; do
    START=`$AWK -F: '
#
# Setup data conversion data structures.
#
func setupDateTimeStuff()
{
    Months["Jan"] =  0; Months["Feb"] =  1; Months["Mar"] =  2;
    Months["Apr"] =  3; Months["May"] =  4; Months["Jun"] =  5;
    Months["Jul"] =  6; Months["Aug"] =  7; Months["Sep"] =  8;
    Months["Oct"] =  9; Months["Nov"] = 10; Months["Dec"] = 11;

    daysInMonth[ 0] = 31; daysInMonth[ 1] = 28; daysInMonth[ 2] = 31;
    daysInMonth[ 3] = 30; daysInMonth[ 4] = 31; daysInMonth[ 5] = 30;
    daysInMonth[ 6] = 31; daysInMonth[ 7] = 31; daysInMonth[ 8] = 30;
    daysInMonth[ 9] = 31; daysInMonth[10] = 30; daysInMonth[11] = 31;

    FULLDAY = 24 * 60 * 60;
}

#
# Convert MMM DD hh:mm:ss.ms to seconds.
# NB: this does not deal with leap years.
#
func cvtTime(s)
{
    mon = Months[substr(s, 0, 3)];
    yday = substr(s, 5, 2) - 1;
    for (i = 0; i < mon; i++)
	yday += daysInMonth[i];
    s = substr(s, 7);
    t = i = 0;
    for (n = split(s, a, ":"); i++ < n; )
	t = t*60 + a[i];
    return yday*FULLDAY + t;
}

BEGIN			{ setupDateTimeStuff();
			  KEEP = cvtTime(TODAY) - AGE*FULLDAY;
			  lastRecord = "$"
			}
			{ if (cvtTime($1 ":" $2 ":" $3) >= KEEP) {
			      lastRecord = NR; exit
			  }
			}
END			{ print lastRecord }
' TODAY="$TODAY" AGE=$AGELOG $i` 2>/dev/null
    if [ "$START" != 1 ]; then
	sed 1,${START}d $i >$JUNK
	if [ -s $JUNK ]; then
	    $MV $JUNK $i; $CHOWN ${FAXUSER} $i; $CHMOD ${LOGMODE} $i
	    ls -ls $i
	else
	    echo "     Remove empty $i"
	    $RM $i
	fi
    fi
done
echo ""

#
# Purge old stuff from the receive queue.
#
find recvq -mtime +7 -print >$JUNK

echo "Purge old stuff in receive queue:"
if [ -s $JUNK ]; then
    (for i in `cat $JUNK`; do
	bin/faxinfo $i
	$RM $i >/dev/null 2>&1
    done) | $AWK -F: '
/recvq.*/	{ file=$1; }
/Sender/	{ sender = $2; }
/Pages/		{ pages = $2; }
/Quality/	{ quality = $2; }
/Received/	{ date = $2;
		  for (i = 3; i <= NF; i++)
		      date = date ":" $i;
		  printf "    %-16.16s %21.21s %2d %8s%s\n", \
			file, sender, pages, quality, date;
		}
'
fi
echo ""

#
# Note destinations whose jobs are currently being rejected.
#
grep "^rejectNotice:" info/* cinfo/* 2>/dev/null | $AWK -F: '
		{ reason = $3;
		  for (i = 4; i <= NF; i++)
			reason = reason ":" $i;
		  sub("^[ ]*", "", reason);
		  if (reason != "") {
		      sub(".*/", "", $1);
		      printf "Rejecting jobs to +%s because \"%s\".\n", \
			    $1, reason;
		  }
		}
'
