/*	$Header: /usr/people/sam/fax/./util/RCS/FaxClient.h,v 1.25 1995/04/08 21:44:03 sam Rel $ */
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
#ifndef _FaxClient_
#define	_FaxClient_

#include "Types.h"
#include "Str.h"
#include "IOHandler.h"

typedef unsigned int FaxClientRC;
struct hash_t;			// NB: for busted DEC C++ compiler

class FaxClient : public IOHandler {
private:
    fxStr	host;		// server's host
    fxStr	modem;		// server's modem
    fxBool	verbose;	// print data as sent or received
    fxBool	running;	// client is still running
    fxBool	peerdied;	// server went away
    fxBool	trusted;	// permitted to do trusted-kinds of things
    fxStr	userName;	// sender's account name
    fxStr	senderName;	// sender's full name (if available)
    int		fd;		// input socket
    int		fdOut;		// normally same as fd
    char	buf[1024];	// input buffer
    int		prevcc;		// previous unprocessed input
    u_int	version;	// protocol version
    fxStr	proto;		// protocol to use for service query
    int		port;		// server port to connect to

    void init();

    struct hash_t* hashtab;	// LZW encoder hash table
    void clearstate(void);
    fxBool sendLZWData(int fdIn, int cc);

    fxBool sendRawData(void* buf, int cc);
protected:
    FaxClient();
    FaxClient(const fxStr& hostarg);
    FaxClient(const char* hostarg);
    ~FaxClient();

    virtual fxBool setupUserIdentity();
    virtual void setProtocolVersion(u_int);
    virtual void setupHostModem(const char*);
    virtual void setupHostModem(const fxStr&);

    virtual void startRunning();
    virtual void stopRunning();

    virtual void recvConf(const char* cmd, const char* tag) = 0;
    virtual void recvEof() = 0;
    virtual void recvError(const int err) = 0;

    virtual void printError(const char* fmt, ...) = 0;
    virtual void printWarning(const char* va_alist ...) = 0;
public:
    fxBool isRunning() const;
    fxBool getPeerDied() const;
    fxBool isTrusted() const;

    // bookkeeping
    void setHost(const fxStr&);
    void setHost(const char*);
    void setPort(int);
    void setProtoName(const char*);
    const fxStr& getHost() const;
    virtual void setModem(const fxStr&);
    virtual void setModem(const char*);
    const fxStr& getModem() const;
    virtual fxBool callServer();
    virtual fxBool hangupServer();
    virtual void setFds(const int in, const int out);

    void setVerbose(fxBool);
    fxBool getVerbose() const;

    u_int getProtocolVersion() const;

    const fxStr& getSenderName() const;
    const fxStr& getUserName() const;

    // output
    virtual fxBool sendData(const char* type, const char* filename);
    virtual fxBool sendLZWData(const char* type, const char* filename);
    virtual fxBool sendLine(const char* cmd);
    virtual fxBool sendLine(const char* cmd, int v);
    virtual fxBool sendLine(const char* cmd, const fxStr& s);
    virtual fxBool sendLine(const char* cmd, const char* tag);

    // input
    virtual int inputReady(int);
};
inline fxBool FaxClient::isRunning() const		{ return running; }
inline fxBool FaxClient::getPeerDied() const		{ return peerdied; }
inline fxBool FaxClient::isTrusted() const		{ return trusted; }
inline const fxStr& FaxClient::getSenderName() const	{ return senderName; }
inline const fxStr& FaxClient::getUserName() const	{ return userName; }
inline u_int FaxClient::getProtocolVersion() const	{ return version; }
inline const fxStr& FaxClient::getHost() const		{ return host; }
inline const fxStr& FaxClient::getModem() const		{ return modem; }
inline fxBool FaxClient::getVerbose() const		{ return verbose; }

extern void fxFatal(const char* fmt, ...);
#endif /* _FaxClient_ */
