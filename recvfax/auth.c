#ident	$Header: /usr/people/sam/flexkit/fax/recvfax/RCS/auth.c,v 1.1 91/05/31 10:09:31 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "defs.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

static char*
topdomain(char* h)
{
    register char* p;
    char* maybe = NULL;
    int dots = 0;

    for (p = h + strlen(h); p >= h; p--)
	if (*p == '.') {
	    if (++dots == 2)
		return (p);
	    maybe = p;
	}
    return (maybe);
}

/*
 * Check whether host h is in our local domain,
 * defined as sharing the last two components of the domain part,
 * or the entire domain part if the local domain has only one component.
 * If either name is unqualified (contains no '.'),
 * assume that the host is local, as it will be
 * interpreted as such.
 */
static int
local_domain(char* h)
{
    char localhost[80];
    char* p1;
    char* p2;

    localhost[0] = 0;
    (void) gethostname(localhost, sizeof (localhost));
    p1 = topdomain(localhost);
    p2 = topdomain(h);
    return (p1 == NULL || p2 == NULL || !strcasecmp(p1, p2));
}

void
checkPermission()
{
    static int alreadyChecked = 0;
    int sinlen;
    char* dotaddr;
    struct sockaddr_in sin;
    FILE* db;

    if (alreadyChecked)
	return;
    alreadyChecked = 1;
    sinlen = sizeof (sin);
    if (getpeername(fileno(stdin), &sin, &sinlen) < 0) {
	syslog(LOG_ERR, "getpeername: %m");
	sendError("Can not get your network address");
	if (debug)
	    syslog(LOG_DEBUG, "EXIT");
	exit(-1);
    }
    dotaddr = inet_ntoa(sin.sin_addr);
    db = fopen(FAX_PERMFILE, "r");
    if (db) {
	struct hostent* hp;
	char* hostname = NULL;
	char line[1024];

	hp = gethostbyaddr(&sin.sin_addr, sizeof (sin.sin_addr),
		sin.sin_family);
	if (hp) {
	    /*
	     * If name returned by gethostbyaddr is in our domain,
	     * attempt to verify that we haven't been fooled by someone
	     * in a remote net; look up the name and check that this
	     * address corresponds to the name.
	     */
	    if (local_domain(hp->h_name)) {
		strncpy(line, hp->h_name, sizeof (line) - 1);
		line[sizeof (line) - 1] = '\0';
		hp = gethostbyname(line);
		if (hp) {
		    for (; hp->h_addr_list[0] != NULL; hp->h_addr_list++) {
			if (bcmp(hp->h_addr_list[0], (caddr_t) &sin.sin_addr,
			  hp->h_length) == 0) {
			    hostname = hp->h_name;	/* accept name */
			    break;
			}
		    }
		    if (hostname == NULL) {
			sendError(
		    "Your host address \"%s\" is not listed for host \"%s\"",
			    dotaddr, hp->h_name);
			syslog(LOG_ERR,
			  "Host address \"%s\" not listed for host \"%s\"",
			    dotaddr, hp->h_name);
		    }
		} else {
		    sendError("Could not find the address for \"%s\"", line);
		    syslog(LOG_ERR, "Could not look up address for \"%s\"",
			line);
		}
	    } else
		hostname = hp->h_name;
	} else {
	    sendError("Can not map your network address to a hostname");
	    syslog(LOG_ERR, "Could not look up hostname for \"%s\"", dotaddr);
	}
	/*
	 * Now check the host name/address against
	 * the list of hosts that are permitted to
	 * submit jobs.
	 */
	while (fgets(line, sizeof (line)-1, db)) {
	    char* cp = cp = strchr(line, '#');
	    if (cp || (cp = strchr(line, '\n')))
		*cp = '\0';
	    /* trim off trailing white space */
	    for (cp = strchr(line, '\0'); cp > line; cp--)
		if (!isspace(cp[-1]))
		    break;
	    *cp = '\0';
	    if (strcmp(line, dotaddr) == 0 ||
	      (hostname != NULL && strcasecmp(line, hostname) == 0))
		return;
	}
	fclose(db);
    } else
	sendError("The server does not have a permissions file");
    syslog(LOG_ERR, "%s: Service refused", dotaddr);
    sendError("Your host does not have permission to use the fax server");
    if (debug)
	syslog(LOG_DEBUG, "EXIT");
    exit(-1);
}
