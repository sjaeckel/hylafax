#ident $Header: /d/sam/flexkit/fax/faxd/RCS/Class2.c++,v 1.8 91/10/21 14:23:49 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "Class2.h"
#include "FaxServer.h"
#include "class2.h"
#include "t.30.h"

#include <sys/termio.h>
#include <libc.h>
#include <ctype.h>

u_int Class2Modem::logicalRate[4] = { 0, 1, 3, 2 };

Class2Modem::Class2Modem(FaxServer& s) : FaxModem(s, UNKNOWN)
{
    // we use BOR_C_DIR instead of BOR_C_REV (which makes more
    // sense for our big-endian machine 'cuz of a bug in the
    // modem firmware (Rev 901231 or earlier) -- BOR=3 and REL=1
    // generates garbage data when receiving facsimile (in the
    // pad bytes)
    bor = BOR_C_DIR + BOR_BD_REV;
    if (selectBaudRate())
	setupModem();
}

Class2Modem::~Class2Modem()
{
}

const char* Class2Modem::getName() const { return "Class2"; }

fxBool
Class2Modem::setupModem()
{
    type = UNKNOWN;
    if (class2Query("CLASS=") &&
      getModemLine(rbuf) > 0 && parseRange(rbuf, services)) {
	if (services & SERVICE_DATA)
	    server.traceStatus(FAXTRACE_SERVER, "MODEM Service \"data\"");
	if (services & SERVICE_CLASS1)
	    server.traceStatus(FAXTRACE_SERVER, "MODEM Service \"class1\"");
	if (services & SERVICE_CLASS2) {
	    server.traceStatus(FAXTRACE_SERVER, "MODEM Service \"class2\"");
	    type = CLASS2;
	}
	sync();
    }
    if (type != CLASS2)
	return (FALSE);
    class2Cmd("CLASS", 2);
    if (class2Query("MFR", manufacturer))
	server.traceStatus(FAXTRACE_SERVER, "MODEM Mfr \"%s\"",
	    (char*) manufacturer);
    if (class2Query("MDL", model))
	server.traceStatus(FAXTRACE_SERVER, "MODEM Model \"%s\"",
	    (char*) model);
    if (class2Query("REV", revision))
	server.traceStatus(FAXTRACE_SERVER, "MODEM Revision \"%s\"",
	    (char*) revision);
    if (class2Query("DCC=")) {
	int vr, br, wd, pl, df, ec, bf, st;
	// syntax: (vr),(br),(wd),(ln),(df),(ec),(bf),(st)
	// where,
	//	vr	vertical resolution
	//	br	bit rate
	//	wd	page width
	//	pl	page length
	//	df	data compression
	//	ec	error correction
	//	bf	binary file transfer
	//	st	scan time/line
	if (getModemLine(rbuf) > 0 &&
	  parseRange(rbuf, vr, br, wd, pl, df, ec, bf, st)) {
	    maxsignal = 0;
	    for (u_int i = 0; i < 4; i++)
		if (br & BIT(i)) {
		    server.traceStatus(FAXTRACE_SERVER, "MODEM supports %s",
			getSignallingRateName(logicalRate[i]));
		    maxsignal = i;
		}
	    if (ec & (BIT(1)|BIT(2)))
		server.traceStatus(FAXTRACE_SERVER,
		    "MODEM supports error correction");
	    if (bf & BIT(1))
		server.traceStatus(FAXTRACE_SERVER,
		    "MODEM supports binary file transfer");
	}
	sync();
    }
    int cq = 0;
    if (class2Query("CQ=", cq)) {
	if (cq & BIT(1)) {
	    server.traceStatus(FAXTRACE_SERVER,
		"MODEM supports 1-D copy quality checking");
	    class2Cmd("CQ", 1);		// enable checking
	    server.traceStatus(FAXTRACE_SERVER,
		"MODEM error threshold multiplier %d", 20);
	    class2Cmd("BADMUL", 20);	// error threshold multiplier
	    server.traceStatus(FAXTRACE_SERVER,
		"MODEM bad line threshold %d", 10);
	    class2Cmd("BADLIN", 10);	// bad line threshold
	} else
	    server.traceStatus(FAXTRACE_SERVER,
		"MODEM does no copy quality checking");
    }
    server.traceStatus(FAXTRACE_SERVER, "MODEM Phase C timeout %d", 30);
    class2Cmd("PHCTO", 30);		// phase C timeout
    class2Cmd("TBC", 0);		// stream mode
    class2Cmd("BOR", bor);		// bit ordering
    class2Cmd("CR", 1);			// enable receiving
    class2Cmd("REL", 1);		// byte-align received EOL's
    if (server.getTracing() & FAXTRACE_PROTOCOL)
	class2Cmd("BUG", 1);
    return (TRUE);
}

