#ident $Header: /d/sam/flexkit/fax/faxd/RCS/Everex.c++,v 1.5 91/05/23 12:25:18 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "Everex.h"
#include "FaxServer.h"
#include "everex.h"
#include "t.30.h"

#include <libc.h>

EverexModem::EverexModem(FaxServer& s) : FaxModem(s, UNKNOWN)
{
    setEcho(FALSE);
    if (atCmd("I4?", FALSE)) {
	if (getModemLine(rbuf) >= 5) {
	    if (streq(rbuf, "EV958", 5))
		type = EV958;
	    else if (streq(rbuf, "EV968", 5))
		type = EV968;
	}
	if (sync() && type != UNKNOWN) {
	    atCmd("#S3?", FALSE);
	    capabilities = fromHex(rbuf, getModemLine(rbuf));
	    for (int i = S4_GROUP2; i <= S4_9600V29; i++)
		if (isCapable(i))
		    server.traceStatus(FAXTRACE_SERVER, "MODEM supports %s",
			getSignallingRateName(t30RateCodes[i]>>12));
	    sync();
	}
    }
}

EverexModem::~EverexModem()
{
}

const char*
EverexModem::getName() const
{
    return type == EV958 ? "Everex 958" :
	   type == EV968 ? "Everex 968" :
			   "Abaton";
}

int EverexModem::modemRateCodes[4] = {
    S4_4800V27,			// V.27 ter fallback mode
    S4_4800V27,			// V.27 ter
    S4_9600V29,			// V.29
    S4_9600V29,			// V.27 ter and V.29
};
int EverexModem::t30RateCodes[6] = {
    -1,				// S4_GROUP2
    DCSSIGRATE_2400V27,		// S4_2400V27
    DCSSIGRATE_4800V27,		// S4_4800V27
    DCSSIGRATE_7200V29,		// S4_4800V29
    DCSSIGRATE_7200V29,		// S4_7200V29
    DCSSIGRATE_9600V29,		// S4_9600V29
};

fxBool
EverexModem::isCapable(int what) const
{
    return (capabilities & (1<<what));
}

u_int
EverexModem::getBestSignallingRate() const
{
    u_int DIS = 0;
    if (isCapable(S4_9600V29)) {
	if (isCapable(S4_4800V27) || isCapable(S4_2400V27))
	    DIS = DISSIGRATE_V27V29;
	else
	    DIS = DISSIGRATE_V29;
    } else
	DIS = DISSIGRATE_V27;
    return (DIS);
}

int
EverexModem::selectSignallingRate(u_int t30rate)
{
    int rate = modemRateCodes[t30rate];
    if (!isCapable(rate)) {
	// modem doesn't support best remote rate, choose an alternative
	switch (t30rate) {
	case DISSIGRATE_V27FB:
	case DISSIGRATE_V27:
	    rate = isCapable(S4_2400V27) ? S4_2400V27 : -1;
	    break;
	case DISSIGRATE_V29:
	    rate = 
		isCapable(S4_7200V29) ? S4_7200V29 :
	        isCapable(S4_4800V29) ? S4_4800V29 :
		-1;
	    break;
	case DISSIGRATE_V27V29:
	    rate = 
		isCapable(S4_7200V29) ? S4_7200V29 :
	        isCapable(S4_4800V29) ? S4_4800V29 :
	        isCapable(S4_4800V27) ? S4_4800V27 :
	        isCapable(S4_2400V27) ? S4_2400V27 :
		-1;
	    break;
	}
    }
    return (rate == -1 ? -1 : t30RateCodes[rate]);
}

/*
 * Construct the Calling Station Identifier (CSI) string
 * for the modem.  This is encoded as a string of hex digits
 * according to Table 3/T.30 (see the spec).  Hyphen ('-')
 * and period are converted to space; otherwise invalid
 * characters are ignored in the conversion.  The string may
 * be at most 20 characters (according to the spec).
 */
