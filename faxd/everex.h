#ident $Header: /d/sam/flexkit/fax/faxd/RCS/everex.h,v 1.2 91/05/23 12:26:53 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _everex_
#define	_everex_

/*
 * Everex FAX Modem Definitions.
 */

// fax configuration (#S2 register)
#define	S2_HOSTCTL	0x00		// host control
#define	S2_MODEMCTL	0x01		// modem control
#define	S2_DEFMSGSYS	0x00		// default message system
#define	S2_ALTMSGSYS	0x04		// alternate message system
#define	S2_FLOWATBUF	0x00		// xon/xoff at buffer transitions
#define	S2_FLOW1SEC	0x08		// xon/xoff once a second
#define	S2_19200	0x00		// 19.2Kb T4 host-modem link speed
#define	S2_9600		0x10		// 9.6Kb T4 host-modem link speed
#define	S2_PADEOLS	0x00		// byte align and zero stuff eols
#define	S2_RAWDATA	0x20		// pass data unaltered
#define	S2_RESET	0x80		// reset detected by host

// high speed carrier select values (#S4 register)
#define	S4_GROUP2	0		// Group 2
#define	S4_2400V27	1		// V.27 2400
#define	S4_4800V27	2		// V.27 4800
#define	S4_4800V29	3		// V.29 4800
#define	S4_7200V29	4		// V.29 7200
#define	S4_9600V29	5		// V.29 9600

#endif /* _everex_ */
