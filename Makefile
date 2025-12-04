# cc - C compiler
# See LICENSE file for copyright and license details.

include config.mk

SRC = lc.c utils.c lexer.c parser.c sema.c
HDR = config.def.h utils.h lexer.h parser.h sema.h
OBJ = ${SRC:.c=.o}

all: options lc

options:
	@echo lc build options:
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

lc: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f lc ${OBJ} lc-${VERSION}.tar.gz

dist: clean
	mkdir -p lc-${VERSION}
	cp -R LICENSE Makefile README config.mk\
		lc.1 ${HDR} ${SRC} lc-${VERSION}
	tar -cf lc-${VERSION}.tar lc-${VERSION}
	gzip lc-${VERSION}.tar
	rm -rf lc-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f lc ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/lc
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < lc.1 > ${DESTDIR}${MANPREFIX}/man1/lc.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/lc.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/lc\
		${DESTDIR}${MANPREFIX}/man1/lc.1

.PHONY: all options clean dist install uninstall
