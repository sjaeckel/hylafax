#! /bin/sh
#	$Header: /usr/people/sam/fax/etc/RCS/faxaddmodem.sh,v 1.122 1994/09/17 17:05:04 sam Exp $
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
# faxaddmodem [tty]
#
# This script interactively configures a FlexFAX server
# from keyboard input on a standard terminal.  There may
# be some system dependencies in here; hard to say with
# this mountain of shell code!
#
PATH=/bin:/usr/bin:/etc
test -d /usr/ucb  && PATH=$PATH:/usr/ucb		# Sun and others
test -d /usr/bsd  && PATH=$PATH:/usr/bsd		# Silicon Graphics
test -d /usr/5bin && PATH=/usr/5bin:$PATH:/usr/etc	# Sun and others
test -d /usr/sbin && PATH=/usr/sbin:$PATH		# 4.4BSD-derived

OS=`uname -s 2>/dev/null || echo unknown`		# system identification
case "$OS" in
# believable values...
IRIX|*BSD*|*bsd*|Linux|ULTRIX|HP-UX) ;;
*)  if [ -d /etc/saf ]; then
	# uname -s is unreliable on svr4 as it can return the nodename
	OS=svr4
    fi
    ;;
esac
SPEEDS="76800 57600 38400 19200 9600 4800 2400 1200"	# set of speeds to try
SPEED=

while [ x"$1" != x"" ] ; do
    case $1 in
    -os)    OS=$2; shift;;
    -s)	    SPEED=$2; shift;;
    -*)	    echo "Usage: $0 [-os OS] [-s SPEED] [ttyname]"; exit 1;;
    *)	    TTY=$1;;
    esac
    shift
done

#
# Deduce the effective user id:
#   1. POSIX-style, the id program
#   2. the old whoami program
#   3. last gasp, check if we have write permission on /dev
#
euid=`id|sed -e 's/.*uid=[0-9]*(\([^)]*\)).*/\1/'`
test -z "$euid" && euid=`whoami 2>/dev/null`
test -z "$euid" -a -w /dev && euid=root
if [ "$euid" != "root" ]; then
    echo "Sorry, but you must run this script as the super-user!"
    exit 1
fi

SPOOL=/var/spool/fax		# top of fax spooling tree
CPATH=$SPOOL/etc/config		# prefix of configuration file
LOCKDIR=/var/spool/locks	# UUCP locking directory
OUT=/tmp/addmodem$$		# temp file in which modem output is recorded
SVR4UULCKN=$SPOOL/bin/lockname	# SVR4 UUCP lock name construction program
ONDELAY=$SPOOL/bin/ondelay	# prgm to open devices blocking on carrier
CAT="cat -u"			# something to do unbuffered reads and writes
FAX=fax				# identity of the fax user
SERVICES=/etc/services		# location of services database
INETDCONF=/usr/etc/inetd.conf	# default location of inetd configuration file
ALIASES=/usr/lib/aliases	# default location of mail aliases database file
SGIPASSWD=/etc/passwd.sgi	# for hiding fax user from pandor on SGI's
PASSWD=/etc/passwd		# where to go for password entries
PROTOUID=uucp			# user who's uid we use for FAX user
defPROTOUID=3			# use this uid if PROTOUID doesn't exist
GROUP=/etc/group		# where to go for group entries
PROTOGID=nuucp			# group who's gid we use for FAX user
defPROTOGID=10			# use this gid if PROTOGID doesn't exist
SERVERDIR=/usr/etc		# directory where servers are located
QUIT=/usr/local/bin/faxquit	# cmd to terminate server
MODEMCONFIG=$SPOOL/config	# location of prototype modem config files

#
# Deal with known alternate locations for system files.
#
PickFile()
{
    for i do
	test -f $i && { echo $i; return; }
    done
    echo $1
}
INETDCONF=`PickFile	$INETDCONF /etc/inetd.conf /etc/inet/inetd.conf`
ALIASES=`PickFile	$ALIASES   /etc/aliases`
SERVICES=`PickFile	$SERVICES  /etc/inet/services`
test -f /etc/master.passwd			&& PASSWD=/etc/master.passwd

#
# Setup the password file manipulation functions according
# to whether we have System-V style support through the
# passmgmt program, or BSD style support through the chpass
# program.  If neither are found, we setup functions that
# will cause us to abort if we need to munge the password file.
#
if [ -f /bin/passmgmt -o -f /usr/sbin/passmgmt ]; then
    addPasswd()
    {
	passmgmt -o -a -c 'Facsimile Agent' -h $4 -u $2 -g $3 $1
    }
    deletePasswd()
    {
	passmgmt -d $1
    }
    modifyPasswd()
    {
	passmgmt -m -h $4 -u $2 -o -g $3 $1
    }
    lockPasswd()
    {
	passwd -l $1
    }
elif [ -f /usr/bin/chpass ]; then
    addPasswd()
    {
	chpass -a "$1:*:$2:$3::0:0:Facsimile Agent:$4:"
    }
    modifyPasswd()
    {
	chpass -a "$1:*:$2:$3::0:0:Facsimile Agent:$4:"
    }
    lockPasswd()
    {
	return 0				# entries are always locked
    }
elif [ "$OS" = "Linux" -a -f /usr/sbin/useradd ]; then
    addPasswd()
    {
	useradd -m -c 'Facsimile Agent' -d $4 -u $2 -o -g $3 $1
    }
    deletePasswd()
    {
	userdel -r $1
    }
    modifyPasswd()
    {
	usermod -m -d $4 -u $2 -o -g $3 $1
    }
    lockPasswd()
    {
	return 0
    }
elif [ -f /etc/useradd -o -f /usr/sbin/useradd ]; then
    addPasswd()
    {
	useradd -c 'Facsimile Agent' -d $4 -u $2 -o -g $3 $1
    }
    deletePasswd()
    {
	userdel $1
    }
    modifyPasswd()
    {
	usermod -m -d $4 -u $2 -o -g $3 $1
    }
    lockPasswd()
    {
	passwd -l $1
    }
else
    addPasswd()
    {
	echo "Help, I don't know how to add a passwd entry!"; exit 1
    }
    modifyPasswd()
    {
	echo "Help, I don't know how to modify a passwd entry!"; exit 1
    }
fi

#
# Figure out which brand of echo we have and define
# prompt and printf shell functions accordingly.
# Note that we assume that if the System V-style
# echo is not present, then the BSD printf program
# is available.
#
if [ `echo foo\\\c`@ = "foo@" ]; then
    # System V-style echo supports \r
    # and \c which is all that we need
    prompt()
    {
       echo "$* \\c"
    }
    printf()
    {
       echo "$*\\c"
    }
elif [ "`echo -n foo`@" = "foo@" ]; then
    # BSD-style echo; use echo -n to get
    # a line without the trailing newline
    prompt()
    {
       echo -n "$* "
    }
else
    # something else; do without
    prompt()
    {
	echo "$*"
    }
fi
t=`printf hello` 2>/dev/null
if [ "$t" != "hello" ]; then
    echo "You don't seem to have a System V-style echo command"
    echo "or a BSD-style printf command.  I'm bailing out..."
    exit 1
fi

#
# If the killall program is not present on the system
# cobble together a shell function to emulate the
# functionality that we need.
#
(killall -l >/dev/null) 2>/dev/null || {
    killall()
    {
	# NB: ps ax should give an error on System V, so we try it first!
	pid="`ps ax 2>/dev/null | grep $2 | grep -v grep | awk '{print $1;}'`"
	test "$pid" ||
	    pid="`ps -e | grep $2 | grep -v grep | awk '{print $2;}'`"
	test "$pid" && kill $1 $pid; return
    }
}

#
# Prompt the user for a string that can not be null.
#
promptForNonNullStringParameter()
{
    x=""
    while [ -z "$x" ]; do
	prompt "$2 [$1]?"; read x
	if [ "$x" ]; then
	    # strip leading and trailing white space
	    x=`echo "$x" | sed -e 's/^[ 	]*//' -e 's/[ 	]*$//'`
	else
	    x="$1"
	fi
    done
    param="$x"
}

#
# Prompt the user for a string that can be null.
#
promptForStringParameter()
{
    prompt "$2 [$1]?"; read x
    if [ "$x" ]; then
	# strip leading and trailing white space
	x=`echo "$x" | sed -e 's/^[ 	]*//' -e 's/[ 	]*$//'`
    else
	x="$1"
    fi
    param="$x"
}

#
# Prompt the user for a numeric value.
#
promptForNumericParameter()
{
    x=""
    while [ -z "$x" ]; do
	prompt "$2 [$1]?"; read x
	if [ "$x" ]; then
	    # strip leading and trailing white space
	    x=`echo "$x" | sed -e 's/^[ 	]*//' -e 's/[ 	]*$//'`
	    match=`expr "$x" : "\([0-9]*\)"`
	    if [ "$match" != "$x" ]; then
		echo ""
		echo "This must be entirely numeric; please correct it."
		echo ""
		x="";
	    fi
	else
	    x="$1"
	fi
    done
    param="$x"
}

#
# Prompt the user for a C-style numeric value.
#
promptForCStyleNumericParameter()
{
    x=""
    while [ -z "$x" ]; do
	prompt "$2 [$1]?"; read x
	if [ "$x" ]; then
	    # strip leading and trailing white space and C-style 0x prefix
	    x=`echo "$x" | sed -e 's/^[ 	]*//' -e 's/[ 	]*$//'`
	    match=`expr "$x" : "\([0-9]*\)" \| "$x" : "\(0x[0-9a-fA-F]*\)"`
	    if [ "$match" != "$x" ]; then
		echo ""
		echo "This must be entirely numeric; please correct it."
		echo ""
		x="";
	    fi
	else
	    x="$1"
	fi
    done
    param="$x"
}

#
# Prompt the user for a boolean value.
#
promptForBooleanParameter()
{
    x=""
    while [ -z "$x" ]; do
	prompt "$2 [$1]?"; read x
	if [ "$x" ]; then
	    # strip leading and trailing white space
	    x=`echo "$x" | sed -e 's/^[ 	]*//' -e 's/[ 	]*$//'`
	    case "$x" in
	    n|no|off)	x="no";;
	    y|yes|on)	x="yes";;
	    *)
cat <<EOF

"$x" is not a valid boolean parameter setting;
use one of: "yes", "on", "no", or "off".
EOF
		x="";;
	    esac
	else
	    x="$1"
	fi
    done
    param="$x"
}

#
# Start of system-related setup checking.  All this stuff
# should be done outside this script, but we check here
# so that it's sure to be done before starting up a fax
# server process.
# 
echo "Verifying that your system is setup properly for fax service..."

faxUID=`grep "^$PROTOUID:" $PASSWD | cut -d: -f3`
if [ -z "$faxUID" ]; then faxUID=$defPROTOUID; fi
faxGID=`grep "^$PROTOGID:" $GROUP | cut -d: -f3`
if [ -z "$faxGID" ]; then faxGID=$defPROTOGID; fi

#
# Change the password file entry for the fax user.
#
fixupFaxUser()
{
    emsg1=`modifyPasswd $FAX $faxUID $faxGID $SPOOL 2>&1`
    case $? in
    0)	echo "Done, the \"$FAX\" user should now have the right user id.";;
    *) cat <<-EOF
	Something went wrong; the command failed with:

	"$emsg1"

	The fax server will not work correctly until the proper uid
	is setup for it in the password file.  Please fix this problem
	and then rerun faxaddmodem.
	EOF
	exit 1
	;;
    esac
}

#
# Add a fax user to the password file and lock the
# entry so that noone can login as the user.
#
addFaxUser()
{
    emsg1=`addPasswd $FAX $faxUID $faxGID $SPOOL 2>&1`
    case $? in
    0)  emsg2=`lockPasswd $FAX 2>&1`
	case $? in
	0) echo "Added user \"$FAX\" to $PASSWD.";;
	*) emsg3=`deletePasswd $FAX 2>&1`
	   case $? in
	   0|9) cat <<-EOF
		Failed to add user "$FAX" to $PASSWD because the
		attempt to lock the password failed with:

		"$emsg2"

		Please fix this problem and rerun this script."
		EOF
		exit 1
		;;
	   *)   cat <<-EOF
		You will have to manually edit $PASSWD because
		after successfully adding the new user "$FAX", the
		attempt to lock its password failed with:

		"$emsg2"

		and the attempt to delete the insecure passwd entry failed with:

		"$emsg3"

		To close this security hole, you should add a password
		to the "$FAX" entry in the file $PASSWD, or lock this
		entry with an invalid password.
		EOF
		;;
	    esac
	    exit 1;;
	esac;;
    9)  # fax was already in $PASSWD, but not found with grep
	;;
    *)  cat <<-EOF
	There was a problem adding user "$FAX" to $PASSWD;
	the command failed with:

	"$emsg1"

	The fax server will not work until you have corrected this problem.
	EOF
	exit 1;;
    esac
}

x=`grep "^$FAX:" $PASSWD | cut -d: -f3`
if [ -z "$x" ]; then
    echo ""
    echo "You do not appear to have a "$FAX" user in the password file."
    prompt "The fax software needs this to work properly, add it [yes]?"
    read x
    if [ -z "$x" -o "$x" = "y" -o "$x" = "yes" ]; then
	addFaxUser
    fi
elif [ "$x" != "$faxUID" ]; then
    echo ""
    echo "It looks like you have a \"$FAX\" user in the password file,"
    echo "but with a uid different than the uid for uucp.  You probably"
    echo "have old fax software installed.  In order for this software"
    echo "to work properly, the fax user uid must be the same as uucp."
    prompt "Is it ok to change the password entry for \"$FAX\" [yes]?"
    read x
    if [ -z "$x" -o "$x" = "y" -o "$x" = "yes" ]; then
	fixupFaxUser
    fi
