#ident $Header: /d/sam/flexkit/fax/faxd/RCS/t.30.h,v 1.2 91/05/23 12:27:04 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _t30_
#define	_t30_

/*
 * The Group 3/T.30 protocol defines pre-, in-, and post-
 * message phases.  During the pre-message phase, the modems
 * exchange capability information and do "training" to
 * synchronize transmission.  Once this is done, one or more
 * pages of information may be transmitted, and following this
 * the post-message phase allows for retransmission, line
 * turn-around, and so on.
 *
 * Consult CCITT recommendation T.30, "Procedures for Document
 * Facsimile Transmission in the General Switched Telephone
 * Network" (especially pp. 69-109) for further information.
 */

/*
 * Note: All the FCF codes after the initial identification
 * commands should include FCF_SNDR or FCF_RCVR ``or-ed in''.
 * For example, 
 *    Calling Station	    Called Station
 *    -----------------------------------
 *			<-  FCF_DIS
 *    FCF_DCS|FCF_SNDR  ->
 *    			<-  FCF_CFR|FCF_RCVR
 *	      <<send message data>>
 *    FCF_EOP|FCF_SNDR  ->
 *			<-  FCF_MCF|FCF_RCVR
 *    FCF_DCN|FCF_SNDR	->
 */

// protocol timeouts
#define	TIMER_T1	(35+5)		// 35 +/- 5 seconds
#define	TIMER_T2	(6+1)		// 6 +/- 1 seconds
#define	TIMER_T3	(10+5)		// 10 +/- 5 seconds
#define	TIMER_T4	(5+1)		// 4.5secs +/- 15% for manual units

/*
 * Facsimile control field (FCF) values
 */
#define	FCF_SNDR	0x80		// station receiving valid DIS
#define	FCF_RCVR	0x00		// station receiving valid DIS response

// initial identification commands from the called to calling station
#define	FCF_DIS		0x01		// digital identification signal
#define	FCF_CSI		0x02		// called subscriber identification
#define	FCF_NSF		0x04		// non-standard facilities (optional)

// responses from calling station wishing to recv 
#define	FCF_DTC		(FCF_DIS|FCF_SNDR) // digital transmit command
#define	FCF_CIG		(FCF_CSI|FCF_SNDR) // calling subrscriber identification
#define	FCF_NSC		(FCF_NSF|FCF_SNDR) // non-standard facilities command
// responses from transmitter to receiver
#define	FCF_DCS		(0x40|FCF_DIS)	// digital command signal
#define	FCF_TSI		(0x40|FCF_CSI)	// transmitting subscriber ident.
#define	FCF_NSS		(0x40|FCF_NSF)	// non-standard facilities setup (opt)

// DIS definitions (24-bit representation)
#define	DIS_T4RCVR	0x004000	// receiver honors T.4
#define	DIS_SIGRATE	0x003000	// data signalling rate
#define	    DISSIGRATE_V27FB	0	// V.27 ter fallback mode
#define	    DISSIGRATE_V27	1	// V.27 ter
#define	    DISSIGRATE_V29	2	// V.29
#define	    DISSIGRATE_V27V29	3	// V.27 ter & V.29
#define	DIS_7MMVRES	0x000200	// vertical resolution = 7.7 line/mm
#define	DIS_2DENCODE	0x000100	// 2-d compression supported
#define	DIS_PAGEWIDTH	0x0000C0	// recording width capabilities
#define	    DISWIDTH_1728	0	// only 1728
#define	    DISWIDTH_2432	1	// 2432, 2048, 1728
#define	    DISWIDTH_2048	2	// 2048, 1728
#define	    DISWIDTH_INVALID	3	// invalid, but treat as 2432
#define	DIS_PAGELENGTH	0x000030	// max recording length capabilities
#define	    DISLENGTH_A4	0	// A4 (297 mm)
#define	    DISLENGTH_UNLIMITED	1	// no max length
#define	    DISLENGTH_A4B4	2	// A4 and B4 (364 mm)
#define	    DISLENGTH_INVALID	3
#define	DIS_MINSCAN	0x00000E	// receiver min scan line time
#define	    DISMINSCAN_20MS	0x0
#define	    DISMINSCAN_40MS	0x1
#define	    DISMINSCAN_10MS	0x2
#define	    DISMINSCAN_5MS	0x4
#define	    DISMINSCAN_10MS2	0x3
#define	    DISMINSCAN_20MS2	0x6
#define	    DISMINSCAN_40MS2	0x5
#define	    DISMINSCAN_0MS	0x7
#define	DIS_XTNDFIELD	0x000001	// extended field indicator

// DCS definitions (24-bit representation)
#define	DCS_T4RCVR	0x004000	// receiver honors T.4
#define	DCS_SIGRATE	0x003000	// data signalling rate
#define	    DCSSIGRATE_2400V27	(0<<12)
#define	    DCSSIGRATE_4800V27	(1<<12)
#define	    DCSSIGRATE_9600V29	(2<<12)
#define	    DCSSIGRATE_7200V29	(3<<12)
#define	DCS_7MMVRES	0x000200	// vertical resolution = 7.7 line/mm
#define	DCS_2DENCODE	0x000100	// use 2-d encoding
#define	DCS_PAGEWIDTH	0x0000C0	// recording width
#define	    DCSWIDTH_1728	(0<<6)
#define	    DCSWIDTH_2432	(1<<6)
#define	    DCSWIDTH_2048	(2<<6)
#define	DCS_PAGELENGTH	0x000030	// max recording length
#define	    DCSLENGTH_A4	(0<<4)
#define	    DCSLENGTH_UNLIMITED	(1<<4)
#define	    DCSLENGTH_B4	(2<<4)
#define	DCS_MINSCAN	0x00000E	// receiver min scan line time
#define	    DCSMINSCAN_20MS	(0x0<<1)
#define	    DCSMINSCAN_40MS	(0x1<<1)
#define	    DCSMINSCAN_10MS	(0x2<<1)
#define	    DCSMINSCAN_5MS	(0x4<<1)
#define	    DCSMINSCAN_0MS	(0x7<<1)
#define	DCS_XTNDFIELD	0x000001	// extended field indicator

// pre-message responses
#define	FCF_CFR		0x21		// confirmation to receive
#define	FCF_FTT		0x22		// failure to train

// post-message commands (from transmitter to receiver)
#define	FCF_EOM		0x71		// end-of-page, restart phase B on ack
#define	FCF_MPS		0x72		// end-of-page, restart phase C on ack
#define	FCF_EOP		0x74		// end-of-procedures, hangup after ack
#define	FCF_PRI_EOM	0x79		// EOM, but allow operator intervention
#define	FCF_PRI_MPS	0x7A		// MPS, but allow operator intervention
#define	FCF_PRI_EOP	0x7C		// MPS, but allow operator intervention

// post-message responses (from receiver to transmitter)
#define	FCF_MCF		0x31		// message confirmation (ack MPS/EOM)
#define	FCF_RTP		0x33		// ack, continue after retraining
#define	FCF_RTN		0x32		// nak, retry after retraining
#define	FCF_PIP		0x35		// ack, continue after operating interv.
#define	FCF_PIN		0x34		// nak, retry after operation interv.

// other line control signals
#define	FCF_DCN		0x5F		// disconnect - initiate call release
#define	FCF_CRP		0x58		// command repeat - resend last command
#endif /* _t30_ */
