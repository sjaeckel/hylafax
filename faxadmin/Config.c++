#ident $Header: /usr/people/sam/flexkit/fax/faxadmin/RCS/Config.c++,v 1.10 91/05/28 22:17:12 sam Exp $
#include "faxAdmin.h"
#include "FaxTrace.h"

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "FieldEditor.h"
#include "Valuator.h"
#include "MultiChoice.h"

#include <osfcn.h>

static fxBool getBoolean(const char* cp)
    { return (strcasecmp(cp, "on") == 0 || strcasecmp(cp, "yes") == 0); }
#define	isTag(s)	(strcasecmp(buf, (s)) == 0)

#include <ctype.h>

void
faxAdmin::readConfiguration()
{
    int tracing = protocolTracing;

    u_int l = device.length();
    const fxStr& dev = device.tokenR(l, '/');
    restoreState(queueDir | "/" | configName | "." | dev);

    if (okToUpdate) {
	ringsSlider->setValue(rings);	setRings();
	setFaxNumber(faxNumber);
	setVoiceNumber(voiceNumber);
	if (speakerVolumeChoice.getCurrentButton() != speakerVolume) {
	    speakerVolumeChoice.setCurrentButton(speakerVolume);
	    setSpeakerVolume(speakerVolume);
	}
	setQualifyTSI(qualifyTSI);
	if (tracing != protocolTracing) {
	    int t = protocolTracing;
	    protocolTracing = tracing;
	    tracing ^= t;
	    if (tracing & FAXTRACE_SERVER)
		traceServer();
	    if (tracing & FAXTRACE_PROTOCOL)
		traceProtocol();
	    if (tracing & FAXTRACE_MODEMOPS)
		traceModemOps();
	    if (tracing & FAXTRACE_MODEMCOM)
		traceModemCom();
	    if (tracing & FAXTRACE_TIMEOUTS)
		traceTimeouts();
	}
    }
}

void
faxAdmin::restoreState(const char* file)
{
    FILE* fd = fopen(file, "r");
    if (!fd)
	return;
    char buf[256];
    while (fgets(buf, sizeof (buf)-1, fd)) {
	char* cp = strchr(buf, '#');
	if (!cp)
	    cp = strchr(buf, '\n');
	if (cp)
	    *cp = '\0';
	cp = strchr(buf, ':');
	if (!cp)
	    continue;
	*cp++ = '\0';
	while (isspace(*cp))
	    cp++;
	if (isTag("FAXNumber"))
	    faxNumber = cp;
	else if (isTag("VoiceNumber"))
	    voiceNumber = cp;
	else if (isTag("AreaCode"))
	    areaCode = cp;
	else if (isTag("CountryCode"))
	    countryCode = cp;
	else if (isTag("DialPrefix"))
	    prefix = cp;
	else if (isTag("LongDistancePrefix"))
	    longDistancePrefix = cp;
	else if (isTag("InternationalPrefix"))
	    internationalPrefix = cp;
	else if (isTag("UseDialPrefix"))
	    usePrefix = getBoolean(cp);
	else if (isTag("SpeakerVolume"))
	    speakerVolume = (SpeakerVolume) atoi(cp);
	else if (isTag("ProtocolTracing"))
	    protocolTracing = atoi(cp);
	else if (isTag("ToneDialing"))
	    toneDialing = getBoolean(cp);
	else if (isTag("RingsBeforeAnswer"))
	    rings = atoi(cp);
	else if (isTag("CommaPauseTime"))
	    pauseTime = atoi(cp);
	else if (isTag("RecvFileMode"))
	    recvFileMode = (int) strtol(cp, 0, 8);
	else if (isTag("WaitForCarrier"))
	    waitTime = atoi(cp);
	else if (isTag("TagLineFont"))
	    tagLineFont = cp;
	else if (isTag("TagLineFormat"))
	    tagLineFormat = cp;
	else if (isTag("TagLineAtTop"))
	    tagLineAtTop = getBoolean(cp);
	else if (isTag("ModemType"))
	    modemType = getModemTypeFromName(cp);
	else if (isTag("QualifyTSI"))
	    qualifyTSI = getBoolean(cp);
    }
    fclose(fd);
}

