#! /bin/sh
#	$Header: /usr/people/sam/fax/util/RCS/xferstats.sh,v 1.10 1994/06/14 23:01:47 sam Exp $
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

#
# Print Statistics about Transmitted Facsimile.
#
SPOOL=/var/spool/fax
AWK=nawk

PATH=/bin:/usr/bin:/etc
test -d /usr/ucb  && PATH=$PATH:/usr/ucb		# Sun and others
test -d /usr/bsd  && PATH=$PATH:/usr/bsd		# Silicon Graphics
test -d /usr/5bin && PATH=/usr/5bin:$PATH:/usr/etc	# Sun and others
test -d /usr/sbin && PATH=/usr/sbin:$PATH		# 4.4BSD-derived

# look for an an awk: nawk, gawk, awk
($AWK '{}' </dev/null >/dev/null) 2>/dev/null ||
    { AWK=gawk; ($AWK '{}' </dev/null >/dev/null) 2>/dev/null || AWK=awk; }

FILES=
SORTKEY=-sender
MAPNAMES=yes

while [ x"$1" != x"" ] ; do
    case $1 in
    -send*|-csi|-dest*|-speed|-rate|-format)
	    SORTKEY=$1;;
    -nomap) MAPNAMES=no;;
    -*)	    echo "Usage: $0 [-sortkey] [-nomap] [files]"; exit 1;;
    *)	    FILES="$FILES $1";;
    esac
    shift
done
if [ -z "$FILES" ]; then
    FILES=$SPOOL/etc/xferlog
fi

#
# Construct awk rules to collect information according
# to the desired sort key.  There are three rules for
# each; to deal with the three different formats that
# have existed over time.
#
case $SORTKEY in
-send*)
    AWKRULES='$2 == "SEND" && NF == 9  { acct($3, $4, $7, $8, $5, $6, $9); }
     	      $2 == "SEND" && NF == 11 { acct($4, $5, $9, $10, $7, $8, $11); }
     	      $2 == "SEND" && NF == 12 { acct($5, $6, $10, $11, $8, $9, $12); }'
    ;;
-csi)
    AWKRULES='$2 == "SEND" && NF == 9  { acct($4, $4, $7, $8, $5, $6, $9); }
    	      $2 == "SEND" && NF == 11 { acct($6, $5, $9, $10, $7, $8, $11); }
    	      $2 == "SEND" && NF == 12 { acct($7, $6, $10, $11, $8, $9, $12); }'
    MAPNAMES=no
    ;;
-dest*)
    AWKRULES='$2 == "SEND" && NF == 9  { acct($4, $4, $7, $8, $5, $6, $9); }
    	      $2 == "SEND" && NF == 11 { acct($5, $5, $9, $10, $7, $8, $11); }
    	      $2 == "SEND" && NF == 12 { acct($6, $6, $10, $11, $8, $9, $12); }'
    MAPNAMES=no
    ;;
-speed|-rate)
    AWKRULES='$2 == "SEND" && NF == 9  { acct($5, $4, $7, $8, $5, $6, $9); }
    	      $2 == "SEND" && NF == 11 { acct($7, $5, $9, $10, $7, $8, $11); }
    	      $2 == "SEND" && NF == 12 { acct($8, $6, $10, $11, $8, $9, $12); }'
    MAPNAMES=no
    ;;
-format)
    AWKRULES='$2 == "SEND" && NF == 9  { acct($6, $4, $7, $8, $5, $6, $9); }
    	      $2 == "SEND" && NF == 11 { acct($8, $5, $9, $10, $7, $8, $11); }
    	      $2 == "SEND" && NF == 12 { acct($9, $6, $10, $11, $8, $9, $12); }'
    MAPNAMES=no
    ;;
esac

#
# Generate an awk program to process the statistics file.
#
tmpAwk=/tmp/xfer$$
trap "rm -f $tmpAwk; exit 1" 0 1 2 15