fi
#
# This should only have an effect on an SGI system...
# hide the fax user from pandora & co.
#
if [ "$OS" = "IRIX" -a -f $SGIPASSWD ]; then
    x=$FAX:noshow
    grep $x $SGIPASSWD >/dev/null 2>&1 ||
    (echo ""; echo "Adding fax user to \"$SGIPASSWD\"."; echo $x >>$SGIPASSWD)
fi

#
# Make sure a service entry is present.
#
hasYP=`ypcat services 2>/dev/null | tail -1` 2>/dev/null
x=
if [ "$hasYP" ]; then
    x=`ypcat services 2>/dev/null | egrep '^(flex)?fax[ 	]'` 2>/dev/null
    if [ -z "$x" ]; then
	echo ""
	echo "There does not appear to be an entry for the fax service in the YP database;"
	echo "the software will not work properly without one.  Contact your administrator"
	echo "to get the folllowing entry added:"
	echo ""
	echo "fax	4557/tcp		# FAX transmission service"
	echo ""
	exit 1
    fi
else
    x=`egrep '^(flex)?fax[ 	]' $SERVICES 2>/dev/null` 2>/dev/null
    if [ -z "$x" ]; then
	echo ""
	echo "There does not appear to be an entry for the fax service in the $SERVICES file;"
	prompt "should an entry be added to $SERVICES [yes]?"
	read x
	if [ "$x" = "" -o "$x" = "y" -o "$x" = "yes" ]; then
	    echo "fax		4557/tcp		# FAX transmission service" >>$SERVICES
	fi
    fi
fi

#
# Check that inetd is setup to provide service.
#
E="fax	stream	tcp	nowait	$FAX	$SERVERDIR/faxd.recv	faxd.recv"
test -f $INETDCONF && {
    egrep "^(flex)?fax[ 	]*stream[ 	]*tcp" $INETDCONF >/dev/null 2>&1 ||
    (echo ""
     echo "There is no entry for the fax service in \"$INETDCONF\";"
     prompt "should one be added [yes]?"
     read x
     if [ "$x" = "" -o "$x" = "y" -o "$x" = "yes" ]; then
	echo "$E" >>$INETDCONF;
	if killall -HUP inetd 2>/dev/null; then
	    echo "Poked inetd so that it re-reads the configuration file."
	else
	    echo "Beware, you may need to send a HUP signal to inetd."
	fi
     fi
    )
}

#
# Check for a FaxMaster entry for sending mail.
#
x=`ypcat -k aliases 2>/dev/null | grep -i '^faxmaster'` 2>/dev/null
if [ -z "$x" -a -f $ALIASES ]; then
    x=`grep -i '^faxmaster' $ALIASES`
fi
if [ -z "$x" ]; then
    echo ""
    echo "There does not appear to be an entry for the FaxMaster either in"
    echo "the yellow pages database or in the $ALIASES file;"
    prompt "should an entry be added to $ALIASES [yes]?"
    read x
    if [ "$x" = "" -o "$x" = "y" -o "$x" = "yes" ]; then
	promptForNonNullStringParameter "${USER:-root}" \
	   "Users to receive fax-related mail"
	(echo "# alias for notification messages from FlexFAX servers";
	 echo "FaxMaster: $param") >>$ALIASES
	if newaliases 2>/dev/null; then
	    echo "Rebuilt $ALIASES database."
	else
	    echo "Can not find newaliases to rebuild $ALIASES;"
	    echo "you will have to do it yourself."
	fi
    fi
fi


echo ""
echo "Done verifying system setup."
echo ""
# End of system-related setup checking

while [ -z "$TTY" -o ! -c /dev/$TTY ]; do
    if [ "$TTY" != "" ]; then
	echo "/dev/$TTY is not a terminal device."
    fi
    prompt "Serial port that modem is connected to [$TTY]?"; read TTY
done

JUNK="$OUT"
trap "rm -f \$JUNK; exit 1" 0 1 2 15

if [ ! -d $LOCKDIR ]; then
    prompt "Hmm, uucp lock files are not in \"$LOCKDIR\", where are they?"
    read x
    while [ ! -d $x ]; do
	prompt "Nope, \"$x\" is not a directory; try again:"
	read x
    done
    LOCKDIR=$x
fi

#
# Try to deduce if the tty devices are named in the SGI
# sense (ttyd<port>, ttym<port>, and ttyf<port>) or the
# way that everyone else seems to do it--tty<port>
#
# (I'm sure that someone will tell me there is another way as well.)
#
case "$OS" in
IRIX)
    PORT=`expr $TTY : 'tty.\(.*\)'`
    for x in f m d; do
	LOCKX="$LOCKX $LOCKDIR/LCK..tty$x${PORT}"
    done
    DEVS="/dev/ttyd${PORT} /dev/ttym${PORT} /dev/ttyf${PORT}"
    #
    # NB: we use ttyd* device names in the following
    # work so that we are not stopped by a need for DCD.
    #
    tdev=/dev/ttyd${PORT}
    #
    # No current SGI equipment supports rates >38400
    #
    SPEEDS="38400 19200 9600 4800 2400 1200"
    ;;
BSDi|BSD/386|386bsd|386BSD)
    PORT=`expr $TTY : 'com\(.*\)'`
    LOCKX="$LOCKDIR/LCK..$TTY"
    DEVS=/dev/$TTY
    tdev=/dev/$TTY
    ;;
SunOS|Linux|ULTRIX|HP-UX|FreeBSD|NetBSD)
    PORT=`expr $TTY : 'tty\(.*\)'`
    LOCKX="$LOCKDIR/LCK..$TTY"
    DEVS=/dev/$TTY
    tdev=/dev/$TTY
    ;;
svr4)
    PORT=`expr $TTY : 'term\/\(.*\)' \| $TTY`	# Usual
    PORT=`expr $PORT : 'cua\/\(.*\)' \| $PORT`	# Solaris
    PORT=`expr $PORT : 'tty\(.*\)' \| $PORT`	# Old-style
    DEVS=/dev/$TTY
    tdev=/dev/$TTY
    LOCKX="$LOCKDIR/`$SVR4UULCKN $DEVS`" || {
	echo "Sorry, I cannot determine the UUCP lock file name for $DEVS"
	exit 1
    }
    ;;
*)
    echo "Beware, I am guessing the tty naming conventions on your system:"
    PORT=`expr $TTY : 'tty\(.*\)'`;	echo "Serial port: $PORT"
    LOCKX="$LOCKDIR/LCK..$TTY";		echo "UUCP lock file: $LOCKX"
    DEVS=/dev/$TTY; tdev=/dev/$TTY;	echo "TTY device: $DEVS"
    ;;
esac
DEVID="`echo $TTY | tr '/' '_'`"
CONFIG=$CPATH.$DEVID

#
# Check that device is not currently being used.
#
for x in $LOCKX; do
    if [ -f $x ]; then
	echo "Sorry, the device is currently in use by another program."
	exit 1
    fi
done

#
# Look for conflicting configuration stuff.
#
OLDCONFIG=""

checkPort()
{
    devID="`echo $1 | tr '/' '_'`"
    if [ -f $CPATH.$devID -a -p $SPOOL/FIFO.$devID ]; then
	echo "There appears to be a modem already setup on $devID,"
	prompt "is this to be replaced [yes]?"
	read x;
	if [ "$x" = "n" -o "$x" = "no" ]; then
	    echo "Sorry, but you can not configure multiple servers on"
	    echo "the same serial port."
	    exit 1

	fi
	echo "Removing old FIFO special file $SPOOL/FIFO.$devID."
	$QUIT $devID >/dev/null 2>&1; rm -f $SPOOL/FIFO.$devID
	OLDCONFIG=$CPATH.$devID
    fi
}

if [ "$OS" = "IRIX" ]; then
    case $TTY in
    ttym${PORT}) checkPort ttyd${PORT}; checkPort ttyf${PORT};;
    ttyf${PORT}) checkPort ttym${PORT}; checkPort ttyd${PORT};;
    ttyd${PORT}) checkPort ttym${PORT}; checkPort ttyf${PORT};;
    esac
fi
$QUIT $DEVID >/dev/null 2>&1; sleep 1		# shutdown existing server

#
# Lock the device for later use when deducing the modem type.
#
JUNK="$JUNK $LOCKX"

LOCKSTR=`expr "         $$" : '.*\(..........\)'`
# lock the device by all of its names
for x in $LOCKX; do
    echo "$LOCKSTR" > $x
done
# zap any gettys or other users
fuser -k $DEVS >/dev/null 2>&1 || {
    cat<<EOF
Hmm, there does not appear to be an fuser command on your machine.
This means that I am unable to insure that all processes using the
modem have been killed.  I will keep going, but beware that you may
have competition for the modem.
EOF
}

cat<<EOF

Ok, time to setup a configuration file for the modem.  The manual
page config(4F) may be useful during this process.  Also be aware
that at any time you can safely interrupt this procedure.

EOF

getParameter()
{
    param=`grep "^$1:" $2 | sed -e 's/[ 	]*#.*//' -e 's/.*:[ 	]*//'`
}

#
# Get the server config values
# from the prototype config file.
#
getServerProtoParameters()
{
    file=$1
    getParameter FAXNumber $file;	    protoFAXNumber="$param"
    getParameter CountryCode $file;	    protoCountryCode="$param"
    getParameter AreaCode $file;	    protoAreaCode="$param"
    getParameter LongDistancePrefix $file;  protoLongDistancePrefix="$param"
    getParameter InternationalPrefix $file; protoInternationalPrefix="$param"
    getParameter DialStringRules $file;	    protoDialStringRules="$param"
    getParameter ServerTracing $file;	    protoServerTracing="$param"
    getParameter SessionTracing $file;	    protoSessionTracing="$param"
    getParameter RecvFileMode $file;	    protoRecvFileMode="$param"
    getParameter LogFileMode $file;	    protoLogFileMode="$param"
    getParameter DeviceMode $file;	    protoDeviceMode="$param"
    getParameter RingsBeforeAnswer $file;   protoRingsBeforeAnswer="$param"
    getParameter SpeakerVolume $file;
    # convert old numeric style
    case "$param" in
    0)	protoSpeakerVolume="off";;
    1)	protoSpeakerVolume="low";;
    2)	protoSpeakerVolume="quiet";;
    3)	protoSpeakerVolume="medium";;
    4)	protoSpeakerVolume="high";;
    *)	protoSpeakerVolume="$param";;
    esac
    # convert old-style UseDialPrefix & DialingPrefix to ModemDialCmd fmt
    getParameter UseDialPrefix $file;
    case "$param" in
    [yY]es|[oO]n) getParameter DialingPrefix $file; DialingPrefix="$param";;
    *)		  				    DialingPrefix="";;
    esac
    # convert old-style GettyAllowed & GettySpeed to GettyArgs
    getParameter GettyAllowed $file;
    case "$param" in
    [yY]es|[oO]n) getParameter GettySpeed $file; protoGettyArgs="$param";;
    *)		  getParameter GettyArgs $file;  protoGettyArgs="$param";;
    esac
    # convert old-style yes/no for QualifyTSI
    getParameter QualifyTSI $file;
    case "$param" in
    [yY]es|[oO]n) protoQualifyTSI="etc/tsi";;
    *)		  protoQualifyTSI="$param";;
    esac
    getParameter JobReqBusy $file;	protoJobReqBusy="$param"
    getParameter JobReqNoCarrier $file;	protoJobReqNoCarrier="$param"
    getParameter JobReqNoAnswer $file;	protoJobReqNoAnswer="$param"
    getParameter JobReqDataConn $file;	protoJobReqDataConn="$param"
    getParameter JobReqProto $file;	protoJobReqProto="$param"
    getParameter JobReqOther $file;	protoJobReqOther="$param"
    getParameter UUCPLockTimeout $file;	protoUUCPLockTimeout="$param"
    getParameter PollModemWait $file;	protoPollModemWait="$param"
    getParameter PollLockWait $file;	protoPollLockWait="$param"
    getParameter ContCoverPage $file;	protoContCoverPage="$param"
    getParameter TagLineFont $file;	protoTagLineFont="$param"
    getParameter TagLineFormat $file;	protoTagLineFormat="$param"
    getParameter PercentGoodLines $file;protoPercentGoodLines="$param"
    getParameter MaxConsecutiveBadLines $file;
	protoMaxConsecutiveBadLines="$param"
    getParameter MaxRecvPages $file;	protoMaxRecvPages="$param"
    getParameter MaxSendPages $file;	protoMaxSendPages="$param"
    getParameter MaxBadCalls $file;	protoMaxBadCalls="$param"
    getParameter PostScriptTimeout $file;
	protoPostScriptTimeout="$param"
    getParameter LocalIdentifier $file;	protoLocalIdentifier="$param"
}

