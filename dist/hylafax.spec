#	$Header: /usr/people/sam/fax/./dist/RCS/hylafax.spec,v 1.33 1995/04/08 21:08:39 sam Rel $
#
# HylaFAX Facsimile Software
#
# Copyright (c) 1990-1995 Sam Leffler
# Copyright (c) 1991-1995 Silicon Graphics, Inc.
# HylaFAX is a trademark of Silicon Graphics
# 
# Permission to use, copy, modify, distribute, and sell this software and 
# its documentation for any purpose is hereby granted without fee, provided
# that (i) the above copyright notices and this permission notice appear in
# all copies of the software and related documentation, and (ii) the names of
# Sam Leffler and Silicon Graphics may not be used in any advertising or
# publicity relating to the software without the specific, prior written
# permission of Sam Leffler and Silicon Graphics.
# 
# THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
# EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
# WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
# 
# IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
# ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
# OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
# WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
# LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
# OF THIS SOFTWARE.
#
define CUR_MAJ_VERS	1006		# Major Version number
define CUR_MIN_VERS	004		# Minor Version number
define CUR_VERS		${CUR_MAJ_VERS}${CUR_MIN_VERS}${ALPHA}
define FAX_NAME		"HylaFAX"

include hylafax.version
include hylafax.alpha

product hylafax
    id	"${FAX_NAME} Facsimile Software, Version ${FAX_VNUM}"
    inplace

    image sw
	id "${FAX_NAME} Software"
	version	"${CUR_VERS}"
	subsys client default
	    id	"${FAX_NAME} Client Software"
	    exp	"hylafax.sw.client"
	    replaces self
	    replaces flexfax.sw.client 0 oldvers
	endsubsys
	subsys server
	    id	"${FAX_NAME} Server Software"
	    exp	"hylafax.sw.server"
	    replaces self
	    replaces flexfax.sw.server 0 oldvers
	endsubsys
	subsys dpsrip
	    id	"${FAX_NAME} DPS-based PostScript RIP (for server)"
	    exp	"hylafax.sw.dpsrip"
	    # need DPS fonts and VM startup file
	    prereq (
		dps_eoe.sw.dps		1006000000 maxint
		dps_eoe.sw.dpsfonts	1006000000 maxint
	    )
	endsubsys
	subsys gsrip
	    id	"Ghostscript-based PostScript RIP (for server)"
	    exp	"hylafax.sw.gsrip"
	endsubsys
	subsys afm
	    id	"${FAX_NAME} Font Metric Files (for client)"
	    exp	"hylafax.sw.afm"
	    incompat dps_eoe.sw.dpsfonts 0 maxint
	endsubsys
    endimage

    image man
	id "${FAX_NAME} UNIX Manual Pages"
	version	"${CUR_VERS}"
	subsys client default
	    id	"${FAX_NAME} Client Manual Pages"
	    exp	"hylafax.man.client"
	    replaces self
	    replaces flexfax.man.* 0 oldvers
	endsubsys
	subsys server
	    id	"${FAX_NAME} Server Manual Pages"
	    exp	"hylafax.man.server"
	    replaces self
	    replaces flexfax.man.* 0 oldvers
	endsubsys
	subsys gsrip
	    id	"Ghostscript Manual Pages"
	    exp	"hylafax.man.gsrip"
	    replaces self
	endsubsys
    endimage

    image html
	id "${FAX_NAME} World Wide Web Materials"
	version	"${CUR_VERS}"
	subsys docs
	    id	"${FAX_NAME} Online Documentation"
	    exp	"hylafax.html.docs"
	endsubsys
	subsys faq
	    id	"${FAX_NAME} Frequently Asked Questions"
	    exp	"hylafax.html.faq"
	endsubsys
	subsys tools
	    id	"${FAX_NAME} CGI Scripts & Tools"
	    exp	"hylafax.html.cgi"
	endsubsys
    endimage

    image doc
	id "Ancillary Documentation"
	version	"${CUR_VERS}"
	subsys gsrip
	    id	"Ghostscript Documentation"
	    exp	"hylafax.doc.gsrip"
	    replaces self
	endsubsys
    endimage
endproduct
