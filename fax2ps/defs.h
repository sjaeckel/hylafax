#ifndef _DEFS_
#define	_DEFS_

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
#include "tiffioP.h"

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif

typedef	struct {
    unsigned short count;	/* frequency count (maybe long?) */
    unsigned short code;	/* assigned code */
    unsigned long cost;		/* cost to use w/o encoding */
} Code;

typedef struct {
    Code	c;
    unsigned short move;	/* relative move before */
    unsigned short runlen;	/* runlen draw */
} CodeEntry;
#define	CODEHASH	8209
#define	HASHCODE(x,y)	((((x)<<16)|(y)) % CODEHASH)
extern	CodeEntry* codehash[CODEHASH];
#define	MAXCODES	6000	/* about 75% population */
extern	CodeEntry codetable[MAXCODES];

typedef struct {
    Code	c;
    CodeEntry*	a;		/* first part of pair */
    CodeEntry*	b;		/* second part of pair */
} CodePairEntry;
#define	PAIRHASH	16033
#define	HASHPAIR(a,b)	((((u_int)(a)<<16)|((u_int)(b)&0xffff)) % PAIRHASH)
extern	CodePairEntry* pairhash[PAIRHASH];
#define	MAXPAIRS	10000
extern	CodePairEntry pairtable[MAXPAIRS];
int	npairs;
#define	isPair(p) \
    (&pairtable[0] <= (CodePairEntry*)(p) && \
     (CodePairEntry*)(p) < &pairtable[MAXPAIRS])

extern	char** codeNames;	/* codeNames[code] => ASCII code name */
extern	char* codeNameSpace;	/* storage space for codeNames and strings */
extern	int ncodes;		/* number of assigned codes */
extern	int includeStatistics;	/* if 1, add comments w/ frequency stats */
extern	int startOfRow;		/* if 1, have yet to emit a code for this row */
extern	int dopairs;		/* if 1, encode pairs of codes */
extern	int debug;		/* debug decoding */

extern	CodeEntry* enterCode(int dx, int len);
extern	CodePairEntry* findPair(CodeEntry* a, CodeEntry* b);
extern	CodePairEntry* enterPair(CodeEntry* a, CodeEntry* b);
extern	int printPair(TIFF* tif, CodeEntry* a, CodeEntry* b);

#define	USE_PROTOTYPES	1
#include "tif_fax3.h"

typedef struct {
    int		cc;
    u_char*	buf;
    u_char*	bp;
    u_char*	scanline;
    u_short	pass;
    u_short	is2d;
    CodeEntry*	lastCode;
    Fax3BaseState b;
} Fax3DecodeState;
extern	Fax3DecodeState fax;

extern	int Fax3DecodeRow(TIFF* tif, int npels);
#endif	_DEFS_