setupServerParameters()
{
    FAXNumber="$protoFAXNumber"
    AreaCode="$protoAreaCode"
    CountryCode="${protoCountryCode:-1}"
    LongDistancePrefix="${protoLongDistancePrefix:-1}"
    InternationalPrefix="${protoInternationalPrefix:-011}"
    DialStringRules="${protoDialStringRules:-etc/dialrules}"
    ServerTracing="${protoServerTracing:-1}"
    SessionTracing="${protoSessionTracing:-11}"
    RecvFileMode="${protoRecvFileMode:-0600}"
    LogFileMode="${protoLogFileMode:-0600}"
    DeviceMode="${protoDeviceMode:-0600}"
    RingsBeforeAnswer="${protoRingsBeforeAnswer:-1}"
    SpeakerVolume="${protoSpeakerVolume:-off}"
    GettyArgs="$protoGettyArgs"
    QualifyTSI="$protoQualifyTSI"
    JobReqBusy="$protoJobReqBusy"
    JobReqNoCarrier="$protoJobReqNoCarrier"
    JobReqNoAnswer="$protoJobReqNoAnswer"
    JobReqDataConn="$protoJobReqDataConn"
    JobReqProto="$protoJobReqProto"
    JobReqOther="$protoJobReqOther"
    UUCPLockTimeout="$protoUUCPLockTimeout"
    PollModemWait="$protoPollModemWait"
    PollLockWait="$protoPollLockWait"
    ContCoverPage="$protoContCoverPage"
    TagLineFont="$protoTagLineFont"
    TagLineFormat="$protoTagLineFormat"
    PercentGoodLines="$protoPercentGoodLines"
    MaxConsecutiveBadLines="$protoMaxConsecutiveBadLines"
    MaxRecvPages="$protoMaxRecvPages"
    MaxSendPages="$protoMaxSendPages"
    MaxBadCalls="$protoMaxBadCalls"
    PostScriptTimeout="$protoPostScriptTimeout"
    LocalIdentifier="$protoLocalIdentifier"
}

#
# Append a sed command to the ServerCmds if we need to
# alter the existing configuration parameter.  Note that
# we handle the case where there are embedded blanks or
# tabs by enclosing the string in quotes.
#
addServerSedCmd()
{
    test "$1" != "$2" && {
	# escape backslashes to counteract shell parsing and sed magic chars
	x=`echo "$2" | sed 's/[&\\\\"]/\\\\&/g'`
	if [ `expr "$2" : "[^\"].*[ ]"` = 0 ]; then
	    ServerCmds="$ServerCmds -e '/^$3:/s;\(:[ 	]*\).*;\1$x;'"
	else
	    ServerCmds="$ServerCmds -e '/^$3:/s;\(:[ 	]*\).*;\1\"$x\";'"
	fi
    }
}

#
# Setup the sed commands for crafting the configuration file:
#
makeSedServerCommands()
{
    ServerCmds=""
    addServerSedCmd "$protoFAXNumber"		"$FAXNumber"	FAXNumber
    addServerSedCmd "$protoAreaCode"		"$AreaCode"	AreaCode
    addServerSedCmd "$protoCountryCode"		"$CountryCode"	CountryCode
    addServerSedCmd "$protoLongDistancePrefix"	"$LongDistancePrefix" \
	LongDistancePrefix
    addServerSedCmd "$protoInternationalPrefix"	"$InternationalPrefix" \
	InternationalPrefix
    addServerSedCmd "$protoDialStringRules"	"$DialStringRules" \
	DialStringRules
    addServerSedCmd "$protoServerTracing"	"$ServerTracing" \
	ServerTracing
    addServerSedCmd "$protoSessionTracing"	"$SessionTracing" \
	SessionTracing
    addServerSedCmd "$protoRecvFileMode"	"$RecvFileMode"	RecvFileMode
    addServerSedCmd "$protoLogFileMode"		"$LogFileMode"	LogFileMode
    addServerSedCmd "$protoDeviceMode"		"$DeviceMode"	DeviceMode
    addServerSedCmd "$protoRingsBeforeAnswer"	"$RingsBeforeAnswer" \
	RingsBeforeAnswer
    addServerSedCmd "$protoSpeakerVolume"	"$SpeakerVolume" \
	SpeakerVolume
    addServerSedCmd "$protoGettyArgs"		"$GettyArgs"	GettyArgs
    addServerSedCmd "$protoQualifyTSI"		"$QualifyTSI"	QualifyTSI
    addServerSedCmd "$protoJobReqBusy"		"$JobReqBusy" \
       JobReqBusy
    addServerSedCmd "$protoJobReqNoCarrier"	"$JobReqNoCarrier" \
	JobReqNoCarrier
    addServerSedCmd "$protoJobReqNoAnswer"	"$JobReqNoAnswer" \
	JobReqNoAnswer
    addServerSedCmd "$protoJobReqDataConn"	"$JobReqDataConn" \
	JobReqDataConn
    addServerSedCmd "$protoJobReqProto"		"$JobReqProto" \
       JobReqProto
    addServerSedCmd "$protoJobReqOther"		"$JobReqOther" \
       JobReqOther
    addServerSedCmd "$protoUUCPLockTimeout"	"$UUCPLockTimeout" \
	UUCPLockTimeout
    addServerSedCmd "$protoPollModemWait"	"$PollModemWait" \
	PollModemWait
    addServerSedCmd "$protoPollLockWait"	"$PollLockWait" \
        PollLockWait
    addServerSedCmd "$protoContCoverPage"	"$ContCoverPage" \
	ContCoverPage
    addServerSedCmd "$protoTagLineFont"		"$TagLineFont" \
	TagLineFont
    addServerSedCmd "$protoTagLineFormat"	"$TagLineFormat" \
	TagLineFormat
    addServerSedCmd "$protoPercentGoodLines"	"$PercentGoodLines" \
	PercentGoodLines
    addServerSedCmd "$protoMaxConsecutiveBadLines" "$MaxConsecutiveBadLines" \
	MaxConsecutiveBadLines
    addServerSedCmd "$protoMaxRecvPages"	"$MaxRecvPages"	MaxRecvPages
    addServerSedCmd "$protoMaxSendPages"	"$MaxSendPages"	MaxSendPages
    addServerSedCmd "$protoMaxBadCalls"		"$MaxBadCalls"	MaxBadCalls
    addServerSedCmd "$protoPostScriptTimeout"	"$PostScriptTimeout" \
	PostScriptTimeout
    addServerSedCmd "$protoLocalIdentifier"	"$LocalIdentifier" \
	LocalIdentifier
}

#
# Prompt the user for volume setting.
#
promptForSpeakerVolume()
{
    x=""
    while [ -z "$x" ]; do
	prompt "Modem speaker volume [$SpeakerVolume]?"; read x
	if [ "$x" != "" ]; then
	    # strip leading and trailing white space
	    x=`echo "$x" | sed -e 's/^[ 	]*//' -e 's/[ 	]*$//'`
	    case "$x" in
	    [oO]*)	x="off";;
	    [lL]*)	x="low";;
	    [qQ]*)	x="quiet";;
	    [mM]*)	x="medium";;
	    [hH]*)	x="high";;
	    *)
cat <<EOF

"$x" is not a valid speaker volume setting; use one
of: "off", "low", "quiet", "medium", and "high".
EOF
		x="";;
	    esac
	else
	    x="$SpeakerVolume"
	fi
    done
    SpeakerVolume="$x"
}

#
# Verify that the fax number, area code, and country
# code jibe.  Perhaps this is too specific to the USA?
#
checkFaxNumber()
{
    pat="[\"]*\+$CountryCode[-. ]*$AreaCode[-. ]*[0-9][- .0-9]*[\"]*"
    match=`expr "$FAXNumber" : "\($pat\)"`
    if [ "$match" != "$FAXNumber" ]; then
	cat<<EOF

Your facsimile phone number ($FAXNumber) does not agree with your
country code ($CountryCode) or area code ($AreaCode).  The number
should be a fully qualified international dialing number of the form:

    +$CountryCode $AreaCode <local phone number>

Spaces, hyphens, and periods can be included for legibility.  For example,

    +$CountryCode.$AreaCode.555.1212

is a possible phone number (using your country and area codes).
EOF
	ok="no";
    fi
}

#
# Verify that a number is octal and if not, add a prefixing "0".
#
checkOctalNumber()
{
    param=$1
    if [ "`expr "$param" : '\(.\)'`" != "0" ]; then
	param="0${param}"
	return 0
    else
	return 1
    fi
}

#
# Verify that the dial string rules file exists.
#
checkDialStringRules()
{
    if [ ! -f $SPOOL/$DialStringRules ]; then
	cat<<EOF

Warning, the dial string rules file,

    $SPOOL/$DialStringRules

does not exist, or is not a plain file.  The rules file
must reside in the $SPOOL directory tree.
EOF
	ok="no";
    fi
}

#
# Verify that the tag line font file exists.
#
checkTagLineFont()
{
    if [ ! -f $SPOOL/$TagLineFont ]; then
	cat<<EOF

Warning, the tag line font file,

    $SPOOL/$TagLineFont

does not exist, or is not a plain file.  The font file
must reside in the $SPOOL directory tree.
EOF
	ok="no";
    fi
}

#
# Print the current server configuration parameters.
#
printServerConfig()
{
    cat<<EOF

The server configuration parameters are:

AreaCode:		$AreaCode
CountryCode:		$CountryCode
FAXNumber:		$FAXNumber
LongDistancePrefix:	$LongDistancePrefix
InternationalPrefix:	$InternationalPrefix
DialStringRules:	$DialStringRules
ServerTracing:		$ServerTracing
SessionTracing:		$SessionTracing
RecvFileMode:		$RecvFileMode
LogFileMode:		$LogFileMode
DeviceMode:		$DeviceMode
RingsBeforeAnswer:	$RingsBeforeAnswer
SpeakerVolume:		$SpeakerVolume
EOF
    test "$protoGettyArgs" &&
	echo "GettyArgs:		$GettyArgs"
    test "$protoQualifyTSI" &&
	echo "QualifyTSI:		$QualifyTSI"
    test "$protoJobReqBusy" &&
	echo "JobReqBusy:		$JobReqBusy"
    test "$protoJobReqNoCarrier" &&
	echo "JobReqNoCarrier:	$JobReqNoCarrier"
    test "$protoJobReqNoAnswer" &&
	echo "JobReqNoAnswer:		$JobReqNoAnswer"
    test "$protoJobReqProto" &&
	echo "JobReqProto:		$JobReqProto"
    test "$protoJobReqOther" &&
	echo "JobReqOther:		$JobReqOther"
    test "$protoUUCPLockTimeout" &&
	echo "UUCPLockTimeout:	$UUCPLockTimeout"
    test "$protoPollModemWait" &&
	echo "PollModemWait:		$PollModemWait"
    test "$protoPollLockWait" &&
	echo "PollLockWait:		$PollLockWait"
    test "$protoContCoverPage" &&
	echo "ContCoverPage:		$ContCoverPage"
    test "$protoTagLineFont" &&
	echo "TagLineFont:		$TagLineFont"
    test "$protoTagLineFormat" &&
	echo "TagLineFormat:		$TagLineFormat"
    test "$protoPercentGoodLines" &&
	echo "PercentGoodLines:	$PercentGoodLines"
    test "$protoMaxConsecutiveBadLines" &&
	echo "MaxConsecutiveBadLines:	$MaxConsecutiveBadLines"
    test "$protoMaxRecvPages" &&
	echo "MaxRecvPages:		$MaxRecvPages"
    test "$protoMaxSendPages" &&
	echo "MaxSendPages:		$MaxSendPages"
    test "$protoMaxBadCalls" &&
	echo "MaxBadCalls:		$MaxBadCalls"
    test "$protoPostScriptTimeout" &&
	echo "PostScriptTimeout:	$PostScriptTimeout"
    test "$protoLocalIdentifier" &&
	echo "LocalIdentifier:		$LocalIdentifier"
    echo ""
}

if [ -f $CONFIG -o -z "$OLDCONFIG" ]; then
    OLDCONFIG=$CONFIG
fi
if [ -f $OLDCONFIG ]; then
    echo "Hey, there is an existing config file "$OLDCONFIG"..."
    getServerProtoParameters $OLDCONFIG
    ok="skip"				# skip prompting first time through
else
    echo "No existing configuration, let's do this from scratch."
    echo ""
    getServerProtoParameters $MODEMCONFIG/config.skel # get from skeletal file
    ok="prompt"				# prompt for parameters
fi
setupServerParameters			# record prototype & setup defaults

if [ -z "$FAXNumber" ]; then
    FAXNumber="+$CountryCode $AreaCode 555-1212"