(cat<<'EOF'
#
# Convert hh:mm:ss to seconds.
#
func cvtTime(s)
{
    t = i = 0;
    for (n = split(s, a, ":"); i++ < n; )
	t = t*60 + a[i];
    return t;
}

func setupDigits()
{
  digits[0] = "0"; digits[1] = "1"; digits[2] = "2";
  digits[3] = "3"; digits[4] = "4"; digits[5] = "5";
  digits[6] = "6"; digits[7] = "7"; digits[8] = "8";
  digits[9] = "9";
}

#
# Format seconds as hh:mm:ss.
#
func fmtTime(t)
{
    v = int(t/3600);
    result = "";
    if (v > 0) {
	if (v >= 10)
	    result = digits[int(v/10)];
	result = result digits[int(v%10)] ":";
	t -= v*3600;
    }
    v = int(t/60);
    if (v >= 10 || result != "")
	result = result digits[int(v/10)];
    result = result digits[int(v%10)];
    t -= v*60;
    return (result ":" digits[int(t/10)] digits[int(t%10)]);
}

#
# Setup a map for histogram calculations.
#
func setupMap(s, map)
{
    n = split(s, a, ":");
    for (i = 1; i <= n; i++)
	map[a[i]] = i;
}

#
# Add pages to a histogram.
#
func addToMap(key, ix, pages, map)
{
    if (key == "") {
	for (i in map)
	    key = key ":";
    }
    n = split(key, a, ":");
    a[map[ix]] += pages;
    t = a[1];
    for (i = 2; i <= n; i++)
      t = t ":" a[i];
    return t;
}

#
# Merge two histogram maps.
#
func mergeMap(map2, map1)
{
    if (map2 == "")
	return map1;
    else if (map1 == "")
	return map2;
    # map1 & map2 are populated
    n1 = split(map1, a1, ":");
    n2 = split(map2, a2, ":");
    for (i = 1; i <= n1; i++)
	a2[i] += a1[i];
    t = a2[1];
    for (i = 2; i <= n; i++)
      t = t ":" a2[i];
    return t;
}

#
# Return the name of the item with the
# largest number of accumulated pages.
#
func bestInMap(totals, map)
{
   n = split(totals, a, ":");
   imax = 1; max = -1;
   for (j = 1; j <= n; j++)
       if (a[j] > max) {
	   max = a[j];
	   imax = j;
       }
   split(map, a, ":");
   return a[imax];
}

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
func acct(key, dest, pages, time, br, df, status)
{
    status = cleanup(status);
    if (length(status) > 11) {
	msg = toLower(substr(status, 1, 11));
	if (callFailed[msg])
	    return;
    }
    key = cleanup(key);
EOF
test "$MAPNAMES" = "yes" && cat<<'EOF'
    #
    # Try to merge unqualified names w/ fully qualified names
    # by stripping the host part of the domain address and
    # mapping unqualified names to a stripped qualified name.
    #
    n = split(key, parts, "@");
    if (n == 2) {			# user@addr
	user = parts[1];
	if (user != "root" && user != "guest") {
	    addr = parts[2];
	    #
	    # Strip hostname from multi-part domain name.
	    #
	    n = split(addr, domains, ".");
	    if (n > 1) {		# e.g. flake.asd.sgi.com
		l = length(domains[1])+1;
		addr = substr(addr, l+1, length(addr)-l);
		key = user "@" addr;
	    }
	    if (addrs[user] == "") {	# record mapped name
		addrs[user] = addr;
	    } else if (addrs[user] != addr) {
		if (!warned[user "@" addr]) {
		    warned[user "@" addr] = 1;
		    printf "Warning, address clash, \"%s\" and \"%s\".\n", \
		       user "@" addrs[user], user "@" addr
		}
	    }
	}
    } else if (n != 1) {
	printf "Warning, weird user address/name \"%s\".\n", key
    }
EOF
cat<<'EOF'
    dest = cleanup(dest);
    sendpages[key] += pages;
    time = cleanup(time);
    if (pages == 0 && time > 60)
	time = 0;
    sendtime[key] += cvtTime(time);
    if (status != "")
	senderrs[key]++;
    br = cleanup(br);
    sendrate[key] = addToMap(sendrate[key], br, pages, rateMap);
    df = cleanup(df);
    senddata[key] = addToMap(senddata[key], df, pages, dataMap);
}

#
# Print a rule between the stats and the totals line.
#
func printRule(n, s)
{
    r = "";
    while (n-- >= 0)
	r = r s;
    printf "%s\n", r;
}

BEGIN		{ FS="\t";
		  rates = "2400:4800:7200:9600:12000:14400";
		  setupMap(rates, rateMap);
		  datas = "1-D MR:2-D MR:2-D Uncompressed Mode:2-D MMR";
		  setupMap(datas, dataMap);
		  callFailed["busy signal"] = 1;
		  callFailed["unknown pro"] = 1;
		  callFailed["no carrier "] = 1;
		  callFailed["no local di"] = 1;
		  callFailed["no answer f"] = 1;
		  setupToLower();
		}
END		{ OFS="\t"; setupDigits();
		  maxlen = 15;
		  # merge unqualified and qualified names
		  for (key in sendpages) {
		      if (addrs[key] != "") {
			  fullkey = key "@" addrs[key];
			  sendpages[fullkey] += sendpages[key];
			  sendtime[fullkey] += sendtime[key];
			  senderrs[fullkey] += senderrs[key];
			  sendrate[fullkey] = \
			      mergeMap(sendrate[fullkey], sendrate[key]);
			  senddata[fullkey] = \
			      mergeMap(senddata[fullkey], senddata[key]);
		      }
		  }
		  nsorted = 0;
		  for (key in sendpages) {
		      if (addrs[key] != "")	# unqualified name
			  continue;
		      l = length(key);
		      if (l > maxlen)
			maxlen = l;
		      sorted[nsorted++] = key;
		  }
		  qsort(sorted, 0, nsorted-1);
		  fmt = "%-" maxlen "." maxlen "s";	# e.g. %-24.24s
		  printf fmt " %5s %8s %6s %4s %7s %7s\n",
		      "Destination", "Pages", "Time", "Pg/min",
		      "Errs", "TypRate", "TypData";
		  tpages = 0;
		  ttime = 0;
		  terrs = 0;
		  for (k = 0; k < nsorted; k++) {
		      i = sorted[k];
		      t = sendtime[i]/60; if (t == 0) t = 1;
		      n = sendpages[i]; if (n == 0) n = 1;
		      brate = best
		      printf fmt " %5d %8s %6.1f %4d %7d %7.7s\n",
			  i, sendpages[i], fmtTime(sendtime[i]),
			  sendpages[i] / t, senderrs[i],
			  bestInMap(sendrate[i], rates),
			  bestInMap(senddata[i], datas);
			tpages += sendpages[i];
			ttime += sendtime[i];
			terrs += senderrs[i];
		  }
		  printRule(maxlen+1+5+1+8+6+1+4+1+7+1+7, "-");
		  t = ttime/60; if (t == 0) t = 1;
		  printf fmt " %5d %8s %6.1f %4d\n",
		      "Total", tpages, fmtTime(ttime), tpages/t, terrs;
		}
EOF
echo "$AWKRULES"
)>$tmpAwk
$AWK -f $tmpAwk $FILES
