/*	$Id: Login.c++ 721 2007-11-26 21:59:09Z faxguy $ */
/*
 * Copyright (c) 1995-1996 Sam Leffler
 * Copyright (c) 1995-1996 Silicon Graphics, Inc.
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
#include "HylaFAXServer.h"
#include "Sys.h"

#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#if HAS_CRYPT_H
#include <crypt.h>
#endif

void
HylaFAXServer::loginRefused(const char* why)
{
    if (++loginAttempts >= maxLoginAttempts) {
	reply(530, "Login incorrect (closing connection).");
	logNotice("Repeated login failures for user %s from %s [%s]"
	    , (const char*) the_user
	    , (const char*) remotehost
	    , (const char*) remoteaddr
	);
	dologout(0);
    } else {
	reply(530, "User %s access denied.", (const char*) the_user);
	logNotice("HylaFAX LOGIN REFUSED (%s) FROM %s [%s], %s"
	    , why
	    , (const char*) remotehost
	    , (const char*) remoteaddr
	    , (const char*) the_user
	);
    }
}

/*
 * USER command. Sets global passwd state if named
 * account exists and is acceptable; sets askpasswd if a
 * PASS command is expected.  If logged in previously,
 * need to reset state.  User account must be accessible
 * from client host according to the contents of the
 * userAccessFile.
 */
void
HylaFAXServer::userCmd(const char* name)
{
    if (IS(LOGGEDIN)) {
	if (IS(PRIVILEGED) && the_user == name) {// revert to unprivileged mode
	    state &= ~S_PRIVILEGED;
	    reply(230, "User %s logged in.", name);
	    return;
	}
        end_login();
    }
    the_user = name;
    state &= ~S_PRIVILEGED;
    adminWd = "*";			// make sure no admin privileges
    passWd = "*";			// just in case...

    if (checkUser(name)) {
	if (passWd != "") {
	    state |= S_WAITPASS;
	    reply(331, "Password required for %s.", name);
	    /*
	     * Delay before reading passWd after first failed
	     * attempt to slow down password-guessing programs.
	     */
	    if (loginAttempts)
		sleep(loginAttempts);
	} else
	    login(230);
    } else
	loginRefused("user denied");
}

#ifdef HAVE_PAM
int
pamconv(int num_msg, STRUCT_PAM_MESSAGE **msg, struct pam_response **resp, void *appdata)
{
	/*
	 * This PAM conversation function expects that the PAM modules
	 * being used will only have one message.  If this expectation
	 * is not met then this will fail, and this will need modification
	 * in order to work.
	 */

	char *password =(char*) appdata;
	struct pam_response* replies;

	if (num_msg != 1 || msg[0]->msg_style != PAM_PROMPT_ECHO_OFF)
	    return PAM_CONV_ERR;

	if (password == NULL) {
	    /*
	     * Solaris doesn't have PAM_CONV_AGAIN defined.
	     */
	    #ifdef PAM_CONV_AGAIN
		return PAM_CONV_AGAIN;
	    #else
		return PAM_CONV_ERR;
	    #endif
	}

	replies=(struct pam_response*)calloc(num_msg, sizeof(struct pam_response));

	replies[0].resp = password;
	replies[0].resp_retcode = 0;
	*resp = replies;

	return (PAM_SUCCESS);
}
#endif //HAVE_PAM

bool
HylaFAXServer::pamIsAdmin(const char* user)
{
	bool retval = false;
#ifdef HAVE_PAM
	int i;
	static struct group* grinfo = getgrnam(admingroup);
	const char *curruser = (user == NULL ? the_user.c_str() : user);
	if (grinfo != NULL) {
		for (i=0; grinfo->gr_mem[i] != NULL; i++) {
			if (strcmp(curruser, grinfo->gr_mem[i]) == 0) retval = true;
		}
	}
#endif //HAVE_PAM
	return(retval);
}