fi
#
# Prompt user for server-related configuration parameters
# and do consistency checking on what we get.
#
while [ "$ok" != "" -a "$ok" != "y" -a "$ok" != "yes" ]; do
    if [ "$ok" != "skip" ]; then
	promptForNumericParameter "$CountryCode" \
	    "Country code"; CountryCode=$param;
	promptForNumericParameter "$AreaCode" \
	    "Area code"; AreaCode=$param;
	promptForNonNullStringParameter "$FAXNumber" \
	    "Phone number of fax modem"; FAXNumber=$param;
	promptForNumericParameter "$LongDistancePrefix" \
	    "Long distance dialing prefix"; LongDistancePrefix=$param;
	promptForNumericParameter "$InternationalPrefix" \
	    "International dialing prefix"; InternationalPrefix=$param;
	promptForNonNullStringParameter "$DialStringRules" \
	    "Dial string rules file"; DialStringRules=$param;
	promptForCStyleNumericParameter "$ServerTracing" \
	    "Tracing during normal server operation"; ServerTracing=$param;
	promptForCStyleNumericParameter "$SessionTracing" \
	    "Tracing during send and receive sessions"; SessionTracing=$param;
	promptForNumericParameter "$RecvFileMode" \
	    "Protection mode for received facsimile"; RecvFileMode=$param;
	promptForNumericParameter "$LogFileMode" \
	    "Protection mode for session logs"; LogFileMode=$param;
	promptForNumericParameter "$DeviceMode" \
	    "Protection mode for $TTY"; DeviceMode=$param;
	promptForNumericParameter "$RingsBeforeAnswer" \
	    "Rings to wait before answering"; RingsBeforeAnswer=$param;
	promptForSpeakerVolume;
	test "$protoGettyArgs" && {
	    promptForNonNullStringParameter "$GettyArgs" \
		"Command line arguments to getty program";
	    GettyArgs="$param";
	}
	test "$protoQualifyTSI"	&& {
	    promptForNonNullStringParameter "$QualifyTSI" \
		"Relative pathname of TSI patterns file";
	    QualifyTSI="$param";
	}
	test "$protoJobReqBusy"	&& {
	    promptForCStyleNumericParameter "$JobReqBusy" \
		"Job requeue interval on BUSY status (secs)";
	    JobReqBusy=$param;
	}
	test "$protoJobReqNoCarrier" && {
	    promptForCStyleNumericParameter "$JobReqNoCarrier" \
		"Job requeue interval on NO CARRIER status (secs)";
	    JobReqNoCarrier=$param;
	}
	test "$protoJobReqNoAnswer" && {
	    promptForCStyleNumericParameter "$JobReqNoAnswer" \
		"Job requeue interval on NO ANSWER status (secs)";
	    JobReqNoAnswer=$param;
	}
	test "$protoJobReqProto" && {
	    promptForCStyleNumericParameter "$JobReqProto" \
		"Job requeue interval on protocol error (secs)";
	    JobReqProto=$param;
	}
	test "$protoJobReqOther" && {
	    promptForCStyleNumericParameter "$JobReqOther" \
		"Job requeue interval for \"other\" sorts of problems (secs)";
	    JobReqOther=$param;
	}
	test "$protoUUCPLockTimeout" && {
	    promptForCStyleNumericParameter "$UUCPLockTimeout" \
		"UUCP lock file timeout (secs)";
	    UUCPLockTimeout=$param;
	}
	test "$protoPollModemWait" && {
	    promptForCStyleNumericParameter "$PollModemWait" \
		"Polling interval for modem-wait state (secs)";
	    PollModemWait=$param;
	}
	test "$protoPollLockWait" && {
	    promptForCStyleNumericParameter "$PollLockWait" \
		"Polling interval for lock-wait state (secs)";
	    PollLockWait=$param;
	}
	test "$protoContCoverPage" && {
	    promptForStringParameter "$ContCoverPage" \
		"Continuation cover page template file";
	    ContCoverPage=$param;
	}
	test "$protoTagLineFont" && {
	    promptForStringParameter "$TagLineFont" \
		"Tag line font (file)";
	    TagLineFont=$param;
	}
	test "$protoTagLineFormat" && {
	    promptForStringParameter "$TagLineFormat" \
		"Tag line format string";
	    TagLineFormat=$param;
	}
	test "$protoPercentGoodLines" && {
	    promptForNumericParameter "$PercentGoodLines" \
		"Percent good lines to accept during copy quality checking";
	    PercentGoodLines=$param;
	}
	test "$protoMaxConsecutiveBadLines" && {
	    promptForNumericParameter "$MaxConsecutiveBadLines" \
		"Max consecutive bad lines to accept during copy quality checking";
	    MaxConsecutiveBadLines=$param;
	}
	test "$protoMaxRecvPages" && {
	    promptForNumericParameter "$MaxRecvPages" \
		"Max pages to accept when receiving a facsimile";
	    MaxRecvPages=$param;
	}
	test "$protoMaxSendPages" && {
	    promptForNumericParameter "$MaxSendPages" \
		"Max pages to permit when sending a facsimile";
	    MaxSendPages=$param;
	}
	test "$protoMaxBadCalls" && {
	    promptForNumericParameter "$MaxBadCalls" \
		"Max consecutive bad calls before considering modem wedged";
	    MaxBadCalls=$param;
	}
	test "$protoPostScriptTimeout" && {
	    promptForNumericParameter "$PostScriptTimeout" \
		"Timeout when converting PostScript jobs (secs)";
	    PostScriptTimeout=$param;
	}
	test "$protoLocalIdentifier" && {
	    promptForStringParameter "$LocalIdentifier" \
		"Local identification string";
	    LocalIdentifier=$param;
	}
    fi
    checkOctalNumber $RecvFileMode &&	RecvFileMode=$param
    checkOctalNumber $LogFileMode &&	LogFileMode=$param
    checkOctalNumber $DeviceMode &&	DeviceMode=$param
    checkDialStringRules;
    checkFaxNumber;
    test "$TagLineFont" && checkTagLineFont;
    printServerConfig; prompt "Are these ok [yes]?"; read ok
done

#
# We've got all the server-related parameters, now for the modem ones.
#

cat<<EOF

Now we are going to probe the tty port to figure out the type
of modem that is attached.  This takes a few seconds, so be patient.
Note that if you do not have the modem cabled to the port, or the
modem is turned off, this may hang (just go and cable up the modem
or turn it on, or whatever).
EOF

if [ $OS = "SunOS" ]; then
    #
    # Sun systems have a command for manipulating software
    # carrier on a terminal line.  Set or reset carrier
    # according to the type of tty device being used.
    #
    case $TTY in
    tty*) ttysoftcar -y $TTY;;
    cua*) ttysoftcar -n $TTY;;
    esac
fi

if [ -x ${ONDELAY} ]; then
    onDev() {
	if [ "$1" = "-c" ]; then
	    shift; catpid=`${ONDELAY} $tdev sh -c "$* >$OUT" & echo $!`
	else
	    ${ONDELAY} $tdev sh -c "$*"
	fi
    }
else
cat<<'EOF'

The "ondelay" program to open the device without blocking is not
present.  We're going to try to continue without it; let's hope that
the serial port won't block waiting for carrier...
EOF
    onDev() {
	if [ "$1" = "-c" ]; then
	    shift; catpid=`sh <$tdev >$tdev -c "$* >$OUT" & echo $!`
	else
	    sh <$tdev >$tdev -c "$*"
	fi
    }
fi

case $OS in
*bsd*|*BSD*)	STTY="stty -f $tdev";;
*)		STTY=stty;;
esac

#
# Send each command in SendString to the modem and collect
# the result in $OUT.  Read this very carefully.  It's got
# a lot of magic in it!
#
SendToModem()
{
    onDev $STTY 0				# reset the modem (hopefully)
    onDev -c "$STTY clocal && exec $CAT $tdev"	# start listening for output
    sleep 3					# let listener open dev first
    onDev $STTY -echo -icrnl -ixon -ixoff -isig clocal $SPEED; sleep 1
    # NB: merging \r & ATQ0 causes some modems problems
    printf "\r" >$tdev; sleep 1;		# force consistent state
    printf "ATQ0V1E1\r" >$tdev; sleep 1;	# enable echo and result codes
    for i in $*; do
	printf "$i\r" >$tdev; sleep 1;
    done
    kill -9 $catpid; catpid=
    # NB: [*&\\\\$] must have the "$" last for AIX (yech)
    pat=`echo "$i"|sed -e 's/[*&\\\\$]/\\\\&/g'` # escape regex metacharacters
    RESPONSE=`tr -d '\015' < $OUT | \
	sed -n "/$pat/{n;s/ *$//;p;q;}" | sed 's/+F.*=//'`
}

echo ""
if [ -z "$SPEED" ]; then
    #
    # Probe for the highest speed at which the modem
    # responds to "AT" with "OK".
    #
    printf "Probing for best speed to talk to modem:"
    for SPEED in $SPEEDS
    do
	printf " $SPEED"
	SendToModem "AT" >/dev/null 2>&1
	sleep 1
	RESULT=`tr -d "\015" < $OUT | tail -1`
	test "$RESULT" = "OK" && break;
    done
    if [ "$RESULT" != "OK" ]; then
	echo ""
	echo "Unable to deduce DTE-DCE speed; check that you are using the"
	echo "correct device and/or that your modem is setup properly.  If"
	echo "all else fails, try the -s option to lock the speed."
	exit 1
    fi
    echo " OK."
else
    echo "Using user-specified $SPEED to talk to modem."
fi
RESULT="";
while [ -z "$RESULT" ]; do
    #
    # This goes in the background while we try to
    # reset the modem.  If something goes wrong, it'll
    # nag the user to check on the problem.
    #
    (trap 0 1 2 15;
     while true; do
	sleep 10;
	echo ""
	echo "Hmm, something seems to be hung, check your modem eh?"
     done)& nagpid=$!
    trap "rm -f \$JUNK; kill $nagpid \$catpid; exit 1" 0 1 2 15

    SendToModem "AT+FCLASS=?" 			# ask for class support

    kill $nagpid
    trap "rm -f \$JUNK; test \"\$catpid\" && kill \$catpid; exit 1" 0 1 2 15
    sleep 1

    RESULT=`tr -d "\015" < $OUT | tail -1`
    if [ -z "$RESPONSE" ]; then
	echo ""
	echo "There was no response from the modem.  Perhaps the modem is"
	echo "turned off or the cable between the modem and host is not"
	echo "connected.  Please check the modem and hit a carriage return"
	prompt "when you are ready to try again:"
	read x
    fi
done

ModemType="" Manufacturer="" Model="" ProtoType="config.skel"

#
# Select a configuration file for a modem based on the
# deduced modem type.  Each routine below sends a set
# of commands to the modem to figure out the modem model
# and manufacturer and then compares them against the
# set of known values in associated config files.
# Note that this is done with a tricky bit of shell
# hacking--generating a case statement that is then
# eval'dwith the result being the setup of the
# ProtoType shell variable.
#
configureClass2Modem()
{
    ModemType=Class2
    echo "Hmm, this looks like a Class 2 modem."

    SendToModem "AT+FCLASS=2" "AT+FMFR?"
    Manufacturer=$RESPONSE
    echo "Modem manufacturer is \"$Manufacturer\"."

    SendToModem "AT+FCLASS=2" "AT+FMDL?"
    Model=$RESPONSE
    echo "Modem model is \"$Model\"."

    eval `(cd $MODEMCONFIG; \
	grep 'CONFIG:[ 	]*CLASS2' config.* |\
	awk -F: '
	    BEGIN { print "case \"$Manufacturer-$Model\" in" }
	    { print $4 ") ProtoType=" $1 ";;" }
	    END { print "*) ProtoType=config.class2;;"; print "esac" }
	')`
}

#
# As above, but for Class 2.0 modems.
#
configureClass2dot0Modem()
{
    ModemType=Class20
    echo "Hmm, this looks like a Class 2.0 modem."
    #
    SendToModem "AT+FCLASS=2.0" "AT+FMI?"
    Manufacturer=$RESPONSE
    echo "Modem manufacturer is \"$Manufacturer\"."

    SendToModem "AT+FCLASS=2.0" "AT+FMM?"
    Model=$RESPONSE
    echo "Modem model is \"$Model\"."

    eval `(cd $MODEMCONFIG; \
	grep 'CONFIG:[ 	]*CLASS2.0' config.* |\
	awk -F: '
	    BEGIN { print "case \"$Manufacturer-$Model\" in" }
	    { print $4 ") ProtoType=" $1 ";;" }
	    END { print "*) ProtoType=config.class2.0;;"; print "esac" }
	')`
}

#
# Class 1 modems are handled a bit differently as
# there are no commands to obtain the manufacturer
# and model.  Instead we use ATI0 to get the product
# code and then compare it against the set of known
# values in the config files.
#
configureClass1Modem()
{
    ModemType=Class1 Manufacturer=Unknown Model=Unknown
    echo "Hmm, this looks like a Class 1 modem."

    SendToModem "ATI0"
    echo "Product code is \"$RESPONSE\"."

    eval `(cd $MODEMCONFIG; grep 'CONFIG:[ 	]*CLASS1' config.*) |\
	sed 's/:[ 	]*/:/g' |\
	awk -F: '
BEGIN	{ proto = "" }
$4 == C	{ if (proto != "") {
	      print "echo \"Warning, multiple configuration files exist for this modem,\";"
	      print "echo \"   the file " $1 " is ignored.\";"
	  } else
	      proto = $1 " " $5;
	}
END	{ if (proto == "")
	      proto = "config.class1"
	  print "ProtoType=" proto
	}
	' C="$RESPONSE" -`
    echo "Modem manufacturer is \"$Manufacturer\"."
    echo "Modem model is \"$Model\"."
}

configureClass1and2Modem()
{
    echo "This modem looks to have support for both Class 1 and 2;"
    prompt "how should it be configured [2]?"
    x=""
    while [ "$x" != "1" -a "$x" != "2" ]; do
	read x
	case "$x" in
	1|2)	;;
	?*)	echo ""
	        prompt "Configure as Class 1 or 2 [2]?";;
	*)	x="2";;
	esac
    done
    echo ""
    case $x in
    1) configureClass1Modem;;
    2) configureClass2Modem;;
    esac
}

configureClass1and2dot0Modem()
{
    echo "This modem looks to have support for both Class 1 and 2.0;"
    prompt "how should it be configured [2.0]?"
    x=""
    while [ "$x" != "1" -a "$x" != "2.0" ]; do
	read x
	case "$x" in
	1|2.0)	;;
	?*)	echo ""
	        prompt "Configure as Class 1 or 2.0 [2.0]?";;
	*)	x="2.0";;
	esac
    done
    echo ""
    case $x in
    1)   configureClass1Modem;;
    2.0) configureClass2dot0Modem;;
    esac
}

configureClass2and2dot0Modem()
{
    echo "This modem looks to have support for both Class 2 and 2.0;"
    prompt "how should it be configured [2.0]?"
    x=""
    while [ "$x" != "2" -a "$x" != "2.0" ]; do
	read x
	case "$x" in
	2|2.0)	;;
	?*)	echo ""
	        prompt "Configure as Class 2 or 2.0 [2.0]?";;
	*)	x="2.0";;
	esac
    done
    echo ""
    case $x in
    2)   configureClass2Modem;;
    2.0) configureClass2dot0Modem;;
    esac
}

configureClass1and2and2dot0Modem()
{
    echo "This modem looks to have support for both Class 1, 2 and 2.0;"
    prompt "how should it be configured [2.0]?"
    x=""
    while [ "$x" != "1" -a "$x" != "2" -a "$x" != "2.0" ]; do
	read x
	case "$x" in
	1|2|2.0)	;;
	?*)	echo ""
	        prompt "Configure as Class 1, 2 or 2.0 [2.0]?";;
	*)	x="2.0";;
	esac
    done
    echo ""
    case $x in
    1)   configureClass1Modem;;
    2)   configureClass2Modem;;
    2.0) configureClass2dot0Modem;;
    esac
}

