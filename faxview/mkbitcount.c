#ident $Header: /d/sam/flexkit/fax/faxview/RCS/mkbitcount.c,v 1.2 91/05/23 12:33:16 sam Exp $

/*
 * Copyright (c) 1991 by Sam Leffler.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */
unsigned char tab0[256][8];
unsigned char tab1[256][8];

dumparray(char* name, unsigned char tab[256][8])
{
    register int i, sx;
    register char *sep;
    printf("static u_char\t%s[%d][%d] = {\n", name, 8, 256);
    sep = "{   ";
    for (sx = 0; sx < 8; sx++) {
	for (i = 0; i < 256; i++) {
	    printf("%s%d", sep, tab[i][sx]);
	    if (((i + 1) % 16) == 0) {
		printf(",	/* %02x - %02x */\n", i-15, i);
		sep = "    ";
	    } else
		sep = ", ";
	}
	printf("},\n");
	sep = "{   ";
    }
    printf("};\n");
}

main()
{
    int b;
    static unsigned short bitcount[8] =
	{ 0, 1, 1, 2, 1, 2, 2, 3, };
    for (b = 0; b < 256; b++) {
	tab0[b][0] = bitcount[(b>>5)&7]; tab1[b][0] = 0;
	tab0[b][1] = bitcount[(b>>4)&7]; tab1[b][1] = 0;
	tab0[b][2] = bitcount[(b>>3)&7]; tab1[b][2] = 0;
	tab0[b][3] = bitcount[(b>>2)&7]; tab1[b][3] = 0;
	tab0[b][4] = bitcount[(b>>1)&7]; tab1[b][4] = 0;
	tab0[b][5] = bitcount[(b>>0)&7]; tab1[b][5] = 0;
	tab0[b][6] = bitcount[(b>>0)&3]; tab1[b][6] = bitcount[(b>>7)&7];
	tab0[b][7] = bitcount[(b>>0)&1]; tab1[b][7] = bitcount[(b>>6)&7];
    }
    printf("// warning, this file was automatically generated\n");
    dumparray("bitcount0", tab0);
    dumparray("bitcount1", tab1);
    exit(0);
}