bool
HylaFAXServer::pamCheck(const char* user, const char* pass)
{
	bool retval = false;
#ifdef HAVE_PAM

	if (user == NULL) user = the_user;
	if (pass == NULL) pass = passWd.c_str();

	/*
	 * The effective uid must be privileged enough to
	 * handle whatever the PAM module may require.
	 */
	uid_t ouid = geteuid();
	(void) seteuid(0);

	pam_handle_t *pamh;
	struct pam_conv conv = {pamconv, NULL};
	conv.appdata_ptr = strdup(pass);

	int pamret = pam_start(FAX_SERVICE, user, &conv, &pamh);
	if (pamret == PAM_SUCCESS) {
	    pamret = pam_set_item(pamh, PAM_RHOST, remoteaddr);
	    if (pamret == PAM_SUCCESS) {
		pamret = pam_set_item(pamh, PAM_CONV, (const void*)&conv);
		if (pamret == PAM_SUCCESS) {
#ifdef PAM_INCOMPLETE
		    u_int tries = 10;
		    do {
			/*
			 * PAM supports event-driven applications by returning PAM_INCOMPLETE
			 * and requiring the application to recall pam_authenticate after the
			 * underlying PAM module is ready.  The application is supposed to
			 * utilize the pam_conv structure to determine when the authentication
			 * module is ready.  However, in our case we're not event-driven, and
			 * so we can wait, and a call to sleep saves us the headache.
			 */
#endif // PAM_INCOMPLETE
			pamret = pam_authenticate(pamh, 0);
#ifdef PAM_INCOMPLETE
		    } while (pamret == PAM_INCOMPLETE && --tries && !sleep(1));
#endif // PAM_INCOMPLETE
		    if (pamret == PAM_SUCCESS) {
			pamret = pam_acct_mgmt(pamh, 0);
			if (pamret == PAM_SUCCESS) {
			    retval = true;
			} else
			    logNotice("pam_acct_mgmt failed in pamCheck with 0x%X: %s", pamret, pam_strerror(pamh, pamret));
		    } else
			logNotice("pam_authenticate failed in pamCheck with 0x%X: %s", pamret, pam_strerror(pamh, pamret));
		} else
		    logNotice("pam_set_item (PAM_CONV) failed in pamCheck with 0x%X: %s", pamret, pam_strerror(pamh, pamret));
	    } else
		logNotice("pam_set_item (PAM_RHOST) failed in pamCheck with 0x%X: %s", pamret, pam_strerror(pamh, pamret));
	} else
	    logNotice("pam_start failed in pamCheck with 0x%X: %s", pamret, pam_strerror(pamh, pamret));

	pamEnd(pamh, pamret);
	(void) seteuid(ouid);
#endif
	return retval;
}

#ifdef HAVE_PAM
void HylaFAXServer::pamEnd(pam_handle_t *pamh, int pamret)
{
    if (pamret == PAM_SUCCESS)
    {
	if (pamIsAdmin())
	    state |= S_PRIVILEGED;

	char *newname=NULL;

	/*
	 * Solaris has proprietary pam_[sg]et_item() extension.
	 * Sun defines PAM_MSG_VERSION therefore is possible to use
	 * it in order to recognize the extensions of Solaris
	 */
	#ifdef PAM_MSG_VERSION
	    pamret = pam_get_item(pamh, PAM_USER, (void **)&newname);
	#else
	    pamret = pam_get_item(pamh, PAM_USER, (const void **)&newname);
	#endif

	if (pamret == PAM_SUCCESS && newname != NULL)
	    the_user = strdup(newname);

	struct passwd* uinfo=getpwnam((const char *)the_user);
	if (uinfo != NULL) {
	    uid = uinfo->pw_uid;
	}	

    }
    pamret = pam_end(pamh, pamret);
    pamh = NULL;
}
#endif //HAVE_PAM

void
HylaFAXServer::passCmd(const char* pass)
{
    if (IS(LOGGEDIN)) {
        reply(503, "Already logged in as USER %s.", (const char*) the_user);
        return;
    }
    if (!IS(WAITPASS)) {
        reply(503, "Login with USER first.");
        return;
    }
    state &= ~S_WAITPASS;

    /*
     * Disable long reply messages for old (broken) FTP
     * clients if the first character of the password
     * is a ``-''.
     */
    if (pass[0] == '-') {
	state &= ~S_LREPLIES;
	pass++;
    } else
	state |= S_LREPLIES;
    if (pass[0] == '\0' || !(strcmp(crypt(pass, passWd), passWd) == 0 || pamCheck(the_user, pass))) {
	if (++loginAttempts >= maxLoginAttempts) {
	    reply(530, "Login incorrect (closing connection).");
	    logNotice("Repeated login failures for user %s from %s [%s]"
		, (const char*) the_user
		, (const char*) remotehost
		, (const char*) remoteaddr
	    );
	    dologout(0);
	}
	reply(530, "Login incorrect.");
	logInfo("Login failed from %s [%s], %s"
	    , (const char*) remotehost
	    , (const char*) remoteaddr
	    , (const char*) the_user
	);
	return;
    }
    login(230);
}