giveup()
{
	cat<<EOF

We were unable to deduce what type of modem you have.  This means that
it did not respond as a Class 1, Class 2, or Class 2.0 modem should and
it did not appear as though it was an Abaton 24/96 (as made by Everex).
If you believe that your modem conforms to the Class 1, Class 2, or
Class 2.0 interface specification, or is an Abaton modem, then check
that the modem is operating properly and that you can communicate with
the modem from the host.  If your modem is not one of the above types
of modems, then this software does not support it and you will need to
write a driver that supports it.

EOF
	exit 1
}

echo ""
if [ "$RESULT" = "OK" ]; then
    # Looks like a Class 1, 2, or 2.0 modem, get more information
    case "`echo $RESPONSE | sed -e 's/[()]//g'`" in
    1)			configureClass1Modem;;
    2)			configureClass2Modem;;
    2.0)		configureClass2dot0Modem;;
    0,1)		configureClass1Modem;;
    0,2)		configureClass2Modem;;
    0,2.0)		configureClass2dot0Modem;;
    1,2)		configureClass1and2Modem;;
    1,2.0)		configureClass1and2dot0Modem;;
    2,2.0)		configureClass2and2dot0Modem;;
    0,1,2)		configureClass1and2Modem;;
    0,1,2.0)		configureClass1and2dot0Modem;;
    0,2,2.0)		configureClass2and2dot0Modem;;
    1,2,2.0)		configureClass1and2and2dot0Modem;;
    0,1,2,2.0)		configureClass1and2and2dot0Modem;;
    0,1,2,2.0,*)	configureClass1and2and2dot0Modem;;
    1,2,2.0,*)		configureClass1and2and2dot0Modem;;
    0,2,2.0,*)		configureClass2and2dot0Modem;;
    0,1,2.0,*)		configureClass1and2dot0Modem;;
    0,1,2,*)		configureClass1and2Modem;;
    2,2.0,*)		configureClass2and2dot0Modem;;
    1,2.0,*)		configureClass1and2dot0Modem;;
    1,2,*)		configureClass1and2Modem;;
    0,2.0,*)		configureClass2dot0Modem;;
    0,2,*)		configureClass2Modem;;
    0,1,*)		configureClass1Modem;;
    2.0,*)		configureClass2dot0Modem;;
    2,*)		configureClass2Modem;;
    1,*)		configureClass1Modem;;
    *)
	echo "The result of the AT+FCLASS=? command was:"
	echo ""
	cat $OUT
	giveup
	;;
    esac
else
    echo "This not a Class 1, 2, or 2.0 modem,"
    echo "lets check some other possibilities..."

    SendToModem "ATI4?"
    RESULT=`tr -d "\015" < $OUT | tail -1`
    echo ""
    if [ "$RESULT" != "OK" ]; then
	echo "The modem does not seem to understand the ATI4? query."
	echo "Sorry, but I can't figure out how to configure this one."
	exit 1
    fi
    case "$RESPONSE" in
    EV968*|EV958*)
	ModemType="Abaton";
	echo "Hmm, this looks like an Abaton 24/96 (specifically a $RESPONSE)."
	;;
    *)	echo "The result of the ATI4? query was:"
	echo ""
	cat $OUT
	giveup;
	;;
    esac
fi

echo ""
#
# Given a modem type, manufacturer and model, select
# a prototype configuration file to work from.
#
case $ModemType in
Class1|Class2*)
    echo "Using prototype configuration file $ProtoType..."
    ;;
Abaton)
    echo "Using prototype Abaton 24/96 configuration file..."
    ProtoType=config.abaton
    ;;
esac

proto=$MODEMCONFIG/$ProtoType
if [ ! -f $proto ]; then
   cat<<EOF
Uh oh, I can't find the prototype file

"$proto"

EOF
    if [ "$ProtoType" != "config.skel" ]; then
        prompt "Do you want to continue using the skeletal configuration file [yes]?"
        read x
        if [ "$x" = "n" -o "$x" = "no" ]; then
	   exit 1
	fi
	ProtoType="config.skel";
	proto=$MODEMCONFIG/$ProtoType;
	if [ ! -f $proto ]; then
	    cat<<EOF
Sigh, the skeletal configuration file is not available either.  There
is not anything that I can do without some kind of prototype config
file; I'm bailing out..."
EOF
	    exit 1
	fi
    else
	echo "There is nothing more that I can do; I'm bailing out..."
	exit 1
    fi
fi

#
# Prompt the user for an AT-style command.
#
promptForATCmdParameter()
{
    prompt "$2 [$1]?"; read x
    if [ "$x" ]; then
	# strip leading and trailing white space,
	# quote marks, and any prefacing AT; redouble
	# any backslashes lost through shell processing
	x=`echo "$x" | \
	    sed -e 's/^[ 	]*//' -e 's/[ 	]*$//' -e 's/\"//g' | \
	    sed -e 's/^[aA][tT]//' -e 's/\\\\/&&/g'`
    else
	x="$1"
    fi
    param="$x"
}

#
# Prompt the user for a bit order.
#
promptForBitOrderParameter()
{
    x=""
    while [ -z "$x" ]; do
	prompt "$2 [$1]?"; read x
	if [ "$x" ]; then
	    # strip leading and trailing white space
	    x=`echo "$x" | sed -e 's/^[ 	]*//' -e 's/[ 	]*$//'`
	    case "$x" in
	    [lL]*)	x="LSB2MSB";;
	    [mM]*)	x="MSB2LSB";;
	    *)
cat <<EOF

"$x" is not a valid bit order parameter setting; use
one of: "lsb2msb", "LSB2MSB", "msb2lsb", or "MSB2LSB".
EOF
		x="";;
	    esac
	else
	    x="$1"
	fi
    done
    param="$x"
}

#
# Prompt the user for a flow control scheme.
#
promptForFlowControlParameter()
{
    x=""
    while [ -z "$x" ]; do
	prompt "$2 [$1]?"; read x
	if [ "$x" ]; then
	    # strip leading and trailing white space
	    x=`echo "$x" | sed -e 's/^[ 	]*//' -e 's/[ 	]*$//'`
	    case "$x" in
	    xon*|XON*)	x="xonxoff";;
	    rts*|RTS*)	x="rtscts";;
	    *)
cat <<EOF

"$x" is not a valid bit order parameter setting; use
one of: "xonxoff", "XONXOFF", "rtscts", or "RTSCTS".
EOF
		x="";;
	    esac
	else
	    x="$1"
	fi
    done
    param="$x"
}

saveModemProtoParameters()
{
    protoModemAnswerAnyCmd="$ModemAnswerAnyCmd"
    protoModemAnswerBeginDataCmd="$ModemAnswerBeginDataCmd"
    protoModemAnswerBeginFaxCmd="$ModemAnswerBeginFaxCmd"
    protoModemAnswerBeginVoiceCmd="$ModemAnswerBeginVoiceCmd"
    protoModemAnswerCmd="$ModemAnswerCmd"
    protoModemAnswerDataCmd="$ModemAnswerDataCmd"
    protoModemAnswerFaxCmd="$ModemAnswerFaxCmd"
    protoModemAnswerResponseTimeout="$ModemAnswerResponseTimeout"
    protoModemAnswerVoiceCmd="$ModemAnswerVoiceCmd"
    protoModemBaudRateDelay="$ModemBaudRateDelay"
    protoModemCommaPauseTimeCmd="$ModemCommaPauseTimeCmd"
    protoModemDialCmd="$ModemDialCmd"
    protoModemDialResponseTimeout="$ModemDialResponseTimeout"
    protoModemEchoOffCmd="$ModemEchoOffCmd"
    protoModemFlowControl="$ModemFlowControl"
    protoModemFlowControlCmd="$ModemFlowControlCmd"
    protoModemFrameFillOrder="$ModemFrameFillOrder"
    protoModemHostFillOrder="$ModemHostFillOrder"
    protoModemInterPacketDelay="$ModemInterPacketDelay"
    protoModemMaxPacketSize="$ModemMaxPacketSize"
    protoModemMfrQueryCmd="$ModemMfrQueryCmd"
    protoModemModelQueryCmd="$ModemModelQueryCmd"
    protoModemNoAutoAnswerCmd="$ModemNoAutoAnswerCmd"
    protoModemOnHookCmd="$ModemOnHookCmd"
    protoModemPageDoneTimeout="$ModemPageDoneTimeout"
    protoModemPageStartTimeout="$ModemPageStartTimeout"
    protoModemRate="$ModemRate"
    protoModemRecvFillOrder="$ModemRecvFillOrder"
    protoModemResetCmds="$ModemResetCmds"
    protoModemResetDelay="$ModemResetDelay"
    protoModemResultCodesCmd="$ModemResultCodesCmd"
    protoModemRevQueryCmd="$ModemRevQueryCmd"
    protoModemSendFillOrder="$ModemSendFillOrder"
    protoModemSetVolumeCmd="$ModemSetVolumeCmd"
    protoModemSetupAACmd="$ModemSetupAACmd"
    protoModemSetupDCDCmd="$ModemSetupDCDCmd"
    protoModemSetupDTRCmd="$ModemSetupDTRCmd"
    protoModemSoftResetCmd="$ModemSoftResetCmd"
    protoModemVerboseResultsCmd="$ModemVerboseResultsCmd"
    protoModemWaitForConnect="$ModemWaitForConnect"
    protoModemWaitTimeCmd="$ModemWaitTimeCmd"

    protoFaxT1Timer="$FaxT1Timer"
    protoFaxT2Timer="$FaxT2Timer"
    protoFaxT4Timer="$FaxT4Timer"
    case "$ModemType" in
    Class1*)
	protoClass1Cmd="$Class1Cmd"
	protoClass1FrameOverhead="$Class1FrameOverhead"
	protoClass1RecvAbortOK="$Class1RecvAbortOK"
	protoClass1RecvIdentTimer="$Class1RecvIdentTimer"
	protoClass1SendPPMDelay="$Class1SendPPMDelay"
	protoClass1SendTCFDelay="$Class1SendTCFDelay"
	protoClass1TCFResponseDelay="$Class1TCFResponseDelay"
	protoClass1TrainingRecovery="$Class1TrainingRecovery"
	;;
    Class2*)
	protoClass2AbortCmd="$Class2AbortCmd"
	protoClass2BORCmd="$Class2BORCmd"
	protoClass2BUGCmd="$Class2BUGCmd"
	protoClass2CIGCmd="$Class2CIGCmd"
	protoClass2CQCmd="$Class2CQCmd"
	protoClass2CRCmd="$Class2CRCmd"
	protoClass2Cmd="$Class2Cmd"
	protoClass2DCCCmd="$Class2DCCCmd"
	protoClass2DCCQueryCmd="$Class2DCCQueryCmd"
	protoClass2DISCmd="$Class2DISCmd"
	protoClass2LIDCmd="$Class2LIDCmd"
	protoClass2NRCmd="$Class2NRCmd"
	protoClass2PHCTOCmd="$Class2PHCTOCmd"
	protoClass2PIECmd="$Class2PIECmd"
	protoClass2RELCmd="$Class2RELCmd"
	protoClass2RecvDataTrigger="$Class2RecvDataTrigger"
	protoClass2SPLCmd="$Class2SPLCmd"
	protoClass2TBCCmd="$Class2TBCCmd"
	protoClass2XmitWaitForXON="$Class2XmitWaitForXON"
	;;
    esac
}

