#ident $Header: /d/sam/flexkit/fax/faxd/RCS/FaxTrace.h,v 1.2 91/05/23 12:26:14 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _FaxTrace_
#define	_FaxTrace_
const int FAXTRACE_SERVER	= 0x01;		// server operation
const int FAXTRACE_PROTOCOL	= 0x02;		// fax protocol
const int FAXTRACE_MODEMOPS	= 0x04;		// modem operations
const int FAXTRACE_MODEMCOM	= 0x08;		// modem communication
const int FAXTRACE_TIMEOUTS	= 0x10;		// all timeouts
const int FAXTRACE_ANY		= 0xffffffff;
#endif _FaxTrace_
