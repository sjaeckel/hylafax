#ident $Header: /usr/people/sam/flexkit/fax/util/RCS/config.h,v 1.13 91/06/11 15:17:07 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
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
#define	FAX_SPOOLDIR	"/usr/spool/fax"
#define FAX_BINDIR	"/usr/local/bin/fax"	/* default place for apps */
#define FAX_FILTERDIR	"/usr/local/bin/fax"	/* default place for filters */

#define	FAX_USER	"fax"		/* account name of the ``fax user'' */

#define	FAX_SEQF	"sendq/seqf"	/* send sequencing info */
#define	FAX_CONFIG	"etc/config"	/* master configuration file */
#define	FAX_XFERLOG	"etc/xferlog"	/* send/recv log file */
#define	FAX_PERMFILE	"etc/hosts"	/* send permission file */
#define	FAX_TSIFILE	"etc/tsi"	/* receive permission file */
#define	FAX_ETCDIR	"etc"		/* subdir for configuration files  */
#define	FAX_RECVDIR	"recvq"		/* subdir for received facsimiles  */
#define	FAX_SENDDIR	"sendq"		/* subdir for send description files */
#define	FAX_DOCDIR	"docq"		/* subdir for documents to send */
#define	FAX_TMPDIR	"tmp"		/* subdir for temp copies of docs */
#define	FAX_INFODIR	"info"		/* subdir for remote machine info */
#define FAX_CONFIGPREF	"config"	/* prefix for local config files */
#define FAX_REMOTEPREF	"remote"	/* prefix for remote config files */
#define FAX_QFILEPREF	"sendq/q"	/* prefix for queue file */

#define	FAX_FIFO	"FIFO"		/* FIFO file for talking to daemon */

#define	FAX_REQUEUE	(15*60)		/* requeue interval (seconds) */
#define	FAX_TTL		(24*60*60)	/* default time to live for a send */

#define	FAX_COVER	"faxcover.ps"	/* prototype cover sheet file */
#define	FAX_DELIVERCMD	"bin/deliver"	/* command to deliver a received fax */
#define	FAX_NOTIFYCMD	"bin/notify"	/* command to process a received fax */
#define	FAX_MAILCMD	"/bin/mail"	/* command for mailing notify msgs */
#define FAX_ATCMD	"bin/submit2at"	/* command to get job id from at */
#define FAX_ATRM	"/usr/bin/at -r "/* command to remove at job */
#define FAX_RM		"bin/faxrm"	/* command to remove fax job */
#define FAX_SUBMIT	"bin/faxsubmit"	/* command to really submit fax job */
#define FAX_PS2FAX	"bin/ps2fax"	/* command to convert postscript */
/* the following commands are prefixed by FAX_BINDIR */
#define	FAX_VIEWER	"faxview -s"	/* command to view a fax */
#define	FAX_SENDFAX	"sendfax"	/* command to send a fax */
#define	FAX_COMPFAX	"faxcomp"	/* command to compose a fax */
#define	FAX_PRINTFAX	"fax2ps | lp"	/* command to print a fax */
#endif