#
# Get the default modem parameter values
# from the prototype configuration file.
#
getModemProtoParameters()
{
    getParameter ModemAnswerAnyCmd $1;		ModemAnswerAnyCmd="$param"
    getParameter ModemAnswerBeginDataCmd $1;	ModemAnswerBeginDataCmd="$param"
    getParameter ModemAnswerBeginFaxCmd $1;	ModemAnswerBeginFaxCmd="$param"
    getParameter ModemAnswerBeginVoiceCmd $1;	ModemAnswerBeginVoiceCmd="$param"
    getParameter ModemAnswerCmd $1;		ModemAnswerCmd="$param"
    getParameter ModemAnswerDataCmd $1;		ModemAnswerDataCmd="$param"
    getParameter ModemAnswerFaxCmd $1;		ModemAnswerFaxCmd="$param"
    getParameter ModemAnswerResponseTimeout $1;	ModemAnswerResponseTimeout="$param"
    getParameter ModemAnswerVoiceCmd $1;	ModemAnswerVoiceCmd="$param"
    getParameter ModemBaudRateDelay $1;		ModemBaudRateDelay="$param"
    getParameter ModemCommaPauseTimeCmd $1;	ModemCommaPauseTimeCmd="$param"
    getParameter ModemDialCmd $1;		ModemDialCmd="$param"
    getParameter ModemDialResponseTimeout $1;	ModemDialResponseTimeout="$param"
    getParameter ModemEchoOffCmd $1;		ModemEchoOffCmd="$param"
    getParameter ModemFlowControl $1;		ModemFlowControl="$param"
    getParameter ModemFlowControlCmd $1;	ModemFlowControlCmd="$param"
    getParameter ModemFrameFillOrder $1;	ModemFrameFillOrder="$param"
    getParameter ModemHostFillOrder $1;		ModemHostFillOrder="$param"
    getParameter ModemInterPacketDelay $1;	ModemInterPacketDelay="$param"
    getParameter ModemMaxPacketSize $1;		ModemMaxPacketSize="$param"
    getParameter ModemMfrQueryCmd $1;		ModemMfrQueryCmd="$param"
    getParameter ModemModelQueryCmd $1;		ModemModelQueryCmd="$param"
    getParameter ModemNoAutoAnswerCmd $1;	ModemNoAutoAnswerCmd="$param"
    getParameter ModemOnHookCmd $1;		ModemOnHookCmd="$param"
    getParameter ModemPageDoneTimeout $1;	ModemPageDoneTimeout="$param"
    getParameter ModemPageStartTimeout $1;	ModemPageStartTimeout="$param"
    getParameter ModemRate $1;			ModemRate="$param"
    getParameter ModemRecvFillOrder $1;		ModemRecvFillOrder="$param"
    getParameter ModemResetCmds $1;		ModemResetCmds="$param"
    getParameter ModemResetDelay $1;		ModemResetDelay="$param"
    getParameter ModemResultCodesCmd $1;	ModemResultCodesCmd="$param"
    getParameter ModemRevQueryCmd $1;		ModemRevQueryCmd="$param"
    getParameter ModemSendFillOrder $1;		ModemSendFillOrder="$param"
    getParameter ModemSetVolumeCmd $1;		ModemSetVolumeCmd="$param"
    getParameter ModemSetupAACmd $1;		ModemSetupAACmd="$param"
    getParameter ModemSetupDCDCmd $1;		ModemSetupDCDCmd="$param"
    getParameter ModemSetupDTRCmd $1;		ModemSetupDTRCmd="$param"
    getParameter ModemSoftResetCmd $1;		ModemSoftResetCmd="$param"
    getParameter ModemVerboseResultsCmd $1;	ModemVerboseResultsCmd="$param"
    getParameter ModemWaitForConnect $1;	ModemWaitForConnect="$param"
    getParameter ModemWaitTimeCmd $1;		ModemWaitTimeCmd="$param"

    getParameter FaxT1Timer $1;			FaxT1Timer="$param"
    getParameter FaxT2Timer $1;			FaxT2Timer="$param"
    getParameter FaxT4Timer $1;			FaxT4Timer="$param"
    case "$ModemType" in
    Class1*)
	getParameter Class1Cmd $1;		Class1Cmd="$param"
	getParameter Class1FrameOverhead $1;	Class1FrameOverhead="$param"
	getParameter Class1RecvAbortOK $1;	Class1RecvAbortOK="$param"
	getParameter Class1RecvIdentTimer $1;	Class1RecvIdentTimer="$param"
	getParameter Class1SendPPMDelay $1;	Class1SendPPMDelay="$param"
	getParameter Class1SendTCFDelay $1;	Class1SendTCFDelay="$param"
	getParameter Class1TCFResponseDelay $1;	Class1TCFResponseDelay="$param"
	getParameter Class1TrainingRecovery $1;	Class1TrainingRecovery="$param"
	;;
    Class2*)
	getParameter Class2AbortCmd $1;		Class2AbortCmd="$param"
	getParameter Class2BORCmd $1;		Class2BORCmd="$param"
	getParameter Class2BUGCmd $1;		Class2BUGCmd="$param"
	getParameter Class2CIGCmd $1;		Class2CIGCmd="$param"
	getParameter Class2CQCmd $1;		Class2CQCmd="$param"
	getParameter Class2CRCmd $1;		Class2CRCmd="$param"
	getParameter Class2Cmd $1;		Class2Cmd="$param"
	getParameter Class2DCCCmd $1;		Class2DCCCmd="$param"
	getParameter Class2DCCQueryCmd $1;	Class2DCCQueryCmd="$param"
	getParameter Class2DISCmd $1;		Class2DISCmd="$param"
	getParameter Class2LIDCmd $1;		Class2LIDCmd="$param"
	getParameter Class2NRCmd $1;		Class2NRCmd="$param"
	getParameter Class2PHCTOCmd $1;		Class2PHCTOCmd="$param"
	getParameter Class2PIECmd $1;		Class2PIECmd="$param"
	getParameter Class2RELCmd $1;		Class2RELCmd="$param"
	getParameter Class2RecvDataTrigger $1;	Class2RecvDataTrigger="$param"
	getParameter Class2SPLCmd $1;		Class2SPLCmd="$param"
	getParameter Class2TBCCmd $1;		Class2TBCCmd="$param"
	getParameter Class2XmitWaitForXON $1;	Class2XmitWaitForXON="$param"
	;;
    esac

    #
    # If a dialing prefix was being used, add it to the
    # dialing command string.  Perhaps this is too simplistic.
    #
    if [ "$DialingPrefix" ]; then
	ModemDialCmd="${DialingPrefix}${ModemDialCmd}"
    fi
    saveModemProtoParameters
}

addSedCmd()
{
    test "$1" != "$2" && {
	# escape backslashes to counteract shell parsing and sed magic chars
	x=`echo "$2" | sed 's/[&\\\\"]/\\\\&/g'`
	if [ `expr "$2" : "[^\"].*[ ]"` = 0 ]; then
	    ModemCmds="$ModemCmds -e '/^$3:/s;\(:[ 	]*\).*;\1$x;'"
	else
	    ModemCmds="$ModemCmds -e '/^$3:/s;\(:[ 	]*\).*;\1\"$x\";'"
	fi
    }
}

#
# Setup the sed commands for crafting the configuration file:
#
makeSedModemCommands()
{
    ModemCmds=""

    addSedCmd "$protoModemAnswerAnyCmd" \
	"$ModemAnswerAnyCmd"			ModemAnswerAnyCmd
    addSedCmd "$protoModemAnswerBeginDataCmd" \
	"$ModemAnswerBeginDataCmd"		ModemAnswerBeginDataCmd
    addSedCmd "$protoModemAnswerBeginFaxCmd" \
	"$ModemAnswerBeginFaxCmd"		ModemAnswerBeginFaxCmd
    addSedCmd "$protoModemAnswerBeginVoiceCmd" \
	"$ModemAnswerBeginVoiceCmd"		ModemAnswerBeginVoiceCmd
    addSedCmd "$protoModemAnswerCmd" \
	"$ModemAnswerCmd"			ModemAnswerCmd
    addSedCmd "$protoModemAnswerDataCmd" \
	"$ModemAnswerDataCmd"			ModemAnswerDataCmd
    addSedCmd "$protoModemAnswerFaxCmd" \
	"$ModemAnswerFaxCmd"			ModemAnswerFaxCmd
    addSedCmd "$protoModemAnswerResponseTimeout" \
	"$ModemAnswerResponseTimeout"		ModemAnswerResponseTimeout
    addSedCmd "$protoModemAnswerVoiceCmd" \
	"$ModemAnswerVoiceCmd"			ModemAnswerVoiceCmd
    addSedCmd "$protoModemBaudRateDelay" \
	"$ModemBaudRateDelay"			ModemBaudRateDelay
    addSedCmd "$protoModemCommaPauseTimeCmd" \
	"$ModemCommaPauseTimeCmd"		ModemCommaPauseTimeCmd
    addSedCmd "$protoModemDialCmd" \
	"$ModemDialCmd"				ModemDialCmd
    addSedCmd "$protoModemDialResponseTimeout" \
	"$ModemDialResponseTimeout"		ModemDialResponseTimeout
    addSedCmd "$protoModemEchoOffCmd" \
	"$ModemEchoOffCmd"			ModemEchoOffCmd
    addSedCmd "$protoModemFlowControl" \
	"$ModemFlowControl"			ModemFlowControl
    addSedCmd "$protoModemFlowControlCmd" \
	"$ModemFlowControlCmd"			ModemFlowControlCmd
    addSedCmd "$protoModemFrameFillOrder" \
	"$ModemFrameFillOrder"			ModemFrameFillOrder
    addSedCmd "$protoModemHostFillOrder" \
	"$ModemHostFillOrder"			ModemHostFillOrder
    addSedCmd "$protoModemInterPacketDelay" \
	"$ModemInterPacketDelay"		ModemInterPacketDelay
    addSedCmd "$protoModemMaxPacketSize" \
	"$ModemMaxPacketSize"			ModemMaxPacketSize
    addSedCmd "$protoModemMfrQueryCmd" \
	"$ModemMfrQueryCmd"			ModemMfrQueryCmd
    addSedCmd "$protoModemModelQueryCmd" \
	"$ModemModelQueryCmd"			ModemModelQueryCmd
    addSedCmd "$protoModemNoAutoAnswerCmd" \
	"$ModemNoAutoAnswerCmd"			ModemNoAutoAnswerCmd
    addSedCmd "$protoModemOnHookCmd" \
	"$ModemOnHookCmd"			ModemOnHookCmd
    addSedCmd "$protoModemPageDoneTimeout" \
	"$ModemPageDoneTimeout"			ModemPageDoneTimeout
    addSedCmd "$protoModemPageStartTimeout" \
	"$ModemPageStartTimeout"		ModemPageStartTimeout
    addSedCmd "$protoModemRate" \
	"$ModemRate"				ModemRate
    addSedCmd "$protoModemRecvFillOrder" \
	"$ModemRecvFillOrder"			ModemRecvFillOrder
    addSedCmd "$protoModemResetCmds" \
	"$ModemResetCmds"			ModemResetCmds
    addSedCmd "$protoModemResetDelay" \
	"$ModemResetDelay"			ModemResetDelay
    addSedCmd "$protoModemResultCodesCmd" \
	"$ModemResultCodesCmd"			ModemResultCodesCmd
    addSedCmd "$protoModemRevQueryCmd" \
	"$ModemRevQueryCmd"			ModemRevQueryCmd
    addSedCmd "$protoModemSendFillOrder" \
	"$ModemSendFillOrder"			ModemSendFillOrder
    addSedCmd "$protoModemSetVolumeCmd" \
	"$ModemSetVolumeCmd"			ModemSetVolumeCmd
    addSedCmd "$protoModemSetupAACmd" \
	"$ModemSetupAACmd"			ModemSetupAACmd
    addSedCmd "$protoModemSetupDCDCmd" \
	"$ModemSetupDCDCmd"			ModemSetupDCDCmd
    addSedCmd "$protoModemSetupDTRCmd" \
	"$ModemSetupDTRCmd"			ModemSetupDTRCmd
    addSedCmd "$protoModemSoftResetCmd" \
	"$ModemSoftResetCmd"			ModemSoftResetCmd
    addSedCmd "$protoModemVerboseResultsCmd" \
	"$ModemVerboseResultsCmd"		ModemVerboseResultsCmd
    addSedCmd "$protoModemWaitForConnect" \
	"$ModemWaitForConnect"			ModemWaitForConnect
    addSedCmd "$protoModemWaitTimeCmd" \
	"$ModemWaitTimeCmd"			ModemWaitTimeCmd

    addSedCmd "$protoFaxT1Timer" "$FaxT1Timer"	FaxT1Timer
    addSedCmd "$protoFaxT2Timer" "$FaxT2Timer"	FaxT2Timer
    addSedCmd "$protoFaxT4Timer" "$FaxT4Timer"	FaxT4Timer
    case "$ModemType" in
    Class1*)
	addSedCmd "$protoClass1Cmd" "$Class1Cmd" Class1Cmd
	addSedCmd "$protoClass1FrameOverhead" \
	    "$Class1FrameOverhead"		Class1FrameOverhead
	addSedCmd "$protoClass1RecvAbortOK" \
	    "$Class1RecvAbortOK"		Class1RecvAbortOK
	addSedCmd "$protoClass1RecvIdentTimer" \
	    "$Class1RecvIdentTimer"		Class1RecvIdentTimer
	addSedCmd "$protoClass1SendPPMDelay" \
	    "$Class1SendPPMDelay"		Class1SendPPMDelay
	addSedCmd "$protoClass1SendTCFDelay" \
	    "$Class1SendTCFDelay"		Class1SendTCFDelay
	addSedCmd "$protoClass1TCFResponseDelay" \
	    "$Class1TCFResponseDelay"		Class1TCFResponseDelay
	addSedCmd "$protoClass1TrainingRecovery" \
	    "$Class1TrainingRecovery"		Class1TrainingRecovery
	;;
    Class2*)
	addSedCmd "$protoClass2AbortCmd" \
	    "$Class2AbortCmd"			Class2AbortCmd
	addSedCmd "$protoClass2BORCmd" \
	    "$Class2BORCmd"			Class2BORCmd
	addSedCmd "$protoClass2BUGCmd" \
	    "$Class2BUGCmd"			Class2BUGCmd
	addSedCmd "$protoClass2CIGCmd" \
	    "$Class2CIGCmd"			Class2CIGCmd
	addSedCmd "$protoClass2CQCmd" \
	    "$Class2CQCmd"			Class2CQCmd
	addSedCmd "$protoClass2CRCmd" \
	    "$Class2CRCmd"			Class2CRCmd
	addSedCmd "$protoClass2Cmd" \
	    "$Class2Cmd"			Class2Cmd
	addSedCmd "$protoClass2DCCCmd" \
	    "$Class2DCCCmd"			Class2DCCCmd
	addSedCmd "$protoClass2DCCQueryCmd" \
	    "$Class2DCCQueryCmd"		Class2DCCQueryCmd
	addSedCmd "$protoClass2DISCmd" \
	    "$Class2DISCmd"			Class2DISCmd
	addSedCmd "$protoClass2LIDCmd" \
	    "$Class2LIDCmd"			Class2LIDCmd
	addSedCmd "$protoClass2NRCmd" \
	    "$Class2NRCmd"			Class2NRCmd
	addSedCmd "$protoClass2PHCTOCmd" \
	    "$Class2PHCTOCmd"			Class2PHCTOCmd
	addSedCmd "$protoClass2PIECmd" \
	    "$Class2PIECmd"			Class2PIECmd
	addSedCmd "$protoClass2RELCmd" \
	    "$Class2RELCmd"			Class2RELCmd
	addSedCmd "$protoClass2RecvDataTrigger" \
	    "$Class2RecvDataTrigger"		Class2RecvDataTrigger
	addSedCmd "$protoClass2SPLCmd" \
	    "$Class2SPLCmd"			Class2SPLCmd
	addSedCmd "$protoClass2TBCCmd" \
	    "$Class2TBCCmd"			Class2TBCCmd
	addSedCmd "$protoClass2XmitWaitForXON" \
	    "$Class2XmitWaitForXON"		Class2XmitWaitForXON
	;;
    esac
}

