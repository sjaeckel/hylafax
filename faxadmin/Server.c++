#ident $Header: /usr/people/sam/flexkit/fax/faxadmin/RCS/Server.c++,v 1.8 91/05/30 13:50:31 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "faxAdmin.h"
#include "FaxTrace.h"
#include "ConfirmDialog.h"

#include <osfcn.h>
#include <sys/fcntl.h>

#include "Font.h"
#include "RegEx.h"
#include "Valuator.h"
#include "StatusLabel.h"
#include "Menu.h"
#include "FieldEditor.h"

void
faxAdmin::answerPhone()
{
    assert(fifo != NULL);
    putc('A', fifo);
    (void) flushAndCheck();
}

void
faxAdmin::stopServer()
{
    if (fifo != NULL) {
	putc('Q', fifo);
	fflush(fifo);
	fclose(fifo);
	fifo = NULL;
	setServerStatus(FALSE);
    } else
	notifyUser(this, "No server is currently running.");
}

void
faxAdmin::startServer()
{
    if (fifo == NULL) {
	writeConfiguration();
	char cmd[1024];
	sprintf(cmd, "/usr/etc/faxd -m %s&", (char*) device);
	(void) system(cmd);
	u_int l = device.length();
	const fxStr& dev = device.tokenR(l, '/');
	// XXX can block if faxd doesn't start up correctly
	int fd = ::open(queueDir | "/" | fifoName | "." | dev, O_WRONLY);
	if (fd >= 0)
	    fifo = fdopen(fd, "w");
	setServerStatus(fifo != NULL);
    } else
	notifyUser(this, "A server is already running.");
}

void
faxAdmin::sendServer(const char* tag, const fxStr& s)
{
    if (fifo) {
	fprintf(fifo, "M%s:%s\n", tag, (char*) s);
	if (!flushAndCheck())
	    return;
    }
    isDirty = TRUE;
}

void
faxAdmin::sendServer(const char* tag, int v)
{
    if (fifo) {
	fprintf(fifo, "M%s:%d\n", tag, v);
	if (!flushAndCheck())
	    return;
    }
    isDirty = TRUE;
}

fxBool
faxAdmin::flushAndCheck()
{
    fflush(fifo);
    if (ferror(fifo)) {
	fclose(fifo);
	fifo = NULL;
	notifyUser(this, "The server appears to have died.");
	setServerStatus(FALSE);
	return (FALSE);
    } else
	return (TRUE);
}

static const char* putBoolean(fxBool b)
    { return (b ? "yes" : "no"); }

void
faxAdmin::setFaxNumber(const fxStr& code)
{
    fxStr canon;
    if (checkPhoneNumber(code, canon)) {
	if (faxNumber != faxNumberEditor->getValue()) {
	    faxNumber = canon;
	    sendServer("FAXNumber", canon);
	    faxNumberEditor->setValue(canon);
	}
    }
}

void
faxAdmin::setVoiceNumber(const fxStr& code)
{
    fxStr canon;
    if (checkPhoneNumber(code, canon)) {
	if (voiceNumber != voiceNumberEditor->getValue()) {
	    voiceNumber = canon;
	    sendServer("VoiceNumber", canon);
	    voiceNumberEditor->setValue(canon);
	}
    }
}

fxBool
faxAdmin::checkPhoneNumber(const fxStr& code, fxStr& canon)
{
    fxRegEx ws("[-+\\. ]*");
    fxRegEx num("[1-9]+[0-9]*");
    fxStr s(code);
    canon = "";
    for (int i = 0; i < 4; i++) {
	int end;
	if (end = ws.findEnd(s))
	    s.remove(0, end);
	int start;
	num.bracket(s, start, end);	// get number
	if (start == -1)
	    break;
	if (start != 0) {		// garbage in the middle
	    badPhoneNumber();
	    return (FALSE);
	}
	canon.append(s.cut(start, end));
    }
    // XXX US-centric
    if (canon.length() < 10) {
	badPhoneNumber();
	return (FALSE);
    }
    if (canon[0] != '1')
	canon.insert("1 ");
    areaCode = canon.extract(1, 3);
    return (TRUE);
}