u_int
Class2Modem::getBestSignallingRate() const
{
    return brDCSTab[maxsignal] >> 12;
}

int
Class2Modem::selectSignallingRate(u_int t30rate)
{
    int rate = DCSbrTab[t30rate];
    return brDISTab[fxmin(rate, maxsignal)];	// NB: assumes DIS* == DCS*
}

/*
 * Construct the Calling Station Identifier (CSI) string
 * for the modem.  This is encoded as an ASCII string
 * according to Table 3/T.30 (see the spec).  Hyphen ('-')
 * and period are converted to space; otherwise invalid
 * characters are ignored in the conversion.  The string may
 * be at most 20 characters (according to the spec).
 */
void
Class2Modem::setLID(const fxStr& number)
{
    fxStr csi;
    u_int n = fxmin((u_int) number.length(), (u_int) 20);
    for (u_int i = 0; i < n; i++) {
	char c = number[i];
	if (c == ' ' || c == '-' || c == '.')
	    csi.append(' ');
	else if (c == '+' || isdigit(c))
	    csi.append(c);
    }
    class2Cmd("LID", (char*) csi);
}

/* 
 * Modem manipulation support.
 */

fxBool
Class2Modem::reset()
{
    return (FaxModem::reset());
}

fxBool
Class2Modem::abort()
{
    return class2Cmd("K");
}

fxBool
Class2Modem::waitFor(const char* wanted)
{
    u_int len = strlen(wanted);
    fxBool status = TRUE;
    do {
	if (getModemLine(rbuf) < 0)
	    return (FALSE);
	if (streq(rbuf, "+FHNG:", 6)) {
	    hangupCode = (u_int) atoi(rbuf+6);
	    server.traceStatus(FAXTRACE_PROTOCOL,
		"REMOTE HANGUP: %s (code %s)", hangupCause(hangupCode), rbuf+6);
	    // return failure, but wait for wanted string
	    status = (hangupCode == 0);
	}
	if (streq(rbuf, "ERROR", 5)) {
	    server.traceStatus(FAXTRACE_PROTOCOL,
		"MODEM ERROR: Command error (unknown reason)");
	    return (FALSE);
	}
    } while (!streq(rbuf, wanted, len));
    return (status);
}

fxBool Class2Modem::class2Cmd(const char* cmd)
    { return vclass2Cmd(cmd, TRUE, 0); }
fxBool Class2Modem::class2Cmd(const char* cmd, int a0)
    { return vclass2Cmd(cmd, TRUE, 1, a0); }
fxBool Class2Modem::class2Cmd(const char* cmd, int a0, int a1)
    { return vclass2Cmd(cmd, TRUE, 2, a0, a1); }
fxBool Class2Modem::class2Cmd(const char* cmd, int a0, int a1, int a2)
    { return vclass2Cmd(cmd, TRUE, 3, a0, a1, a2); }
fxBool Class2Modem::class2Cmd(const char* cmd, int a0, int a1, int a2, int a3)
    { return vclass2Cmd(cmd, TRUE, 4, a0, a1, a2, a3); }
fxBool Class2Modem::class2Cmd(const char* cmd, int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)
    { return vclass2Cmd(cmd, TRUE, 8, a0, a1, a2, a3, a4, a5, a6, a7); }

fxBool Class2Modem::dataTransfer()
    { return vclass2Cmd("DT", FALSE, 0); }
fxBool Class2Modem::dataReception()
    { return vclass2Cmd("DR", FALSE, 0); }

