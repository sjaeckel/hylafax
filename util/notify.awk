#! /bin/sh
#	$Header: /usr/people/sam/fax/./util/RCS/notify.awk,v 1.20 1995/04/08 21:44:56 sam Rel $
#
# HylaFAX Facsimile Software
#
# Copyright (c) 1990-1995 Sam Leffler
# Copyright (c) 1991-1995 Silicon Graphics, Inc.
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
# Awk support program for notify shell script.  This
# stuff is broken out into a separate file to avoid
# overflowing the exec arg list on some systems like SCO.
#

func printItem(fmt, tag, value)
{
    printf "%14s: " fmt "\n", tag, value;
}

#
# Construct a return-to-sender message.
#
func returnToSender()
{
    printf "\n    ---- Unsent job status ----\n\n"
    printItem("%s", "Destination", number);
    printItem("%s", "Sender", sender);
    printItem("%s", "Mailaddr", mailaddr);
    if (modem != "any")
	printItem("%s", "Modem", modem);
    printItem("%s", "Submitted From", client);
    if (jobType == "facsimile") {
	printItem("%u (mm)", "Page Width", pagewidth);
	printItem("%.0f (mm)", "Page Length", pagelength);
	printItem("%.0f (lpi)", "Resolution", resolution);
    }
    printItem("%s", "Status", status);
    printItem("%u (exchanges with remote device)", "Dialogs", tottries);
    printItem("%u (consecutive failed calls to destination)", "Dials", ndials);
    printItem("%u (total phone calls placed)", "Calls", totdials);
    if (jobType == "facsimile") {
	printItem("%u (attempts to send current page)", "Attempts", ntries);
	printItem("%u (directory of next page to send)", "Dirnum", dirnum);
	if (nfiles > 0) {
	    printf "\n    ---- Unsent files submitted for transmission ----\n\n";
	    printf "This archive was created with %s, %s, and %s.\n",
		tar, compressor, encoder;
	    printf "The original files can be recovered by applying the following commands:\n";
	    printf "\n";
	    printf "    %s			# generates rts.%s.Z file\n", decoder, tar;
	    printf "    %s rts.%s.Z  | %s xvf -	# generates separate files\n",
		decompressor, tar, tar;
	    printf "\n";
	    printf "and the job can be resubmitted using the following command:\n";
	    printf "\n";
	    printf "    sendfax -d %s" poll notify resopt "%s\n", number, files;
	    printf "\n";

	    system("cd docq;" tar " cf - " files \
			    " | " compressor \
			    " | " encoder " rts." tar ".Z");
	}
    } else if (jobType == "pager") {
	printf "\n    ---- Unsent pages submitted for transmission ----\n\n";
	for (i = 0; i < npins; i++)
	    printf "%14s\n",  "PIN " pins[i];
	if (nfiles != 0) {
	    printf "\n    ---- Message text ----\n\n";
	    while (getline <files)
		print $0;
	}    
    }
}

func returnTranscript(pid, canon)
{
    system(transcript " " pid " \"" canon "\" 2>&1");
}

func printStatus(s)
{
    if (s == "")
	print "<no reason recorded>";
    else
	print s
}

func putHeaders(subject)
{
    print "To: " mailaddr;
    print "Subject: " subject;
    print "";
    printf "Your " jobType " job to " number;
}

BEGIN		{ nfiles = 0;
		  npins = 0;
		  pagewidth = 0;
		  pagelength = 0;
		  resolution = 0;
		  jobType = "facsimile";
		  signalrate = "unknown";
		  dataformat = "unknown";
		}
/^number/	{ number = $2; }
/^external/	{ number = $2; }		# override unprocessed number
/^sender/	{ sender = $2; }
/^mailaddr/	{ mailaddr = $2; }
/^jobtag/	{ jobtag = $2; }
/^jobtype/	{ jobType = $2; }
/^status/	{ status = $0; sub("status:", "", status);
		  if (status ~ /\\$/) {
		      sub("\\\\$", "", status);
		      while (getline) {
			  status = status $0;
			  sub("\\\\$", "", status);
			  if ($0 !~ /\\$/)
			      break;
		      }
		  }
		}
/^resolution/	{ resolution = $2;
		  if (resolution == 196)
		      resopt = " -m";
		}
/^npages/	{ npages = $2; }
/^dirnum/	{ dirnum = $2; }
/^ntries/	{ ntries = $2; }
/^ndials/	{ ndials = $2; }
/^pagewidth/	{ pagewidth = $2; }
/^pagelength/	{ pagelength = $2; }
/^signalrate/	{ signalrate = $2; }
/^dataformat/	{ dataformat = $2; }
/^modem/	{ modem = $2; }
/^totdials/	{ totdials = $2; }
/^tottries/	{ tottries = $2; }
/^client/	{ client = $2; }
/^notify/	{ if ($2 == "when done")
		      notify = " -D";
		  else if ($2 == "when requeued")
		      notify = " -R";
		}