#
# Check if the configured flow control scheme is
# consistent with the tty device being used.
#
checkFlowControlAgainstTTY()
{
    case "$ModemFlowControl" in
    xonxoff|XONXOFF)
	if [ "$TTY" = "ttyf${PORT}" -a -c /dev/ttym${PORT} ]; then
	    echo ""
	    echo "Warning, the modem is setup to use software flow control,"
	    echo "but the \"$TTY\" device is used with hardware flow control"
	    prompt "Do you want to use \"ttym${PORT}\" instead [yes]?"
	    read x
	    if [ -z "$x" -o "$x" = "y" -o "$x" = "yes" ]; then
		TTY="ttym${PORT}"
		DEVID="`echo $TTY | tr '/' '_'`"
		CONFIG=$CPATH.$DEVID
	    fi
	fi
	;;
    rtscts|RTSCTS)
	if [ "$TTY" = "ttym${PORT}" -a -c /dev/ttyf${PORT} ]; then
	    echo ""
	    echo "Warning, the modem is setup to use hardware flow control,"
	    echo "but the \"$TTY\" device does not honor the RTS/CTS signals."
	    prompt "Do you want to use \"ttyf${PORT}\" instead [yes]?"
	    read x
	    if [ -z "$x" -o "$x" = "y" -o "$x" = "yes" ]; then
		TTY="ttyf${PORT}"
		DEVID="`echo $TTY | tr '/' '_'`"
		CONFIG=$CPATH.$DEVID
	    fi
	fi
	;;
    esac
}

printIfNotNull()
{
    test "$2" && {
	x=`echo "$2" | sed 's/\\\\/&&/g'`	# escape backslashes to shell
	echo "$1	$x"
    }
}

#
# Print the current modem-related parameters.
#
printModemConfig()
{
    echo ""
    echo "The modem configuration parameters are:"
    echo ""
    printIfNotNull "ModemAnswerCmd:	"	"$ModemAnswerCmd"
    printIfNotNull "ModemAnswerAnyCmd:"		"$ModemAnswerAnyCmd"
    printIfNotNull "ModemAnswerDataCmd:"	"$ModemAnswerDataCmd"
    printIfNotNull "ModemAnswerFaxCmd:"		"$ModemAnswerFaxCmd"
    printIfNotNull "ModemAnswerVoiceCmd:"	"$ModemAnswerVoiceCmd"
    printIfNotNull "ModemAnswerBeginDataCmd:"	"$ModemAnswerBeginDataCmd"
    printIfNotNull "ModemAnswerBeginFaxCmd:"	"$ModemAnswerBeginFaxCmd"
    printIfNotNull "ModemAnswerBeginVoiceCmd:"	"$ModemAnswerBeginVoiceCmd"
    printIfNotNull "ModemAnswerResponseTimeout:" "$ModemAnswerResponseTimeout"
    printIfNotNull "ModemBaudRateDelay:"	"$ModemBaudRateDelay"
    printIfNotNull "ModemCommaPauseTimeCmd:"	"$ModemCommaPauseTimeCmd"
    printIfNotNull "ModemDialCmd:	"	"$ModemDialCmd"
    printIfNotNull "ModemDialResponseTimeout:"	"$ModemDialResponseTimeout"
    printIfNotNull "ModemEchoOffCmd:	"	"$ModemEchoOffCmd"
    printIfNotNull "ModemFlowControl:"		"$ModemFlowControl"
    printIfNotNull "ModemFlowControlCmd:"	"$ModemFlowControlCmd"
    printIfNotNull "ModemFrameFillOrder:"	"$ModemFrameFillOrder"
    printIfNotNull "ModemHostFillOrder:"	"$ModemHostFillOrder"
    printIfNotNull "ModemInterPacketDelay:"	"$ModemInterPacketDelay"
    printIfNotNull "ModemMaxPacketSize:"	"$ModemMaxPacketSize"
    printIfNotNull "ModemMfrQueryCmd:"		"$ModemMfrQueryCmd"
    printIfNotNull "ModemModelQueryCmd:"	"$ModemModelQueryCmd"
    printIfNotNull "ModemNoAutoAnswerCmd:"	"$ModemNoAutoAnswerCmd"
    printIfNotNull "ModemOnHookCmd:	"	"$ModemOnHookCmd"
    printIfNotNull "ModemPageDoneTimeout:"	"$ModemPageDoneTimeout"
    printIfNotNull "ModemPageStartTimeout:"	"$ModemPageStartTimeout"
    printIfNotNull "ModemRate:	"		"$ModemRate"
    printIfNotNull "ModemRecvFillOrder:"	"$ModemRecvFillOrder"
    printIfNotNull "ModemResetCmds:	"	"$ModemResetCmds"
    printIfNotNull "ModemResetDelay:"		"$ModemResetDelay"
    printIfNotNull "ModemResultCodesCmd:"	"$ModemResultCodesCmd"
    printIfNotNull "ModemRevQueryCmd:"		"$ModemRevQueryCmd"
    printIfNotNull "ModemSendFillOrder:"	"$ModemSendFillOrder"
    printIfNotNull "ModemSetVolumeCmd:"		"$ModemSetVolumeCmd"
    printIfNotNull "ModemSetupAACmd:"		"$ModemSetupAACmd"
    printIfNotNull "ModemSetupDCDCmd:"		"$ModemSetupDCDCmd"
    printIfNotNull "ModemSetupDTRCmd:"		"$ModemSetupDTRCmd"
    printIfNotNull "ModemSoftResetCmd:"		"$ModemSoftResetCmd"
    printIfNotNull "ModemVerboseResultsCmd:"	"$ModemVerboseResultsCmd"
    printIfNotNull "ModemWaitForConnect:"	"$ModemWaitForConnect"
    printIfNotNull "ModemWaitTimeCmd:"		"$ModemWaitTimeCmd"

    printIfNotNull "FaxT1Timer:	"		"$FaxT1Timer"
    printIfNotNull "FaxT2Timer:	"		"$FaxT2Timer"
    printIfNotNull "FaxT4Timer:	"		"$FaxT4Timer"
    case "$ModemType" in
    Class1*)
	printIfNotNull "Class1Cmd:	"	"$Class1Cmd"
	printIfNotNull "Class1FrameOverhead:"	"$Class1FrameOverhead"
	printIfNotNull "Class1RecvAbortOK:"	"$Class1RecvAbortOK"
	printIfNotNull "Class1RecvIdentTimer:"	"$Class1RecvIdentTimer"
	printIfNotNull "Class1SendPPMDelay:"	"$Class1SendPPMDelay"
	printIfNotNull "Class1SendTCFDelay:"	"$Class1SendTCFDelay"
	printIfNotNull "Class1TCFResponseDelay:" "$Class1TCFResponseDelay"
	printIfNotNull "Class1TrainingRecovery:" "$Class1TrainingRecovery"
	;;
    Class2*)
	printIfNotNull "Class2AbortCmd:"	"$Class2AbortCmd"
	printIfNotNull "Class2BORCmd:	"	"$Class2BORCmd"
	printIfNotNull "Class2BUGCmd:	"	"$Class2BUGCmd"
	printIfNotNull "Class2CIGCmd:	"	"$Class2CIGCmd"
	printIfNotNull "Class2CQCmd:	"	"$Class2CQCmd"
	printIfNotNull "Class2CRCmd:	"	"$Class2CRCmd"
	printIfNotNull "Class2Cmd:	"	"$Class2Cmd"
	printIfNotNull "Class2DCCCmd:	"	"$Class2DCCCmd"
	printIfNotNull "Class2DCCQueryCmd:"	"$Class2DCCQueryCmd"
	printIfNotNull "Class2DISCmd:	"	"$Class2DISCmd"
	printIfNotNull "Class2LIDCmd:	"	"$Class2LIDCmd"
	printIfNotNull "Class2NRCmd:	"	"$Class2NRCmd"
	printIfNotNull "Class2PHCTOCmd:	"	"$Class2PHCTOCmd"
	printIfNotNull "Class2PIECmd:	"	"$Class2PIECmd"
	printIfNotNull "Class2RELCmd:	"	"$Class2RELCmd"
	printIfNotNull "Class2RecvDataTrigger:"	"$Class2RecvDataTrigger"
	printIfNotNull "Class2SPLCmd:	"	"$Class2SPLCmd"
	printIfNotNull "Class2TBCCmd:	"	"$Class2TBCCmd"
	printIfNotNull "Class2XmitWaitForXON:"	"$Class2XmitWaitForXON"
	;;
    esac
    echo ""
}