fxBool
Class2Modem::vclass2Cmd(const char* cmd, fxBool waitForOK, int nargs ... )
{
    char buf[512];
    char* cp = buf;
    sprintf(buf, "+F%s", cmd);
    va_list ap;
    va_start(ap, nargs);
    char* sep = "=";
    while (nargs-- > 0) {
	cp = strchr(cp, '\0');
	sprintf(cp, "%s%d", sep, va_arg(ap, int));
	sep = ",";
    }
    va_end(ap);
    if (!atCmd(buf, FALSE))
	return (FALSE);
    return waitForOK ? waitFor("OK") : TRUE;
}

fxBool
Class2Modem::class2Cmd(const char* cmd, const char* s)
{
    char buf[512];
    sprintf(buf, "+F%s=\"%s\"", cmd, s);	// XXX handle escapes
    return atCmd(buf, FALSE) && waitFor("OK");
}

fxBool
Class2Modem::class2Query(const char* what)
{
    char buf[80];
    sprintf(buf, "+F%s?", what);
    return atCmd(buf, FALSE);
}

fxBool
Class2Modem::class2Query(const char* what, int& v)
{
    fxBool status =
	(class2Query(what) && getModemLine(rbuf) > 0 && parseRange(rbuf, v));
    sync();
    return (status);
}

fxBool
Class2Modem::class2Query(const char* what, fxStr& v)
{
    if (class2Query(what)) {
	v.resize(0);
	int n;
	while ((n = getModemLine(rbuf)) >= 0 && !streq(rbuf, "OK", 2)) {
	    if (v.length())
		v.append('\n');
	    v.append(rbuf);
	}
	return (TRUE);
    } else {
	sync();
	return (FALSE);
    }
}

fxBool Class2Modem::parseRange(const char* cp, int& a0)
    { return vparseRange(cp, 1, &a0); }
fxBool Class2Modem::parseRange(const char* cp, int& a0, int& a1)
    { return vparseRange(cp, 2, &a0, &a1); }
fxBool Class2Modem::parseRange(const char* cp, int& a0, int& a1, int& a2)
    { return vparseRange(cp, 3, &a0, &a1, &a2); }
fxBool Class2Modem::parseRange(const char* cp, int& a0, int& a1, int& a2, int& a3)
    { return vparseRange(cp, 4, &a0, &a1, &a2, &a3); }
fxBool Class2Modem::parseRange(const char* cp, int& a0, int& a1, int& a2, int& a3, int& a4)
    { return vparseRange(cp, 5, &a0, &a1, &a2, &a3, &a4); }
fxBool Class2Modem::parseRange(const char* cp, int& a0, int& a1, int& a2, int& a3, int& a4, int& a5)
    { return vparseRange(cp, 6, &a0, &a1, &a2, &a3, &a4, &a5); }
fxBool Class2Modem::parseRange(const char* cp, int& a0, int& a1, int& a2, int& a3, int& a4, int& a5, int& a6)
    { return vparseRange(cp, 7, &a0, &a1, &a2, &a3, &a4, &a5, &a6); }
fxBool Class2Modem::parseRange(const char* cp, int& a0, int& a1, int& a2, int& a3, int& a4, int& a5, int& a6, int& a7)
    { return vparseRange(cp, 8, &a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7); }

const char OPAREN = '(';
const char CPAREN = ')';
const char COMMA = ',';
const char SPACE = ' ';

#include <ctype.h>

fxBool
Class2Modem::vparseRange(const char* cp, int nargs ... )
{
    fxBool b = TRUE;
    va_list ap;
    va_start(ap, nargs);
    while (nargs-- > 0) {
	if (cp[0] != OPAREN) {
	    b = FALSE;
	    break;
	}
	cp++;				// skip OPAREN
	int mask = 0;
	while (cp[0] != CPAREN) {
	    if (!isdigit(cp[0])) {
		b = FALSE;
		goto done;
	    }
	    int v = 0;
	    do {
		v = v*10 + (cp[0] - '0');
	    } while (isdigit((++cp)[0]));
	    int r = v;
	    if (cp[0] == '-' && isdigit((++cp)[0])) {	// range
		r = 0;
		do {
		    r = r*10 + (cp[0] - '0');
		} while (isdigit((++cp)[0]));
	    }
	    for (; v <= r; v++)
		mask |= 1<<v;
	    if (cp[0] == SPACE || cp[0] == COMMA)
		cp++;
	}
	cp++;				// skip CPAREN
	*va_arg(ap, int*) = mask;
	if (cp[0] == SPACE || cp[0] == COMMA)
	    cp++;
    }
done:
    va_end(ap);
    return (b);
}

