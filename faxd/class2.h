#ident $Header: /d/sam/flexkit/fax/faxd/RCS/class2.h,v 1.3 91/05/23 12:26:50 sam Exp $
/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _class2_
#define	_class2_

/*
 * Class 2 Fax Modem Definitions.
 */
#define	BIT(i)	(1<<(i))

// bit ordering directives +fbor=<n>
const int BOR_C_DIR	= 0;		// phase C direct
const int BOR_C_REV	= 1;		// phase C reversed
const int BOR_C		= 0x1;
const int BOR_BD_DIR	= (0<<1);	// phase B/D direct
const int BOR_BD_REV	= (1<<1);	// phase B/D reversed
const int BOR_BD	= 0x2;

// service types returned by +fclass=?
const int SERVICE_DATA	 = BIT(0);	// data modem
const int SERVICE_CLASS1 = BIT(1);	// class 1 interface
const int SERVICE_CLASS2 = BIT(2);	// class 2 interface

// post page message codes
const int PPM_MPS	= 0;		// another page next, same document
const int PPM_EOM	= 1;		// another document next
const int PPM_EOP	= 2;		// no more pages or documents
const int PPM_PRI_MPS	= 3;		// another page, procedure interrupt
const int PPM_PRI_EOM	= 4;		// another doc, procedure interrupt
const int PPM_PRI_EOP	= 5;		// all done, procedure interrupt

// post page response codes
const int PPR_MCF	= 1;		// page good
const int PPR_RTN	= 2;		// page bad, retrain requested
const int PPR_RTP	= 3;		// page good, retrain requested
const int PPR_PIN	= 4;		// page bad, interrupt requested
const int PPR_PIP	= 5;		// page good, interrupt requested

// important stream transfer codes
const int DLE = 16;
const int ETX = 3;
const int DC1 = 17;
#endif /* _class2_ */
