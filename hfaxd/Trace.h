/*	$Id: Trace.h,v 1.11 1996/06/24 03:01:48 sam Rel $ */
/*
 * Copyright (c) 1995-1996 Sam Leffler
 * Copyright (c) 1995-1996 Silicon Graphics, Inc.
 * HylaFAX is a trademark of Silicon Graphics
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
#ifndef _Trace_
#define	_Trace_
/*
 * Server Tracing Definitions.
 */
const int TRACE_SERVER		= 0x00001;	// server operation
const int TRACE_PROTOCOL	= 0x00002;	// fax protocol
const int TRACE_INXFERS		= 0x00004;	// inbound file xfers
const int TRACE_OUTXFERS	= 0x00008;	// outbound file xfers
const int TRACE_LOGIN		= 0x00010;	// all user logins
const int TRACE_CONNECT		= 0x00020;	// network connections
const int TRACE_FIFO		= 0x00040;	// FIFO messages
const int TRACE_TIFF		= 0x00080;	// TIFF library
const int TRACE_CONFIG		= 0x00100;	// config file processing
const int TRACE_ANY		= 0xfffff;

#define	TRACE(x)	((tracingLevel & TRACE_##x) != 0)

const int TRACE_MASK		= 0xfffff;
#endif /* _Trace_ */