static struct HangupCode {
    u_int	code;
    char*	message;
} hangupCodes[] = {
// Call placement and termination
    {  0, "Normal and proper end of connection" },
    {  1, "Ring detect without successful handshake" },
    {  2, "Call aborted, from +FK or <CAN>" },
    {  3, "No loop current" },
// Transmit Phase A & miscellaneous errors
    { 10, "Unspecified Phase A error" },
    { 11, "No answer (T.30 T1 timeout)" },
// Transmit Phase B
    { 20, "Unspecified Transmit Phase B error" },
    { 21, "Remote cannot be polled" },
    { 22, "COMREC error in transmit Phase B/got DCN" },
    { 23, "COMREC invalid command received/no DIS or DTC" },
    { 24, "RSPEC error/got DCN" },
    { 25, "DCS sent 3 times without response" },
    { 26, "DIS/DTC recieved 3 times; DCS not recognized" },
    { 27, "Failure to train at 2400 bps or +FMINSP value" },
    { 28, "RSPREC invalid response received" },
// Transmit Phase C
    { 40, "Unspecified Transmit Phase C error" },
    { 43, "DTE to DCE data underflow" },
// Transmit Phase D
    { 50, "Unspecified Transmit Phase D error, including +FPHCTO timeout"
	  " between data and +FET command" },
    { 51, "RSPREC error/got DCN" },
    { 52, "No response to MPS repeated 3 times" },
    { 53, "Invalid response to MPS" },
    { 54, "No response to EOP repeated 3 times" },
    { 55, "Invalid response to EOP" },
    { 56, "No response to EOM repeated 3 times" },
    { 57, "Invalid response to EOM" },
    { 58, "Unable to continue after PIN or PIP" },
// Received Phase B
    { 70, "Unspecified Receive Phase B error" },
    { 71, "RSPREC error/got DCN" },
    { 72, "COMREC error" },
    { 73, "T.30 T2 timeout, expected page not received" },
    { 74, "T.30 T1 timeout after EOM received" },
// Receive Phase C
    { 90, "Unspecified Phase C error, including too much delay between"
	  " TCF and +FDR command" },
    { 91, "Missing EOL after 5 seconds (section 3.2/T.4)" },
    { 92, "-unused code 92-" },
    { 93, "DCE to DTE buffer overflow" },
    { 94, "Bad CRC or frame (ECM or BFT modes)" },
// Receive Phase D
    { 100,"Unspecified Phase D error" },
    { 101,"RSPREC invalid response received" },
    { 102,"COMREC invalid response received" },
    { 103,"Unable to continue after PIN or PIP, no PRI-Q" },
// Everex proprietary error codes (9/28/90)
    { 128,"Cannot send: +FMINSP > remote's +FDIS(BR) code" },
    { 129,"Cannot send: remote is V.29 only,"
	  " local DCE constrained to 2400 or 4800 bps" },
    { 130,"Remote station cannot receive (DIS bit 10)" },
    { 131,"+FK aborted or <CAN> aborted" },
    { 132,"+Format conversion error in +FDT=DF,VR,WD,LN"
	  " Incompatible and inconvertable data format" },
    { 133,"Remote cannot receive" },
    { 134,"After +FDR, DCE waited more than 30 seconds for"
	  " XON from DTE after XOFF from DTE" },
    { 135,"In Polling Phase B, remote cannot be polled" },
    { 256 }
};

const char*
Class2Modem::hangupCause(u_int code)
{
    for (HangupCode* hc = hangupCodes; hc->code != 256; hc++)
	if (hc->code == code)
	    return (hc->message);
    return "Unknown code";
}
