/*	$Header: /usr/people/sam/fax/./faxd/RCS/FaxMachineInfo.h,v 1.26 1995/04/08 21:30:09 sam Rel $ */
/*
 * Copyright (c) 1990-1995 Sam Leffler
 * Copyright (c) 1991-1995 Silicon Graphics, Inc.
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
#ifndef _FaxMachineInfo_
#define	_FaxMachineInfo_
/*
 * Fax Machine Information Database Support.
 */
#include "Str.h"
#include "FaxConfig.h"
#include <stdarg.h>

/*
 * Each remote machine the server sends a facsimile to
 * has information that describes capabilities that are
 * important in formatting outgoing documents, and, potentially,
 * controls on what the server should do when presented
 * with documents to send to the destination.  The capabilities
 * are treated as a cache; information is initialized to
 * be a minimal set of capabilities that all machines are
 * required (by T.30) to support and then updated according
 * to the DIS/DTC messages received during send operations.
 */
class FaxMachineInfo : public FaxConfig {
private:
    fxStr	file;			// pathname to info file
    u_int	locked;			// bit vector of locked items
    fxBool	changed;		// changed since restore
    fxBool	supportsHighRes;	// capable of 7.7 line/mm vres
    fxBool	supports2DEncoding;	// handles Group 3 2D
    fxBool	supportsPostScript;	// handles Adobe NSF protocol
    fxBool	calledBefore;		// successfully called before
    int		maxPageWidth;		// max capable page width
    int		maxPageLength;		// max capable page length
    int		maxSignallingRate;	// max capable signalling rate
    int		minScanlineTime;	// min scanline time capable
    fxStr	csi;			// last received CSI
    int		sendFailures;		// count of failed send attempts
    int		dialFailures;		// count of failed dial attempts
    fxStr	lastSendFailure;	// reason for last failed send attempt
    fxStr	lastDialFailure;	// reason for last failed dial attempt
    u_int	pagerMaxMsgLength;	// max text message length for pages
    fxStr	pagerPassword;		// pager service password string

    static const fxStr infoDir;

    void writeConfig(FILE*);

    fxBool setConfigItem(const char* tag, const char* value);
    void vconfigError(const char* fmt0, va_list ap);
    void configError(const char* fmt0 ...);
    void configTrace(const char* fmt0 ...);
    void error(const char* fmt0 ...);
public:
    FaxMachineInfo();
    FaxMachineInfo(const FaxMachineInfo& other);
    virtual ~FaxMachineInfo();

    virtual fxBool updateConfig(const fxStr& filename);
    virtual void writeConfig();
    virtual void resetConfig();

    fxBool getSupportsHighRes() const;
    fxBool getSupports2DEncoding() const;
    fxBool getSupportsPostScript() const;
    fxBool getCalledBefore() const;
    int getMaxPageWidthInPixels() const;
    int getMaxPageWidthInMM() const;
    int getMaxPageLengthInMM() const;
    int getMaxSignallingRate() const;
    int getMinScanlineTime() const;
    const fxStr& getCSI() const;

    int getSendFailures() const;
    int getDialFailures() const;
    const fxStr& getLastSendFailure() const;
    const fxStr& getLastDialFailure() const;

    void setSupportsHighRes(fxBool);
    void setSupports2DEncoding(fxBool);
    void setSupportsPostScript(fxBool);
    void setCalledBefore(fxBool);
    void setMaxPageWidthInPixels(int);
    void setMaxPageLengthInMM(int);
    void setMaxSignallingRate(int);
    void setMinScanlineTime(int);
    void setCSI(const fxStr&);

    void setSendFailures(int);
    void setDialFailures(int);
    void setLastSendFailure(const fxStr&);
    void setLastDialFailure(const fxStr&);

    u_int getPagerMaxMsgLength() const;
    const fxStr& getPagerPassword() const;
};

inline fxBool FaxMachineInfo::getSupportsHighRes() const
    { return supportsHighRes; }
inline fxBool FaxMachineInfo::getSupports2DEncoding() const
    { return supports2DEncoding; }
inline fxBool FaxMachineInfo::getSupportsPostScript() const
    { return supportsPostScript; }
inline fxBool FaxMachineInfo::getCalledBefore() const	
    { return calledBefore; }
inline int FaxMachineInfo::getMaxPageWidthInPixels() const
    { return maxPageWidth; }
inline int FaxMachineInfo::getMaxPageLengthInMM() const
    { return maxPageLength; }
inline int FaxMachineInfo::getMaxSignallingRate() const
    { return maxSignallingRate; }
inline int FaxMachineInfo::getMinScanlineTime() const
    { return minScanlineTime; }
inline const fxStr& FaxMachineInfo::getCSI() const
    { return csi; }

inline int FaxMachineInfo::getSendFailures() const
    { return sendFailures; }
inline int FaxMachineInfo::getDialFailures() const
    { return dialFailures; }
inline const fxStr& FaxMachineInfo::getLastSendFailure() const
    { return lastSendFailure; }
inline const fxStr& FaxMachineInfo::getLastDialFailure() const
    { return lastDialFailure; }

inline u_int FaxMachineInfo::getPagerMaxMsgLength() const
    { return pagerMaxMsgLength; }
inline const fxStr& FaxMachineInfo::getPagerPassword() const
    { return pagerPassword; }
#endif /* _FaxMachineInfo_ */
