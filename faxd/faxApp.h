/*	$Header: /usr/people/sam/fax/./faxd/RCS/faxApp.h,v 1.9 1995/04/08 21:31:13 sam Rel $ */
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
#ifndef _faxApp_
#define	_faxApp_
/*
 * HylaFAX Application Support.
 */
#include "Str.h"
#include <stdarg.h>

class faxApp {
private:
    static fxStr getopts;		// main arguments
    static int	facility;		// syslog facility

    fxBool	running;		// server running
protected:
    int		openFIFO(const char* fifoName, int mode,
		    fxBool okToExist = FALSE);
    void	setRealIDs();
public:
    faxApp();
    virtual ~faxApp();

    static void setupPermissions(void);
    static void detachFromTTY(void);
    static void setupLogging(const char* appName);
    static void setLogFacility(const char* facility);
    static int getLogFacility(void);

    virtual void initialize(int argc, char** argv);
    virtual void open(void);
    virtual void close(void);

    fxBool isRunning(void) const;

    virtual void openFIFOs(void);
    virtual void closeFIFOs(void);
    virtual int FIFOInput(int);
    virtual void FIFOMessage(const char* mesage);

    static void setOpts(const char*);
    static const fxStr& getOpts(void);

    fxBool runCmd(const char* cmd, fxBool changeIDs = FALSE);
};
inline fxBool faxApp::isRunning(void) const	{ return running; }
inline int faxApp::getLogFacility(void)		{ return facility; }

class GetoptIter {
private:
    const fxStr& opts;
    int		argc;
    char**	argv;
    int		c;
public:
    GetoptIter(int ac, char** av, const fxStr& s);
    ~GetoptIter();

    void operator++();
    void operator++(int);
    int option() const;
    fxBool notDone() const;
    const char* optArg() const;
    const char* getArg();
    const char* nextArg();
};
inline int GetoptIter::option() const		{ return c; }
inline fxBool GetoptIter::notDone() const	{ return c != -1; }

extern void logError(const char* fmt ...);
extern void logInfo(const char* fmt ...);
extern void vlogError(const char* fmt, va_list ap);
extern void vlogInfo(const char* fmt, va_list ap);
extern void fxFatal(const char* fmt ...);
#endif