void
EverexModem::setLID(const fxStr& number)
{
    const char* DigitMap =
	" 04" "-04" ".04" "+D4"
	"00C" "18C" "24C" "3CC" "42C"
	"5AC" "66C" "7EC" "81C" "99C";
    char buf[40];
    u_int n = fxmin(number.length(),(u_int) 20);
    for (u_int i = 0, j = 0; i < n; i++) {
	int c = number[i];
	for (const char* dp = DigitMap; *dp; dp += 3)
	    if (c == *dp) {
		buf[j++] = dp[1];
		buf[j++] = dp[2];
		break;
	    }
    }
    /*
     * Now ``reverse copy'' the string.
     */
    lid.resize(40);
    for (i = 0; j > 1; i += 2, j -= 2) {
	lid[i] = buf[j-2];
	lid[i+1] = buf[j-1];
    }
    for (; i < 40; i+= 2)
	lid[i] = '0', lid[i+1] = '4';		// blank pad remainder
}

fxBool
EverexModem::sendFrame(int f1)
{
    lastFrame = f1;
    return (atCmd("#FT=" | toHex(f1,2)));
}

fxBool
EverexModem::sendFrame(int f1, int f2)
{
    return (atCmd("#FT=" | toHex(f1,2) | toHex(f2,2)));
}

fxBool
EverexModem::sendFrame(int f1, int f2, int f3)
{
    return (atCmd("#FT=" | toHex(f1,2) | toHex(f2,2) | toHex(f3,2)));
}

fxBool
EverexModem::setupFrame(int f, int v)
{
    return (setupFrame(f, (char*) toHex(v, 8)));
}

fxBool
EverexModem::setupFrame(int f, const char* v)
{
    return (atCmd("#FT" | toHex(f, 2) | "=" | v));
}

/* 
 * Modem manipulation support.
 */
CallStatus
EverexModem::dial(const fxStr& number)
{
    char* dialCmd = server.getToneDialing() ? "#DT" : "#DP";
    if (!atCmd(dialCmd | number, FALSE))
	return (FAILURE);
    for (;;) {
	if (getModemLine(rbuf) == 0)
	    return (FAILURE);
	if (streq(rbuf, "FAX", 3))
	    return (OK);
	else if (type == EV958 && streq(rbuf, "OK", 2))
	    return (OK);
	else if (streq(rbuf, "BUSY", 4))
	    return (BUSY);
	else if (streq(rbuf, "NO CARRIER", 10))
	    return (NOCARRIER);
	else if (streq(rbuf, "NO DIALTONE", 11))
	    return (NODIALTONE);
	else if (streq(rbuf, "ERROR", 5))
	    return (ERROR);		// XXX
    }
}

fxBool
EverexModem::reset()
{
    // V1	enable verbose codes
    // Q0	enable responses
    // H0	hangup phone
    // E0	disable echo
    // #V	enable verbose codes
    // #Z	reset fax state
    if (!FaxModem::reset())
	return (FALSE);
    return (atCmd("#V#Z"));
}

fxBool
EverexModem::abort()
{
    // XXX need to maintain state and, possibly, send DCN
    return (atCmd("#ZH0"));
}

fxBool
EverexModem::modemFaxConfigure(int bits)
{
    return (atCmd("#S2=" | fxStr(bits, "%d")));
}

fxBool
EverexModem::getPrologue(u_int& dis, u_int& xinfo, u_int& nsf)
{
    if (!atCmd("#FR01?", FALSE))	// fetch DIS bytes
	return (FALSE);
    nsf = 0;
    int n = getModemLine(rbuf);
    if (n >= 6) {
	dis = fromHex(rbuf, 6);
	xinfo = (dis&DIS_XTNDFIELD) && n >= 8 ? fromHex(rbuf+6, 2) : 0;
	return (sync());
    } else {				// bogus read
	(void) sync();
	return (FALSE);
    }
}
