/*	$Header: /usr/people/sam/fax/util/RCS/config.h,v 1.56 1994/07/01 23:55:17 sam Exp $ */
/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Sam Leffler
 * Copyright (c) 1991, 1992, 1993, 1994 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */
#ifndef _CONFIG_
#define	_CONFIG_

/*
 * Spooling configuration definitions.
 * The master spooling directory is broken up into several
 * subdirectories to isolate information that should be
 * protected (e.g. documents) and to minimize the number
 * of files in a single directory (e.g. the send queue).
 */
#define	FAX_SPOOLDIR	"/var/spool/fax"
#define FAX_LIBEXEC	"/usr/local/lib/fax"	/* place for lib executables */
#define	FAX_LIBDATA	"/usr/local/lib/fax"	/* place for lib data files */

#define	FAX_USER	"fax"		/* account name of the ``fax user'' */
#define	FAX_SERVICE	"fax"		/* client-server service name */
#define	FAX_PROTONAME	"tcp"		/* protocol used by client+server */
#define	FAX_DEFPORT	4557		/* port to use if service is unknown */
#define	FAX_DEFHOST	"localhost"	/* default host for service */

#define	FAX_SEQF	"sendq/seqf"	/* send sequencing info */
#define	FAX_CONFIG	"etc/config"	/* master configuration file */
#define	FAX_XFERLOG	"etc/xferlog"	/* send/recv log file */
#define	FAX_PERMFILE	"etc/hosts"	/* send permission file */
#define	FAX_ETCDIR	"etc"		/* subdir for configuration files  */
#define	FAX_RECVDIR	"recvq"		/* subdir for received facsimiles  */
#define	FAX_SENDDIR	"sendq"		/* subdir for send description files */
#define	FAX_DOCDIR	"docq"		/* subdir for documents to send */
#define	FAX_TMPDIR	"tmp"		/* subdir for temp copies of docs */
#define	FAX_INFODIR	"info"		/* subdir for remote machine info */
#define	FAX_CTLDIR	"cinfo"		/* subdir for remote machine ctl info */
#define	FAX_LOGDIR	"log"		/* subdir for log files */
#define	FAX_STATUSDIR	"status"	/* subdir for server status files */
#define FAX_CONFIGPREF	"config"	/* prefix for local config files */
#define FAX_REMOTEPREF	"remote"	/* prefix for remote config files */
#define FAX_QFILEPREF	"sendq/q"	/* prefix for queue file */
#define	FAX_SHUTDOWNSUF	"shut"		/* suffix for shutdown status files */
#define	FAX_INFOSUF	"info"		/* suffix for server info files */

#define	FAX_FIFO	"FIFO"		/* FIFO file for talking to daemon */
#define	MODEM_ANY	"any"		/* any modem acceptable identifier */
#define	FAX_PROTOVERS	1		/* client-server protocol version */

/* NB: all times are given in seconds */
#define	FAX_REQBUSY	(5*60)		/* requeue interval on busy answer */
#define	FAX_REQPROTO	(1*60)		/* requeue interval on protocol error */
#define	FAX_REQUEUE	(15*60)		/* requeue interval on other */
#define	FAX_TTL		(24*60*60)	/* default time to live for a send */
#define	FAX_RETRIES	-1		/* number times to retry send */
#define	FAX_TIMEOUT	"now + 1 day"	/* default job timeout (at syntax) */
#define	FAX_DEFVRES	98		/* default vertical resolution */
					/* default is no email notification */
#define	FAX_DEFNOTIFY	SendFaxClient::no_notice
#define	FAX_DEFPRIORITY	0		/* default job priority */

#ifndef FAX_TRUSTED
/* set of trusted users for setting from address when sending */
#define	FAX_TRUSTED	"daemon", "uucp", "fax"
#endif

#define	UUCP_LCKTIMEOUT	(3*60*60)	/* UUCP lock auto-expiration (secs) */
#define	UUCP_PIDDIGITS	10		/* # digits to write to lock file */
#ifdef svr4
#define	UUCP_LOCKPREFIX	"LK."		/* file name is <prefix><device> */
#else
#define	UUCP_LOCKPREFIX	"LCK.."		/* file name is <prefix><device> */
#endif
#define	DEV_PREFIX	"/dev/"		/* prefix to strip from special files */

#ifndef LOG_FAX
#define	LOG_FAX		LOG_DAEMON	/* logging identity */
#endif

#ifndef HOST_FILLORDER
#define	HOST_FILLORDER	FILLORDER_MSB2LSB	/* CPU bit order */
#endif

#define	FAX_TYPERULES	"typerules"	/* file type and conversion rules */
#define	FAX_DIALRULES	"dialrules"	/* client dialstring conversion rules */
#define	FAX_PAGESIZES	"pagesizes"	/* page size database */
#define	FAX_COVER	"faxcover.ps"	/* prototype cover sheet file */
#define	FAX_NOTIFYCMD	"bin/notify"	/* command to do job notification */
#define	FAX_TRANSCMD	"bin/transcript"/* command to return transcript */
#define	FAX_FAXRCVDCMD	"bin/faxrcvd"	/* command to process a received fax */
#define	FAX_POLLRCVDCMD	"bin/pollrcvd"	/* command to process a received fax */
#define FAX_PS2FAXCMD	"bin/ps2fax"	/* command to convert postscript */
#define	FAX_COVERCMD	"bin/mkcover"	/* command to make cont. cover page */
#define	FAX_WEDGEDCMD	"bin/wedged"	/* command to deal with wedged modem */
#endif