/^[!]*post/	{ if (NF == 2)
		      split($2, parts, "/");
		  else
		      split($3, parts, "/");
		  if (!match(parts[2], "\.cover")) {	# skip cover pages
		     files = files " " parts[2];
		     nfiles++;
		  }
		}
/^!tiff/	{ if (NF == 2)
		      split($2, parts, "/");
		  else
		      split($3, parts, "/");
		  files = files " " parts[2]; nfiles++;
		}
/^!page/	{ pins[npins++] = $3; }
/^data/		{ files = $3; nfiles++; }
/^poll/		{ poll = " -p"; }
END {
    if (jobtag == "") {
	jobtag = FILENAME;
	sub(".*/q", jobType " job ", jobtag);
    }
    if (why == "done") {
	putHeaders(jobtag " to " number " completed");
	print " was completed successfully.";
	print "";
	if (jobType == "facsimile") {
	    printItem("%u", "Pages", npages);
	    if (resolution == 196)
		printItem("%s", "Quality", "Fine");
	    else
		printItem("%s", "Quality", "Normal");
	    printItem("%u (mm)", "Page Width", pagewidth);
	    printItem("%.0f (mm)", "Page Length", pagelength);
	    printItem("%s", "Signal Rate", signalrate);
	    printItem("%s", "Data Format", dataformat);
	}
	if (tottries != 1)
	    printItem("%s (exchanges with remote device)", "Dialogs", tottries);
	if (totdials != 1)
	    printItem("%s (total phone calls placed)", "Calls", totdials);
	if (modem != "any")
	    printItem("%s", "Modem", modem);
	printItem("%s", "Submitted From", client);
	printf "\nTotal transmission time was " jobTime ".";
	if (status != "")
	    print "  Additional information:\n    " status;
    } else if (why == "failed") {
	putHeaders(jobtag " to " number " failed");
	printf " failed because:\n    ";
	printStatus(status);
	returnTranscript(pid, canon);
	returnToSender();
    } else if (why == "rejected") {
	putHeaders(jobtag " to " number " failed");
	printf " was rejected because:\n    ";
	printStatus(status);
	returnToSender();
    } else if (why == "blocked") {
	putHeaders(jobtag " to " number " blocked");
	printf " is delayed in the scheduling queues because:\n    ";
	printStatus(status);
	print "";
	print "The job will be processed as soon as possible."
    } else if (why == "requeued") {
	putHeaders(jobtag " to " number " requeued");
	printf " was not sent because:\n    ";
	printStatus(status);
	print "";
	print "The job will be retried at " nextTry "."
	returnTranscript(pid, canon);
    } else if (why == "removed" || why == "killed") {
	putHeaders(jobtag " to " number " removed from queue");
	print " was deleted from the queue.";
	if (why == "killed")
	    returnToSender();
    } else if (why == "timedout") {
	putHeaders(jobtag " to " number " failed");
	print " could not be sent after repeated attempts.";
	returnToSender();
    } else if (why == "format_failed") {
	putHeaders(jobtag " to " number " failed");
	print " was not sent because conversion"
	print "of PostScript to facsimile failed.  The output from \"" ps2fax "\" was:\n";
	print status "\n";
	printf "Check your job for non-standard fonts %s.\n",
	    "and invalid PostScript constructs";
	returnToSender();
    } else if (why == "no_formatter") {
	putHeaders(jobtag " to " number " failed");
	print " was not sent because";
	print "the conversion program \"" ps2fax "\" was not found.";
	returnToSender();
    } else if (match(why, "poll_*")) {
	putHeaders("Notice about " jobtag);
	printf ", a polling request,\ncould not be completed because ";
	if (why == "poll_rejected")
	    print "the remote side rejected your request.";
	else if (why == "poll_no_document")
	    print "no document was available for retrieval.";
	else if (why == "poll_failed")
	    print "an unspecified problem occurred.";
	print "";
	printf "Total connect time was %s.\n", jobTime;
    } else {
	putHeaders("Notice about " jobtag);
	print " had something happen to it."
	print "Unfortunately, the notification script was invoked",
	    "with an unknown reason"
	print "so the rest of this message is for debugging:\n";
	print "why: " why;
	print "jobTime: " jobTime;
	print "pid: " pid;
	print "canon: " canon;
	print "nextTry: " nextTry;
	print  "";
	print "This should not happen, please report it to your administrator.";
	returnToSender();
    }
}
