#!smake
#
# $Revision: 1.12 $
# $Date: 91/06/04 18:38:47 $
#
COMMONPREF=fax
include defs

DIRS=	\
	util \
	fax2ps \
	faxadmin \
	faxalter \
	faxcomp \
	faxcover \
	faxd \
	faxmail \
	faxrm \
	faxstat \
	faxview \
	recvfax \
	sendfax \
	sgi2fax \
	$(NULL)
OTHERDIRS=\
	libtiff \
	${NULL}

.PATH: $(DIRS) ${OTHERDIRS}

OTHERDIRS=

TARGETS=flexfax

default all ${TARGETS}:
	@$(MAKE) -f $(MAKEFILE) dirs

include $(COMMONRULES)

dirs::
	@for i in ${OTHERDIRS} $(DIRS); do (echo "= "$$i; cd $$i; $(MAKE)); done
depend::
	@for i in $(DIRS); do (echo "= "$$i; cd $$i; $(MAKE) depend); done
clean::
	@for i in $(DIRS); do (echo "= "$$i; cd $$i; $(MAKE) clean); done
clobber::
	@for i in $(DIRS); do (echo "= "$$i; cd $$i; $(MAKE) clobber); done
install::
	@for i in $(DIRS); do (echo "= "$$i; cd $$i; $(MAKE) install); done

# this is how the server distribution image is created
dist.server:
	rm -f flexfax.server.tar
	tar cf flexfax.server.tar \
	    Makefile.server	\
	    README		\
	    doc			\
	    man/man1/*.1*	\
	    man/man4/*.4*	\
	    faxd/TODO		\
	    faxd/faxd		\
	    fax2ps/fax2ps	\
	    faxadmin/faxadmin	\
	    faxadmin/README	\
	    faxadmin/fax.hlp	\
	    faxadmin/vadmin-fax.ftr \
	    faxalter/faxalter	\
	    faxcomp/faxcomp	\
	    faxcover/faxcover	\
	    faxcover/faxcover.ps \
	    faxmail/faxmail	\
	    faxrm/faxrm		\
	    faxstat/faxstat	\
	    faxview/faxview	\
	    recvfax/faxd.recv	\
	    sendfax/sendfax	\
	    sgi2fax/sgi2fax	\
	    util/deliver.sh	\
	    util/dit2fax.sh	\
	    util/fax.ftr	\
	    util/faxanswer	\
	    util/faxchest.ps	\
	    util/faxdb		\
	    util/faxd		\
	    util/faxinfo	\
	    util/faxquit	\
	    util/faxsubmit	\
	    util/notify.sh	\
	    util/ps2fax.sh	\
	    util/submit2at.sh	\
	    util/text2fax.sh	\
	    etc/config.ttym2	\
	    ${NULL}
	compress flexfax.server.tar

# this is how the client distribution image is created
dist.client:
	rm -f flexfax.client.tar
	tar cf flexfax.client.tar \
	    Makefile.client	\
	    README		\
	    doc			\
	    man/man1/*.1*	\
	    faxd/TODO		\
	    fax2ps/fax2ps	\
	    faxalter/faxalter	\
	    faxcomp/faxcomp	\
	    faxcover/faxcover	\
	    faxcover/faxcover.ps \
	    faxmail/faxmail	\
	    faxrm/faxrm		\
	    faxstat/faxstat	\
	    faxview/faxview	\
	    sendfax/sendfax	\
	    sgi2fax/sgi2fax	\
	    util/dit2fax.sh	\
	    util/fax.ftr	\
	    util/text2fax.sh	\
	    util/faxdb		\
	    ${NULL}
	compress flexfax.client.tar

# this is how the distribution of the DPS-based imager is created
dist.ps2fax:
	rm -f ps2fax.tar
	tar cf ps2fax.tar ps2fax
	compress ps2fax.tar

# this is how the source distribution image is created
dist.src:
	rm -f flexfax.src.tar*
	tar cf flexfax.src.tar				\
	    README README.src				\
	    Makefile defs				\
	    Makefile.client Makefile.server		\
	    doc						\
	    etc/config.ttym2				\
	    fax2ps/Makefile				\
		fax2ps/*.h				\
		fax2ps/*.c				\
	    faxadmin/Makefile faxadmin/README		\
		faxadmin/*.h				\
		faxadmin/*.c++				\
		faxadmin/*.ftr				\
		faxadmin/fax.hlp			\
	    faxalter/Makefile				\
		faxalter/*.c++				\
	    faxcomp/Makefile				\
		faxcomp/*.h				\
		faxcomp/*.c++				\
	    faxcover/Makefile				\
		faxcover/*.c++				\
		faxcover/*.ps				\
	    faxd/Makefile faxd/TODO			\
		faxd/*.h				\
		faxd/*.c++				\
	    faxmail/Makefile				\
		faxmail/*.c++				\
		faxmail/*.ps				\
	    faxrm/Makefile				\
		faxrm/*.c++				\
	    faxstat/Makefile				\
		faxstat/*.c++				\
	    faxview/Makefile 				\
		faxview/*.h 				\
		faxview/*.c++				\
		faxview/*.c				\
	    man/man1/*.1*				\
	    man/man4/*.4*				\
	    recvfax/Makefile				\
		recvfax/*.c				\
	    sendfax/Makefile				\
		sendfax/*.c++				\
	    sgi2fax/Makefile				\
		sgi2fax/*.h				\
		sgi2fax/*.c				\
	    util/Makefile				\
		util/*.h				\
		util/*.c++ 				\
		util/*.sh				\
		util/*.ftr				\
		util/*.ps				\
		util/faxdb				\
		util/*.awk
	compress flexfax.src.tar
