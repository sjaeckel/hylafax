#ident $Header: /d/sam/flexkit/fax/faxd/RCS/Class2.h,v 1.5 91/09/23 13:44:38 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#ifndef _CLASS2_
#define	_CLASS2_

#include "FaxModem.h"
#include "Str.h"
#include <setjmp.h>
#include <stdarg.h>

/*
 * Class 2-style FAX Modem.
 */
class Class2Modem : public FaxModem {
private:
    int		services;		// services modem supports
    fxStr	manufacturer;		// manufacturer identification
    fxStr	model;			// model identification
    fxStr	revision;		// product revision identification
    int		maxsignal;		// signalling capabilities
    int		bor;			// phase B/C/D data bit ordering
    fxBool	is2D;			// true if working w/ 2d-encoded data
    char	rbuf[1024];		// last line of input received
    TIFF*	tif;			// temp file for receiving
    u_int	hangupCode;		// hangup reason (from modem)

    static u_int logicalRate[4];	// map modem speed code to our speed
// tables for mapping modem codes to T.30 DIS codes
    static u_int vrDISTab[2];		// vertical resolution
    static u_int dfDISTab[2];		// data compression format
    static u_int brDISTab[4];		// baud rate
    static u_int wdDISTab[3];		// page width
    static u_int lnDISTab[3];		// page length
    static u_int stDISTab[8];		// min scanline time
// tables for mapping modem codes to T.30 DCS codes
    static u_int brDCSTab[4];		// baud rate
// tables for mapping T.30 DCS values to modem codes
    static u_int DCSbrTab[4];		// baud rate
    static u_int DCSwdTab[4];		// page width
    static u_int DCSlnTab[4];		// page length
    static u_int DCSstTab[8];		// min scanline time

    fxBool setupModem();
    fxBool waitFor(const char* wanted);
    const char* hangupCause(u_int code);

    fxBool sendPage(TIFF* tif);
    fxBool sendEOT();

    CallStatus answer();
    fxBool recvDCS(const char*);
    fxBool recvTSI(const char*);
    fxBool recvPage();
    fxBool recvPageData();
    void recvData(u_char* buf, int n);

    fxBool dataTransfer();
    fxBool dataReception();

    fxBool class2Cmd(const char* cmd);
    fxBool class2Cmd(const char* cmd, int a0);
    fxBool class2Cmd(const char* cmd, int a0, int a1);
    fxBool class2Cmd(const char* cmd, int a0, int a1, int a2);
    fxBool class2Cmd(const char* cmd, int a0, int a1, int a2, int a3);
    fxBool class2Cmd(const char* cmd, int, int, int, int, int, int, int, int);
    fxBool class2Cmd(const char* cmd, const char* s);
    fxBool vclass2Cmd(const char* cmd, fxBool waitForOK, int nargs ... );

    fxBool class2Query(const char* what);
    fxBool class2Query(const char* what, int& v);
    fxBool class2Query(const char* what, fxStr& v);

    fxBool parseRange(const char*, int&);
    fxBool parseRange(const char*, int&, int&);
    fxBool parseRange(const char*, int&, int&, int&);
    fxBool parseRange(const char*, int&, int&, int&, int&);
    fxBool parseRange(const char*, int&, int&, int&, int&, int&);
    fxBool parseRange(const char*, int&, int&, int&, int&, int&, int&);
    fxBool parseRange(const char*, int&, int&, int&, int&, int&, int&, int&);
    fxBool parseRange(const char*, int&, int&, int&, int&, int&, int&, int&, int&);
    fxBool vparseRange(const char*, int nargs ...);
public:
    Class2Modem(FaxServer& s);
    virtual ~Class2Modem();

    const char* getName() const;

    fxBool reset();
    fxBool abort();
    void setLID(const fxStr& number);

    CallStatus dial(const fxStr& number);
    int selectSignallingRate(u_int t30rate);
    u_int getBestSignallingRate() const;
    fxBool getPrologue(u_int& dis, u_int& xinfo, u_int& nsf);

    fxBool sendPhaseB(TIFF* tif, u_int dcs, fxStr& emsg, fxBool lastDoc);

    fxBool recvBegin(u_int dis);
    TIFF* recvPhaseB(fxBool okToRecv);
};
#endif /* _CLASS2_ */