/*
 * Login is shared between SNPP and HylaFAX RFC 959 protocol,
 * but different protocols use different codes for success.
 * We make the caller tell us what success is.
 */
void
HylaFAXServer::login(int code)
{
    loginAttempts = 0;		// this time successful
    state |= S_LOGGEDIN;

    uid_t ouid = geteuid();
    (void) seteuid(0);
    bool isSetup = (chroot(".") >= 0 && chdir("/") >= 0);
    /*
     * Install the client's fax-uid as the effective gid
     * so that created files automatically are given the
     * correct ``ownership'' (we store the owner's fax-uid
     * in the group-id field of the inode).
     */
    if (isSetup)
	(void) setegid(uid);
    (void) seteuid(ouid);
    if (!isSetup) {
	reply(550, "Cannot set privileges.");
	end_login();
	return;
    }

#ifdef HAVE_PAM
    pam_chrooted = true;
#endif

    (void) isShutdown(false);	// display any shutdown messages
    reply(code, "User %s logged in.", (const char*) the_user);
    if (TRACE(LOGIN))
	logInfo("FAX LOGIN FROM %s [%s], %s"
	    , (const char*) remotehost
	    , (const char*) remoteaddr
	    , (const char*) the_user
	);
    (void) umask(077);
    if (tracingLevel & (TRACE_INXFERS|TRACE_OUTXFERS))
        xferfaxlog = Sys::open(xferfaxLogFile, O_WRONLY|O_APPEND|O_CREAT, 0600);

    initDefaultJob();		// setup connection-related state
    dirSetup();			// initialize directory handling
	if (pamIsAdmin()) state |= S_PRIVILEGED;
}

void
HylaFAXServer::adminCmd(const char* pass)
{
    fxAssert(IS(LOGGEDIN), "ADMIN command permitted when not logged in");
    // NB: null adminWd is permitted
    if ((strcmp(crypt(pass, adminWd), adminWd) != 0) && !pamIsAdmin()) {
	if (++adminAttempts >= maxAdminAttempts) {
	    reply(530, "Password incorrect (closing connection).");
	    logNotice("Repeated admin failures from %s [%s]"
		, (const char*) remotehost
		, (const char*) remoteaddr
	    );
	    dologout(0);
	} else {
	    reply(530, "Password incorrect.");
	    logInfo("ADMIN failed from %s [%s], %s"
		, (const char*) remotehost
		, (const char*) remoteaddr
		, (const char*) the_user
	    );
	}
	return;
    }
    if (TRACE(SERVER))
	logInfo("FAX ADMIN FROM %s [%s], %s"
	    , (const char*) remotehost
	    , (const char*) remoteaddr
	    , (const char*) the_user
	);
    adminAttempts = 0;
    state |= S_PRIVILEGED;
    reply(230, "Administrative privileges established.");
}

/*
 * Terminate login as previous user, if any,
 * resetting state; used when USER command is
 * given or login fails.
 */
void
HylaFAXServer::end_login(void)
{
    if (IS(LOGGEDIN)) {
	uid_t ouid = geteuid();
	seteuid(0);
	seteuid(ouid);
    }
    state &= ~(S_LOGGEDIN|S_PRIVILEGED|S_WAITPASS);
    passWd = "*";
    adminWd = "*";
}

/*
 * Record logout in wtmp file, cleanup state,
 * and exit with supplied status.
 */
void
HylaFAXServer::dologout(int status)
{
    if (IS(LOGGEDIN))
	end_login();
    if (trigSpec != "") {
	fxStr emsg;
	cancelTrigger(emsg);
    }
    for (u_int i = 0, n = tempFiles.length(); i < n; i++)
	(void) Sys::unlink(tempFiles[i]);
    if (xferfaxlog != -1)
        Sys::close(xferfaxlog);
    if (clientFd != -1)
	Sys::close(clientFd);
    if (clientFIFOName != "")
	Sys::unlink("/" | clientFIFOName);
    for (JobDictIter iter(blankJobs); iter.notDone(); iter++) {
	Job* job = iter.value();
	fxStr file("/" | job->qfile);
	Sys::unlink(file);
    }
    _exit(status);		// beware of flushing buffers after a SIGPIPE
}
