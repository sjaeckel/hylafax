/*	$Id$ */
/*
 * Copyright (c) 1990-1996 Sam Leffler
 * Copyright (c) 1991-1996 Silicon Graphics, Inc.
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
#ifndef _class2_
#define	_class2_
/*
 * Fax Modem Definitions for:
 *
 * Class 2	(nominally SP-2388-A of August 30, 1991)
 * Class 2.0	(TIA/EIA-592)
 * T.class2	(ITU-T)
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
const int SERVICE_DATA	 = BIT(0);	// data service
const int SERVICE_CLASS1 = BIT(1);	// class 1 interface
const int SERVICE_CLASS2 = BIT(2);	// class 2 interface
const int SERVICE_CLASS20 = BIT(3);	// class 2.0 interface
const int SERVICE_CLASS10 = BIT(4);	// class 1.0 interface
const int SERVICE_CLASS21 = BIT(5);	// class 2.1 interface
const int SERVICE_VOICE	 = BIT(8);	// voice service (ZyXEL extension)
const int SERVICE_ALL	 = BIT(9)-1;

// t.30 session subparameter codes
// NB: only the first two are used
const int VR_NORMAL	= 0;		// 98 lpi
const int VR_FINE	= 1;		// 196 lpi
const int VR_R8		= 2;		// R8  x 15.4 l/mm
const int VR_R16	= 4;		// R16 x 15.4 l/mm
const int VR_200X100	= 8;		// 200 dpi x 100 l/25.4mm
const int VR_200X200	= 10;		// 200 dpi x 200 l/25.4mm
const int VR_200X400	= 20;		// 200 dpi x 400 l/25.4mm
const int VR_300X300	= 40;		// 300 dpi x 300 l/25.4mm
const int VR_ALL	= BIT(VR_FINE+1)-1;

const int BR_2400	= 0;		// 2400 bit/s
const int BR_4800	= 1;		// 4800 bit/s
const int BR_7200	= 2;		// 7200 bit/s
const int BR_9600	= 3;		// 9600 bit/s
const int BR_12000	= 4;		// 12000 bit/s
const int BR_14400	= 5;		// 14400 bit/s
const int BR_ALL	= BIT(BR_14400+1)-1;

const int WD_1728	= 0;		// 1728 pixels in 215 mm
const int WD_2048	= 1;		// 2048 pixels in 255 mm
const int WD_2432	= 2;		// 2432 pixels in 303 mm
const int WD_1216	= 3;		// 1216 pixels in 151 mm
const int WD_864	= 4;		// 864 pixels in 107 mm
const int WD_ALL	= BIT(WD_864+1)-1;

const int LN_A4		= 0;		// A4, 297 mm
const int LN_B4		= 1;		// B4, 364 mm
const int LN_INF	= 2;		// Unlimited length
const int LN_ALL	= BIT(LN_INF+1)-1;

const int LN_LET	= 3;		// XXX US Letter size (used internally)

const int DF_1DMR	= 0;		// 1-D Modified Huffman
const int DF_2DMR	= 1;		// 2-D Modified Huffman
const int DF_2DMRUNCOMP	= 2;		// 2-D Uncompressed Mode
const int DF_2DMMR	= 3;		// 2-D Modified Modified Read
const int DF_ALL	= BIT(DF_2DMMR+1)-1;

const int EC_DISABLE	= 0;		// disable ECM
const int EC_ENABLE	= 1;		// enable T.30 Annex A, ECM
const int EC_ECLHALF	= 2;		// enable T.30 Annex C, half duplex
const int EC_ECLFULL	= 3;		// enable T.30 Annex C, full duplex
const int EC_ALL	= 0x3;

const int BF_DISABLE	= 0;		// disable file transfer modes
const int BF_ENABLE	= 1;		// select BFT, T.434
const int BF_DTM	= 2;		// select Document Transfer Mode
const int BF_EDI	= 4;		// select Edifact Mode
const int BF_BTM	= 8;		// select Basic Transfer Mode
const int BF_CM		= 10;		// select character mode T.4 Annex D
const int BF_MM		= 20;		// select Mixed mode, T.4 Annex E
const int BF_PM		= 40;		// select Processable mode, T.505
const int BF_ALL	= 0x3;

const int ST_0MS	= 0;		// scan time/line: 0 ms/0 ms
const int ST_5MS	= 1;		// scan time/line: 5 ms/5 ms
const int ST_10MS2	= 2;		// scan time/line: 10 ms/5 ms
const int ST_10MS	= 3;		// scan time/line: 10 ms/10 ms
const int ST_20MS2	= 4;		// scan time/line: 20 ms/10 ms
const int ST_20MS	= 5;		// scan time/line: 20 ms/20 ms
const int ST_40MS2	= 6;		// scan time/line: 40 ms/20 ms
const int ST_40MS	= 7;		// scan time/line: 40 ms/40 ms
const int ST_ALL	= BIT(ST_40MS+1)-1;

// post page message codes
const int PPM_MPS	= 0;		// another page next, same document
const int PPM_EOM	= 1;		// another document next
const int PPM_EOP	= 2;		// no more pages or documents
const int PPM_PRI_MPS	= 4;		// another page, procedure interrupt
const int PPM_PRI_EOM	= 5;		// another doc, procedure interrupt
const int PPM_PRI_EOP	= 6;		// all done, procedure interrupt

// post page response codes
const int PPR_MCF	= 1;		// page good
const int PPR_RTN	= 2;		// page bad, retrain requested
const int PPR_RTP	= 3;		// page good, retrain requested
const int PPR_PIN	= 4;		// page bad, interrupt requested
const int PPR_PIP	= 5;		// page good, interrupt requested

// important stream transfer codes
const int DLE = 16;		// transparent character escape
const int SUB = 26;		// <DLE><SUB> => <DLE><DLE> for Class 2.0
const int ETX = 3;		// <DLE><ETX> means end of transfer
const int DC1 = 17;		// start data transfer (Class 2)
const int DC2 = 18;		// start data transfer (Class 2.0 and ZyXEL)
const int CAN = 24;		// abort data transfer
#endif /* _class2_ */