#
# Prompt the user to edit the current parameters.  Note that
# we can only edit parameters that are in the prototype config
# file; thus all the checks to see if the prototype value exists.
#
promptForModemParameters()
{
    test "$protoModemAnswerCmd" && {
	promptForATCmdParameter "$ModemAnswerCmd" \
	    "Command for answering the phone"
	ModemAnswerCmd="$param";
    }
    test "$protoModemAnswerAnyCmd" && {
	promptForATCmdParameter "$ModemAnswerAnyCmd" \
	    "Command for answering any type of call"
	ModemAnswerAnyCmd="$param";
    }
    test "$protoModemAnswerDataCmd" && {
	promptForATCmdParameter "$ModemAnswerDataCmd" \
	    "Command for answering a data call"
	ModemAnswerDataCmd="$param";
    }
    test "$protoModemAnswerFaxCmd" && {
	promptForATCmdParameter "$ModemAnswerFaxCmd" \
	    "Command for answering a fax call"
	ModemAnswerFaxCmd="$param";
    }
    test "$protoModemAnswerVoiceCmd" && {
	promptForATCmdParameter "$ModemAnswerVoiceCmd" \
	    "Command for answering a voice call"
	ModemAnswerVoiceCmd="$param";
    }
    test "$protoModemAnswerBeginDataCmd" && {
	promptForATCmdParameter "$ModemAnswerBeginDataCmd" \
	    "Command for start of a data call"
	ModemAnswerBeginDataCmd="$param";
    }
    test "$protoModemAnswerBeginFaxCmd" && {
	promptForATCmdParameter "$ModemAnswerBeginFaxCmd" \
	    "Command for start of a fax call"
	ModemAnswerBeginFaxCmd="$param";
    }
    test "$protoModemAnswerBeginVoiceCmd" && {
	promptForATCmdParameter "$ModemAnswerBeginVoiceCmd" \
	    "Command for start of a voice call"
	ModemAnswerBeginVoiceCmd="$param";
    }
    test "$protoModemAnswerResponseTimeout" && {
	promptForNumericParameter "$ModemAnswerResponseTimeout" \
	    "Answer command response timeout (ms)"
	ModemAnswerResponseTimeout="$param";
    }
    test "$protoModemBaudRateDelay" && {
	promptForNumericParameter "$ModemBaudRateDelay" \
	    "Delay after setting tty baud rate (ms)"
	ModemBaudRateDelay="$param";
    }
    test "$protoModemCommaPauseTimeCmd" && {
	promptForATCmdParameter "$ModemCommaPauseTimeCmd" \
	    "Command for setting time to pause for \",\" in dialing string"
	ModemCommaPauseTimeCmd="$param";
    }
    test "$protoModemDialCmd" && {
	promptForATCmdParameter "$ModemDialCmd" \
	    "Command for dialing (%s for number to dial)"
	ModemDialCmd="$param";
    }
    test "$protoModemDialResponseTimeout" && {
	promptForNumericParameter "$ModemDialResponseTimeout" \
	    "Dialing command response timeout (ms)"
	ModemDialResponseTimeout="$param";
    }
    test "$protoModemEchoOffCmd" && {
	promptForATCmdParameter "$ModemEchoOffCmd" \
	    "Command for disabling command echo"
	ModemEchoOffCmd="$param";
    }
    test "$protoModemFlowControl" && {
	promptForFlowControlParameter "$ModemFlowControl" \
	    "DTE-DCE flow control scheme"
	ModemFlowControl="$param";
    }
    test "$protoModemFlowControlCmd" && {
	promptForATCmdParameter "$ModemFlowControlCmd" \
	    "Command for setting DCE-DTE flow control"
	ModemFlowControlCmd="$param";
    }
    test "$protoModemFrameFillOrder" && {
	promptForBitOrderParameter "$ModemFrameFillOrder" \
	    "Bit order for HDLC frames"
	ModemFrameFillOrder="$param";
    }
    test "$protoModemHostFillOrder" && {
	promptForBitOrderParameter "$ModemHostFillOrder" \
	    "Bit order of host"
	ModemHostFillOrder="$param";
    }
    test "$protoModemInterPacketDelay" && {
	promptForNumericParameter "$ModemInterPacketDelay" \
	    "Delay between modem writes (ms)"
	ModemInterPacketDelay="$param";
    }
    test "$protoModemMaxPacketSize" && {
	promptForNumericParameter "$ModemMaxPacketSize" \
	    "Maximum data to write to the modem at once (bytes)"
	ModemMaxPacketSize="$param";
    }
    test "$protoModemMfrQueryCmd" && {
	promptForATCmdParameter "$ModemMfrQueryCmd" \
	    "Command for querying modem manufacture"
	ModemMfrQueryCmd="$param";
    }
    test "$protoModemModelQueryCmd" && {
	promptForATCmdParameter "$ModemModelQueryCmd" \
	    "Command for querying modem model"
	ModemModelQueryCmd="$param";
    }
    test "$protoModemNoAutoAnswerCmd" && {
	promptForATCmdParameter "$ModemNoAutoAnswerCmd" \
	    "Command for disabling auto-answer"
	ModemNoAutoAnswerCmd="$param";
    }
    test "$protoModemOnHookCmd" && {
	promptForATCmdParameter "$ModemOnHookCmd" \
	    "Command for placing phone \"on hook\" (hangup)"
	ModemOnHookCmd="$param";
    }
    test "$protoModemPageDoneTimeout" && {
	promptForNumericParameter "$ModemPageDoneTimeout" \
	    "Page send/receive timeout (ms)"
	ModemPageDoneTimeout="$param";
    }
    test "$protoModemPageStartTimeout" && {
	promptForNumericParameter "$ModemPageStartTimeout" \
	    "Page send/receive timeout (ms)"
	ModemPageStartTimeout="$param";
    }
    test "$protoModemRate" && {
	promptForNumericParameter "$ModemRate" \
	    "DTE-DCE communication baud rate"
	ModemRate="$param";
    }
    test "$protoModemRecvFillOrder" && {
	promptForBitOrderParameter "$ModemRecvFillOrder" \
	    "Bit order that modem sends received facsimile data"
	ModemRecvFillOrder="$param";
    }
    test "$protoModemResetCmds" && {
	promptForATCmdParameter "$ModemResetCmds" \
	    "Additional commands for resetting the modem"
	ModemResetCmds="$param";
    }
    test "$protoModemResetDelay" && {
	promptForNumericParameter "$ModemResetDelay" \
	    "Delay after sending modem reset commands (ms)"
	ModemResetDelay="$param";
    }
    test "$protoModemResultCodesCmd" && {
	promptForATCmdParameter "$ModemResultCodesCmd" \
	    "Command for enabling result codes"
	ModemResultCodesCmd="$param";
    }
    test "$protoModemRevQueryCmd" && {
	promptForATCmdParameter "$ModemRevQueryCmd" \
	    "Command for querying modem firmware revision"
	ModemRevQueryCmd="$param";
    }
    test "$protoModemSendFillOrder" && {
	promptForBitOrderParameter "$ModemSendFillOrder" \
	    "Bit order that modem expects for transmitted facsimile data"
	ModemSendFillOrder="$param";
    }
    test "$protoModemSetVolumeCmd" && {
	promptForATCmdParameter "$ModemSetVolumeCmd" \
	    "Commands for setting modem speaker volume levels"
	ModemSetVolumeCmd="$param";
    }
    test "$protoModemSetupAACmd" && {
	promptForATCmdParameter "$ModemSetupAACmd" \
	    "Command for setting up adaptive-answer"
	ModemSetupAACmd="$param";
    }
    test "$protoModemSetupDCDCmd" && {
	promptForATCmdParameter "$ModemSetupDCDCmd" \
	    "Command for setting up DCD handling"
	ModemSetupDCDCmd="$param";
    }
    test "$protoModemSetupDTRCmd" && {
	promptForATCmdParameter "$ModemSetupDTRCmd" \
	    "Command for setting up DTR handling"
	ModemSetupDTRCmd="$param";
    }
    test "$protoModemSoftResetCmd" && {
	promptForATCmdParameter "$ModemSoftResetCmd" \
	    "Command for doing a soft reset"
	ModemSoftResetCmd="$param";
    }
    test "$protoModemVerboseResultsCmd" && {
	promptForATCmdParameter "$ModemVerboseResultsCmd" \
	    "Command for enabling verbose result codes"
	ModemVerboseResultsCmd="$param";
    }
    test "$protoModemWaitForConnect" && {
	promptForBooleanParameter "$ModemWaitForConnect" \
	    "Force server to wait for \"CONNECT\" response on answer"
	ModemWaitForConnect="$param";
    }
    test "$protoModemWaitTimeCmd" && {
	promptForATCmdParameter "$ModemWaitTimeCmd" \
	    "Command for setting time to wait for carrier when dialing"
	ModemWaitTimeCmd="$param";
    }

    test "$protoFaxT1Timer" && {
	promptForATCmdParameter "$FaxT1Timer" "CCITT T.30 T1 timer (ms)"
	FaxT1Timer="$param";
    }
    test "$protoFaxT2Timer" && {
	promptForATCmdParameter "$FaxT2Timer" "CCITT T.30 T2 timer (ms)"
	FaxT2Timer="$param";
    }
    test "$protoFaxT4Timer" && {
	promptForATCmdParameter "$FaxT4Timer" "CCITT T.30 T4 timer (ms)"
	FaxT4Timer="$param";
    }
    case "$ModemType" in
    Class1*)
	test "$protoClass1Cmd" && {
	    promptForATCmdParameter "$Class1Cmd" "Command to enter Class 1"
	    Class1Cmd="$param";
	}
	test "$protoClass1FrameOverhead" && {
	    promptForNumericParameter "$Class1FrameOverhead" \
		"Extra bytes in a received HDLC frame"
	    Class1FrameOverhead="$param";
	}
	test "$protoClass1RecvAbortOK" && {
	    promptForNumericParameter "$Class1RecvAbortOK" \
		"Maximum time to wait for \"OK\" after aborting a receive (ms)"
	    Class1RecvAbortOK="$param";
	}
	test "$protoClass1RecvIdentTimer" && {
	    promptForNumericParameter "$Class1RecvIdentTimer" 
		"Maximum wait for initial identification frame (ms)"
	    Class1RecvIdentTimer="$param";
	}
	test "$protoClass1SendPPMDelay" && {
	    promptForNumericParameter "$Class1SendPPMDelay" \
		"Delay before sending post-page message (ms)"
	    Class1SendPPMDelay="$param";
	}
	test "$protoClass1SendTCFDelay" && {
	    promptForNumericParameter "$Class1SendTCFDelay"  \
		"Delay between sending TCF and ack/nak (ms)"
	    Class1SendTCFDelay="$param";
	}
	test "$protoClass1TCFResponseDelay" && {
	    promptForNumericParameter "$Class1TCFResponseDelay" \
		"Delay before sending DCS and TCF (ms)"
	    Class1TCFResponseDelay="$param";
	}
	test "$protoClass1TrainingRecovery" && {
	    promptForNumericParameter "$Class1TrainingRecovery" \
		"Delay after failed training (ms)"
	    Class1TrainingRecovery="$param";
	}
	;;
    Class2*)
	test "$protoClass2Cmd" && {
	    promptForATCmdParameter "$Class2Cmd" \
		"Command to enter Class $ModemType"
	    Class2Cmd="$param";
	}
	test "$protoClass2AbortCmd" && {
	    promptForATCmdParameter "$Class2AbortCmd" \
	        "Command to abort an active session"
	    Class2AbortCmd="$param";
	}
	test "$protoClass2BORCmd" && {
	    promptForATCmdParameter "$Class2BORCmd" \
		"Command to setup data bit order"
	    Class2BORCmd="$param";
	}
	test "$protoClass2BUGCmd" && {
	    promptForATCmdParameter "$Class2BUGCmd" \
		"Command to enable HDLC frame tracing"
	    Class2BUGCmd="$param";
	}
	test "$protoClass2CIGCmd" && {
	    promptForATCmdParameter "$Class2CIGCmd" \
		"Command to set polling identifer"
	    Class2CIGCmd="$param";
	}
	test "$protoClass2CQCmd" && {
	    promptForATCmdParameter "$Class2CQCmd" \
		"Command to setup copy quality parameters"
	    Class2CQCmd="$param";
	}
	test "$protoClass2CRCmd" && {
	    promptForATCmdParameter "$Class2CRCmd" \
		"Command to enable receive capability"
	    Class2CRCmd="$param";
	}
	test "$protoClass2DCCCmd" && {
	    promptForATCmdParameter "$Class2DCCCmd" \
		"Command to set/constrain modem capabilities"
	    Class2DCCCmd="$param";
	}
	test "$protoClass2DCCQueryCmd" && {
	    promptForATCmdParameter "$Class2DCCQueryCmd" \
		"Command to query modem capabilities"
	    Class2DCCQueryCmd="$param";
	}
	test "$protoClass2DISCmd" && {
	    promptForATCmdParameter "$Class2DISCmd" \
		"Command to set session parameters"
	    Class2DISCmd="$param";
	}
	test "$protoClass2LIDCmd" && {
	    promptForATCmdParameter "$Class2LIDCmd" \
		"Command to set local identifier string"
	    Class2LIDCmd="$param";
	}
	test "$protoClass2NRCmd" && {
	    promptForATCmdParameter "$Class2NRCmd" \
		"Command to enable status reporting"
	    Class2NRCmd="$param";
	}
	test "$protoClass2PHCTOCmd" && {
	    promptForATCmdParameter "$Class2PHCTOCmd" \
		"Command to set Phase C timeout"
	    Class2PHCTOCmd="$param";
	}
	test "$protoClass2PIECmd" && {
	    promptForATCmdParameter "$Class2PIECmd" \
		"Command to disable procedure interrupt handling"
	    Class2PIECmd="$param";
	}
	test "$protoClass2RELCmd" && {
	    promptForATCmdParameter "$Class2RELCmd" \
		"Command to enable delivery of byte-aligned EOL codes"
	    Class2RELCmd="$param";
	}
	test "$protoClass2RecvDataTrigger" && {
	    promptForATCmdParameter "$Class2RecvDataTrigger" \
		"Character sent before receiving page data";
	    Class2RecvDataTrigger="$param";
	}
	test "$protoClass2SPLCmd" && {
	    promptForATCmdParameter "$Class2SPLCmd" \
		"Command to set polling request"
	    Class2SPLCmd="$param";
	}
	test "$protoClass2TBCCmd" && {
	    promptForATCmdParameter "$Class2TBCCmd" \
		"Command to enable DTE-DCE stream communication mode"
	    Class2TBCCmd="$param";
	}
	test "$protoClass2XmitWaitForXON" && {
	    promptForBooleanParameter "$Class2XmitWaitForXON" \
		"Wait for XON before sending page data"
	    Class2XmitWaitForXON="$param";
	}
	;;
    esac
}

#
# Construct the configuration file.
#
if [ $ProtoType = config.skel	\
  -o $ProtoType = config.class1	\
  -o $ProtoType = config.class2	\
]; then
    # Go through each important parameter (sigh)
cat<<EOF

There is no prototype configuration file for your modem, so we will
have to fill in the appropriate parameters by hand.  You will need the
manual for how to program your modem to do this task.  In case you are
uncertain of the meaning of a configuration parameter you should
consult the config(4F) manual page for an explanation.

Note that modem commands are specified without a leading "AT".  The "AT"
will not be displayed in the prompts and if you include a leading "AT"
it will automatically be deleted.  Likewise quote marks (") will not be
displayed and will automatically be deleted.  You can use this facility
to supply null parameters as "".

Finally, beware that the set of parameters is long.  If you prefer to
use your favorite editor instead of this script you should fill things
in here as best you can and then edit the configuration file

"$CONFIG"

after completing this procedure.

EOF
    ok="no"
else
    ok="skip"
fi

getModemProtoParameters $proto
while [ "$ok" != "" -a "$ok" != "y" -a "$ok" != "yes" ]; do
    if [ "$ok" != "skip" ]; then
	promptForModemParameters
    fi
    printModemConfig
    if [ "$OS" = "IRIX" ]; then
	checkFlowControlAgainstTTY
    fi
    #
    # XXX not sure what kind of consistency checking that can
    # done w/o knowing more about the modem...
    #
    prompt "Are these ok [yes]?"; read ok
done
makeSedModemCommands

#
# Re-read the server-related prototype parameters from
# the prototype file that we know we're going to hand to
# sed and construct the sed substitute commands that
# transform things according to the typed-in values.
#
getServerProtoParameters $proto
#
# If getty use was previously enabled, try to sed any
# existing GettyArgs line in the prototype file.  Otherwise
# try to uncomment the GettyArgs line in the prototype
# file--though this is not likely to do the right thing (sigh).
#
if [ "$protoGettyArgs" ]; then
    addServerSedCmd "$protoGettyArgs" "$GettyArgs" GettyArgs
else
    ServerCmds="$ServerCmds -e '/GettyArgs:/s/^#//'"
fi
makeSedServerCommands

#
# All done with the prompting; edit up a config file!
#
if [ -f $CONFIG ]; then
    echo "Saving existing configuration file as \"$CONFIG.sav\"."
    mv $CONFIG $CONFIG.sav
fi
echo "Creating new configuration file \"$CONFIG\"."
eval sed "$ServerCmds" "$ModemCmds" '-e /CONFIG:/d' $proto >$CONFIG
chown $FAX $CONFIG; chgrp $faxGID $CONFIG; chmod 644 $CONFIG

#
# Create FIFO.<tty> special file.
#
FIFO=$SPOOL/FIFO.$DEVID
echo "Creating \"$FIFO\" in the spooling directory."
test -p $FIFO \
    || (mkfifo $FIFO) >/dev/null 2>&1 \
    || (mknod $FIFO p) >/dev/null 2>&1 \
    || { echo "Cannot create fifo \"$FIFO\""; exit 1; }
chown $FAX $FIFO; chgrp $faxGID $FIFO; chmod 600 $FIFO

echo "Done setting up the modem configuration."

#
# And, last but not least, startup a server for the modem.
#
echo ""
prompt "Startup a facsimile server for this modem [yes]?"; read x
if [ "$x" = "" -o "$x" = "yes" -o "$x" = "y" ]; then
    echo "$SERVERDIR/faxd -m /dev/$TTY&"
    $SERVERDIR/faxd -m /dev/$TTY&
fi
exec >/dev/null 2>&1