#ifdef notdef
/*
 * Convert a phone number to a canonical format:
 *	+<country><areacode><number>
 * This involves, possibly, stripping off leading
 * dialing prefixes for long distance and/or
 * international dialing.
 */
fxStr
faxAdmin::canonicalizePhoneNumber(const fxStr& number)
{
    fxStr canon(number);
    // strip everything but digits
    for (int i = canon.length()-1; i >= 0; i--)
	if (!isdigit(canon[i]))
	    canon.remove(i);
    if (number[number.skip(0, " \t")] != '+') {
	// form canonical phone number by removing
	// any long-distance and/or international
	// dialing stuff and by prepending local
	// area code and country code -- as appropriate
	fxStr prefix = canon.extract(0, internationalPrefix.length());
	if (prefix != internationalPrefix) {
	    prefix = canon.extract(0, longDistancePrefix.length());
	    if (prefix != longDistancePrefix)
		canon.insert(myAreaCode);
	    else
		canon.remove(0, longDistancePrefix.length());
	    canon.insert(myCountryCode);
	} else
	    canon.remove(0, internationalPrefix.length());
    }
    canon.insert('+');
    return (canon);
}
#endif

void faxAdmin::setDialingPrefix(const fxStr& prefix)
    { sendServer("DialingPrefix", prefix); }
void faxAdmin::setUseDialingPrefix(fxBool on)
    { sendServer("UseDialingPrefix", putBoolean(usePrefix = on)); }
void faxAdmin::setSpeakerVolume(int level)
    { sendServer("SpeakerVolume", speakerVolume = (SpeakerVolume) level); }
void faxAdmin::setToneDialing(fxBool on)
    { sendServer("ToneDialing", putBoolean(toneDialing = on)); }
void
faxAdmin::setTagLineFormat(const fxStr& fmt)	// XXX handle %'s
{
    tagLineFormat = fmt;
    sendServer("TagLineFormat", fmt);
}
void
faxAdmin::setTagLineFont(const fxStr& font)
{
    fxFont* f = fxGetFont(font);
    if (f->getName() == font) {
	tagLineFont = font;
	sendServer("TagLineFont", font);
    } else
	badFont();
}
void faxAdmin::setTagLineAtTop(fxBool on)
    { sendServer("TagLineAtTop", putBoolean(tagLineAtTop = on)); }

void
faxAdmin::setRingsReadout(int v)
{
    ringsReadout->setValue(
	v == 0 ? "don't answer" :
	v == 1 ? "%d ring" :
		 "%d rings",
	v);
}

void
faxAdmin::setRings()
{
    int v = (int) ringsSlider->getValue();
    if (rings != v)
	sendServer("RingsBeforeAnswer", rings = v);
}

void faxAdmin::setQualifyTSI(fxBool on)
    { sendServer("QualifyTSI", putBoolean(qualifyTSI = on)); }

void
faxAdmin::setTracing(fxMenuItem* mi, int kind)
{
    if (protocolTracing & kind) {
	mi->setState(0);
	protocolTracing &= ~kind;
    } else {
	mi->setState(1);
	protocolTracing |= kind;
    }
    sendServer("ProtocolTracing", protocolTracing);
}

void faxAdmin::traceServer()
   { setTracing(traceServerItem, FAXTRACE_SERVER); }
void faxAdmin::traceProtocol()
   { setTracing(traceProtoItem, FAXTRACE_PROTOCOL); }
void faxAdmin::traceModemOps()
   { setTracing(traceModemOpsItem, FAXTRACE_MODEMOPS); }
void faxAdmin::traceModemCom()
   { setTracing(traceModemComItem, FAXTRACE_MODEMCOM); }
void faxAdmin::traceTimeouts()
   { setTracing(traceTimeoutsItem, FAXTRACE_TIMEOUTS); }
