#! smake
# $Revision: 1.14 $
# $Date: 91/05/20 14:30:00 $
#
include ../defs

C++FILES=\
	FaxDB.c++ \
	FaxClient.c++ \
	Path.c++ \
	BusyCursor.c++ \
	ConfirmDialog.c++ \
	StatusLabel.c++ \
	Fatal.c++ \
	${NULL}
TARGETS=libfaxutil.a faxsubmit faxquit faxanswer faxinfo

LC++INCS=\
	-I${ROOT}/usr/include/fxds\
	${NULL}
LIBS=\
	${NULL}
LLDLIBS= $(LIBS) -lm

default all::
	@${MAKE} incdepend
	@${MAKE} targets
	
targets: $(TARGETS)

include ${COMMONRULES}

libfaxutil.a: ${OBJECTS}
	@rm -f $@;
	@echo "$(AR) crls $@ ${OBJECTS}"; $(AR) crls $@ ${OBJECTS} $(C++FILT)

BusyCursor.o: BusyCursor.c++
	${C++F} -I${ROOT}/usr/include/fxui -c BusyCursor.c++
ConfirmDialog.o: ConfirmDialog.c++
	${C++F} -I${ROOT}/usr/include/fxui -c ConfirmDialog.c++
StatusLabel.o: StatusLabel.c++
	${C++F} -I${ROOT}/usr/include/fxui -c StatusLabel.c++

faxsubmit: faxsubmit.c
	${CC} -o $@ ${CFLAGS} faxsubmit.c
faxquit: faxquit.c
	${CC} -o $@  ${CFLAGS} faxquit.c
faxanswer: faxanswer.c
	${CC} -o $@  ${CFLAGS} faxanswer.c
faxinfo: faxinfo.c ../libtiff/libtiff.a
	${CC} -o $@  ${CFLAGS} -I../libtiff faxinfo.c ../libtiff/libtiff.a

tick:	tick.o ${LIBS}
	${C++F} -o $@ tick.o ${LDFLAGS}
faxcheck: faxcheck.c++ ${LIBS}
	${C++F} -o $@ faxcheck.c++ ${LDFLAGS}

install: default
	/etc/install -f ${BIN} -m 755 -src text2fax.sh -O text2fax
	/etc/install -f ${BIN} -m 755 -src dit2fax.sh -O dit2fax
	/etc/install -f ${BIN} -u fax -m 4755 -O faxanswer
	/etc/install -f ${BIN} -u fax -m 4755 -O faxquit
	/etc/install -f ${SPOOL}/bin -m 755 -O faxsubmit faxinfo
	/etc/install -f ${SPOOL}/bin -m 755 -src ps2fax.sh -O ps2fax
	/etc/install -f ${SPOOL}/etc -m 444 -O dpsprinter.ps
	/etc/install -f ${SPOOL}/bin -m 755 -src notify.sh -O notify
	/etc/install -f ${SPOOL}/bin -m 755 -src submit2at.sh -O submit2at
	/etc/install -f ${SPOOL}/bin -m 755 -src deliver.sh -O deliver
	/etc/install -f /etc/init.d -m 755 -O faxd