ModemType
faxAdmin::getModemTypeFromName(const char* cp)
{
    if (strcasecmp(cp, "Abaton") == 0)
	return FaxModem::ABATON;
    else if (strcasecmp(cp, "Ev958") == 0)
	return FaxModem::EV958;
    else if (strcasecmp(cp, "Ev968") == 0)
	return FaxModem::EV968;
    else if (strcasecmp(cp, "Class1") == 0)
	return FaxModem::CLASS1;
    else if (strcasecmp(cp, "Class2") == 0)
	return FaxModem::CLASS2;
    else
	return FaxModem::UNKNOWN;
}

const char*
faxAdmin::getNameFromModemType(const ModemType type)
{
    switch (type) {
    case FaxModem::ABATON:	return "Abaton";
    case FaxModem::EV958:	return "Ev958";
    case FaxModem::EV968:	return "Ev968";
    case FaxModem::CLASS1:	return "Class1";
    case FaxModem::CLASS2:	return "Class2";
    case FaxModem::UNKNOWN:	break;
    }
    return "Unknown";
}

static const char* putBoolean(fxBool b)
    { return (b ? "yes" : "no"); }
static void putLine(FILE* fd, const char* tag, const fxStr& v)
    { if (v != "") fprintf(fd, "%s: %s\n", tag, (char*) v); }
static void putLine(FILE* fd, const char* tag, const char* v)
    { fprintf(fd, "%s: %s\n", tag, v); }
static void putLine(FILE* fd, const char* tag, int v)
    { fprintf(fd, "%s: %d\n", tag, v); }
static void putLine(FILE* fd, const char* tag, fxMultiChoice& c)
    { fprintf(fd, "%s: %s\n", tag, putBoolean(!c.getCurrentButton())); }

void
faxAdmin::writeConfiguration()
{
    if (isDirty) {
	u_int l = device.length();
	const fxStr& dev = device.tokenR(l, '/');
	saveState(queueDir | "/" | configName | "." | dev);
	isDirty = FALSE;
    }
}

void
faxAdmin::saveState(const char* file)
{
    FILE* fd = fopen(file, "w");
    if (fd != NULL) {
	char buf[256];
	time_t now = time(0);
	fprintf(fd, "# last written %s", ctime(&now));
	if (modemType != FaxModem::UNKNOWN)
	    putLine(fd, "ModemType",	getNameFromModemType(modemType));
	putLine(fd, "DialPrefix",	prefix);
	putLine(fd, "UseDialPrefix",	putBoolean(usePrefix));
	putLine(fd, "AreaCode",		areaCode);
	putLine(fd, "CountryCode",	countryCode);
	putLine(fd, "LongDistancePrefix",longDistancePrefix);
	putLine(fd, "InternationalPrefix",internationalPrefix);
	putLine(fd, "FAXNumber",	faxNumber);
	putLine(fd, "VoiceNumber",	voiceNumber);
	putLine(fd, "SpeakerVolume",	speakerVolume);
	putLine(fd, "ProtocolTracing",  protocolTracing);
	putLine(fd, "ToneDialing",	putBoolean(toneDialing));
	putLine(fd, "RingsBeforeAnswer",rings);
	putLine(fd, "CommaPauseTime",	pauseTime);
	putLine(fd, "WaitForCarrier",	waitTime);
	putLine(fd, "TagLineFormat",	tagLineFormat);
	putLine(fd, "TagLineFont",	tagLineFont);
	putLine(fd, "TagLineAtTop",	putBoolean(tagLineAtTop));
	fprintf(fd, "RecvFileMode: 0%o\n", recvFileMode);
	putLine(fd, "QualifyTSI",	putBoolean(qualifyTSI));
	fclose(fd);
    }
}
