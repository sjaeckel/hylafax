#ident $Header: /d/sam/flexkit/fax/faxd/RCS/Everex.h,v 1.7 91/09/23 13:45:35 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _EVEREX_
#define	_EVEREX_

#include "FaxModem.h"
#include "Str.h"
#include <setjmp.h>

/*
 * ``Old'' Everex Class 1-style FAX Modem.
 */
class EverexModem : public FaxModem {
private:
    fxBool	is2D;
    int		capabilities;
    char	rbuf[1024];		// last line of input received
    fxStr	lid;			// local id string in modem format
// for reception
    fxStr	tsi;
    TIFF*	tif;			// temp file for receiving
    int		shdata;			// input byte
    int		shbit;			// bit mask for input byte
    jmp_buf	recvEOF;		// for unexpected EOF on recv
    u_int	lastFrame;		// last HDLC frame transmitted

    static int modemRateCodes[4];
    static int t30RateCodes[6];
    static int modemScanCodes[8];

    fxBool sendTraining(fxStr& emsg);
    fxBool sendDocument(TIFF* tif, fxStr& emsg, int EOPcmd);
    fxBool sendPage(TIFF* tif, fxStr& emsg);
    fxBool sendRTC();

    fxBool recvIdentification();
    fxBool recvTSI(fxStr& tsi);
    fxBool recvDCS();
    fxBool recvTraining(fxBool okToRecv);
    void recvPage();
    void recvPageData();
    void recvCode(int& len, int& code);
    int recvBit();

    fxBool modemFaxConfigure(int bits);
    fxBool isCapable(int) const;

// HDLC frame support
    fxBool sendFrame(int f1);
    fxBool sendFrame(int f1, int f2);
    fxBool sendFrame(int f1, int f2, int f3);
    fxBool setupFrame(int f, int v);
    fxBool setupFrame(int f, const char* v);
public:
    EverexModem(FaxServer& s);
    virtual ~EverexModem();

    const char* getName() const;

    fxBool reset();
    fxBool abort();
    void setLID(const fxStr& number);

    int selectSignallingRate(u_int t30rate);
    u_int getBestSignallingRate() const;

    void sendBegin();
    CallStatus dial(const fxStr& number);
    fxBool getPrologue(u_int& dis, u_int& xinfo, u_int& nsf);
    void sendSetupPhaseB();
    fxBool sendPhaseB(TIFF* tif, u_int dcs, fxStr& emsg, fxBool lastDoc);
    void sendEnd();

    fxBool recvBegin(u_int dis);
    TIFF* recvPhaseB(fxBool okToRecv);
};
#endif /* _EVEREX_ */
