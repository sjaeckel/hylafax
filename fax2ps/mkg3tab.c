#ifndef lint
static char rcsid[] = "$Header: /d/sam/flexkit/fax/fax2ps/RCS/mkg3tab.c,v 1.1 91/05/17 20:45:01 sam Exp $";
#endif

#include "tiffcompat.h"
#include <stdio.h>
#define	G3CODES
#include "t4.h"

#define	TABSIZE	8192

dumparray(name, tab)
	char *name;
	u_char tab[TABSIZE];
{
	register int i;
	register char *sep;
	printf("u_char\t%s[%d] = {\n", name, TABSIZE);
	sep = "    ";
	for (i = 0; i < TABSIZE; i++) {
		printf("%s%3d", sep, tab[i]);
		if (((i + 1) % 10) == 0) {
			printf(",	/* %4d - %4d */\n", i-9, i);
			sep = "    ";
		} else
			sep = ", ";
	}
	if ((i-1) % TABSIZE)
		putchar('\n');
	printf("};\n");
}

#define	SIZEOF(a)	(sizeof (a) / sizeof (a[0]))

addcodes(tab, n, ttab)
	u_char tab[TABSIZE];
	int n;
	tableentry *ttab;
{
	int i;

	for (i = 0; i < n; i++) {
		tableentry *te = &ttab[i];
		int code = te->code << (13-te->length);
		if (code >= TABSIZE) {
			fprintf(stderr,
			    "Unexpected code %d (>=%d) (0x%x,%d,%d)\n",
			    code, TABSIZE,
			    te->code, te->length, te->runlen);
			exit(-1);
		}
		if (tab[code] != 0xff) {
			fprintf(stderr,
			    "Code table collision %d (0x%x,%d,%d)\n",
			    code, te->code, te->length, te->runlen);
		} else
			tab[code] = i;
	}
}

bfill(cp, n, v)
	register u_char *cp;
	register int n;
	register int v;
{
	while (n-- > 0)
		*cp++ = v;
}

main()
{
	u_char tab[TABSIZE];

	bfill(tab, sizeof (tab), 0xff);
	addcodes(tab, SIZEOF(TIFFFaxBlackCodes), TIFFFaxBlackCodes);
	dumparray("TIFFFax3btab", tab);
	bfill(tab, sizeof (tab), 0xff);
	addcodes(tab, SIZEOF(TIFFFaxWhiteCodes), TIFFFaxWhiteCodes);
	dumparray("TIFFFax3wtab", tab);
	exit(0);
}
