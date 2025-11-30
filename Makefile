# cc - C compiler
# See LICENSE file for copyright and license details.

include config.mk

SRC = cc.c utils.c lexer.c
HDR = config.def.h utils.h
OBJ = ${SRC:.c=.o}

all: options cc

options:
	@echo cc build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

users.h:
	cp users.def.h $@

cc: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f cc ${OBJ} cc-${VERSION}.tar.gz

dist: clean
	mkdir -p cc-${VERSION}
	cp -R LICENSE Makefile README config.mk\
		cc.1 ${HDR} ${SRC} cc-${VERSION}
	tar -cf cc-${VERSION}.tar cc-${VERSION}
	gzip cc-${VERSION}.tar
	rm -rf cc-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f cc ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/cc
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < cc.1 > ${DESTDIR}${MANPREFIX}/man1/cc.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/cc.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/cc\
		${DESTDIR}${MANPREFIX}/man1/cc.1

.PHONY: all options clean dist install uninstall
