#include "gl.h"

main()
{
    printf("xmax %ld ymax %ld zmin %ld zmax %ld\n",
	getgdesc(GD_XPMAX), getgdesc(GD_YPMAX),
	getgdesc(GD_ZMIN), getgdesc(GD_ZMAX));
    printf("normal single red %ld green %ld blue %ld\n",
	getgdesc(GD_BITS_NORM_SNG_RED),
	getgdesc(GD_BITS_NORM_SNG_GREEN),
	getgdesc(GD_BITS_NORM_SNG_BLUE));
    printf("normal double red %ld green %ld blue %ld\n",
	getgdesc(GD_BITS_NORM_DBL_RED),
	getgdesc(GD_BITS_NORM_DBL_GREEN),
	getgdesc(GD_BITS_NORM_DBL_BLUE));
    printf("normal cmode single %ld double %ld\n",
	getgdesc(GD_BITS_NORM_SNG_CMODE),
	getgdesc(GD_BITS_NORM_DBL_CMODE));
    printf("normal mmap single %ld double %ld\n",
	getgdesc(GD_BITS_NORM_SNG_MMAP),
	getgdesc(GD_BITS_NORM_DBL_MMAP));
    printf("normal zbuffer %ld\n", getgdesc(GD_BITS_NORM_ZBUFFER));
    printf("single cmode over %ld under %ld pup %ld\n",
	getgdesc(GD_BITS_OVER_SNG_CMODE),
	getgdesc(GD_BITS_UNDR_SNG_CMODE),
	getgdesc(GD_BITS_PUP_SNG_CMODE));
    printf("normal alpha single %ld double %ld\n",
	getgdesc(GD_BITS_NORM_SNG_ALPHA), getgdesc(GD_BITS_NORM_DBL_ALPHA));
    printf("cursor %ld over/under shared %ld blend %ld cifract %ld\n",
	getgdesc(GD_BITS_CURSOR),
	getgdesc(GD_OVERUNDER_SHARED),
	getgdesc(GD_BLEND),
	getgdesc(GD_CIFRACT));
    printf("xhair index %ld\n", getgdesc(GD_CROSSHAIR_CINDEX));
    printf("dithered RGB %ld\n", getgdesc(GD_DITHER));
    printf("linesmooth cmode %ld rgb %ld\n",
	getgdesc(GD_LINESMOOTH_CMODE), getgdesc(GD_LINESMOOTH_RGB));
    printf("logicop %ld\n", getgdesc(GD_LOGICOP));
    printf("# screens %ld\n", getgdesc(GD_NSCRNS));
    printf("nurbs order %ld trimcurve order %ld\n",
	getgdesc(GD_NURBS_ORDER), getgdesc(GD_TRIMCURVE_ORDER));
    printf("nblinks %ld\n", getgdesc(GD_NBLINKS));
    printf("nvertex/poly %ld\n", getgdesc(GD_NVERTEX_POLY));
    printf("pattern size %ld\n", getgdesc(GD_PATSIZE_64));
    printf("point smooth cmode %ld rgb %ld\n",
	getgdesc(GD_PNTSMOOTH_CMODE), getgdesc(GD_PNTSMOOTH_RGB));
    printf("pup to over/under %ld\n", getgdesc(GD_PUP_TO_OVERUNDER));
    printf("read source %ld readsource zbuffer %ld\n",
	getgdesc(GD_READSOURCE),
	getgdesc(GD_READSOURCE_ZBUFFER));
    printf("stereo %ld\n", getgdesc(GD_STEREO));
    printf("subpixel line %ld point %ld poly %ld\n",
	getgdesc(GD_SUBPIXEL_LINE),
	getgdesc(GD_SUBPIXEL_PNT),
	getgdesc(GD_SUBPIXEL_POLY));
    printf("window system %ld\n", getgdesc(GD_WSYS));
    printf("zdraw geometry %ld pixels %ld\n",
	getgdesc(GD_ZDRAW_GEOM), getgdesc(GD_ZDRAW_PIXELS));
    printf("screen type %ld\n", getgdesc(GD_SCRNTYPE));
    printf("text port %ld\n", getgdesc(GD_TEXTPORT));
    printf("# mmaps %ld\n", getgdesc(GD_NMMAPS));
#ifdef notdef
    getgdesc(GD_FRAMEGRABBER);
    getgdesc(GD_TIMERHZ);
    getgdesc(GD_DBBOX);
    getgdesc(GD_AFUNCTION);
    getgdesc(GD_ALPHA_OVERUNDER);
    getgdesc(GD_BITS_ACBUF);
    getgdesc(GD_BITS_ACBUF_HW);
    getgdesc(GD_BITS_STENCIL);
    getgdesc(GD_CLIPPLANES);
    getgdesc(GD_FOGVERTEX);
    getgdesc(GD_LIGHTING_TWOSIDE);
    getgdesc(GD_POLYMODE);
    getgdesc(GD_POLYSMOOTH);
    getgdesc(GD_SCRBOX);
    getgdesc(GD_TEXTURE);
    getgdesc(GD_FOGPIXEL);
    getgdesc(GD_TEXTURE_PERSP);
    getgdesc(GD_MUXPIPES);
#endif
}
