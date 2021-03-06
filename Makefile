# staticdwm - static/dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c staticdwm.c util.c
OBJ = ${SRC:.c=.o}

all: options staticdwm

options:
	@echo staticdwm build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

staticdwm: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f staticdwm ${OBJ} staticdwm-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p staticdwm-${VERSION}
	@cp -R LICENSE TODO BUGS Makefile README config.def.h config.mk \
		staticdwm.1 drw.h util.h ${SRC} staticdwm-${VERSION}
	@tar -cf staticdwm-${VERSION}.tar staticdwm-${VERSION}
	@gzip staticdwm-${VERSION}.tar
	@rm -rf staticdwm-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f staticdwm ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/staticdwm
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < staticdwm.1 > ${DESTDIR}${MANPREFIX}/man1/staticdwm.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/staticdwm.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/staticdwm
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/staticdwm.1

.PHONY: all options clean dist install uninstall
