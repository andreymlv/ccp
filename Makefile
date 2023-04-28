VERSION = 0.1

PREFIX = /usr/local

LIBS = -lz

# flags
CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L -DVERSION=\"${VERSION}\"
CFLAGS   = -g -std=c99 -Wall -O2 ${CPPFLAGS}
# CFLAGS   = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os ${CPPFLAGS}
LDFLAGS  = ${LIBS}

# Solaris
#CFLAGS = -fast -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = cc

SRC = ccp.c server.c client.c lib.c
OBJ = ${SRC:.c=.o}

all: options ccp

options:
	@echo ccp build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

ccp: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f ccp ${OBJ} ccp-${VERSION}.tar.gz *.orig *.rej

dist: clean
	mkdir -p ccp-${VERSION}
	cp -R LICENSE Makefile README ${SRC} server.h client.h lib.h ccp-${VERSION}
	tar -cf ccp-${VERSION}.tar ccp-${VERSION}
	gzip ccp-${VERSION}.tar
	rm -rf ccp-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ccp ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/ccp
	mkdir -p ${DESTDIR}${PREFIX}/share/ccp

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/ccp

.PHONY: all options clean dist install uninstall
