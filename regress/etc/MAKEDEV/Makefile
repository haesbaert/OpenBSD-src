
# $OpenBSD: Makefile,v 1.27 2012/06/20 18:45:44 matthew Exp $

MAKEDEVARCHS+=alpha amd64 armish aviion hp300 hppa hppa64 i386 landisk
MAKEDEVARCHS+=loongson luna88k macppc mvme68k
MAKEDEVARCHS+=mvme88k sgi socppc sparc sparc64 vax zaurus

MAKEDEVTARGS?=all ramdisk

ETCSRCDIR?=	/usr/src/etc

REGRESS_SLOW_TARGETS=${REGRESS_TARGETS}

# test separate targets, all, ramdisk/raminst, std, etc
# t1 = all
# t2 = ramdisk

REGRESS_TARGETS+= ${MAKEDEVARCHS}
REGRESS_ROOT_TARGETS=${REGRESS_TARGETS}

${REGRESS_TARGETS}:
. for _T in ${MAKEDEVTARGS}
	@echo "====> ${_T}.${.TARGET}"
	@exec ${SUDO} rm -rf test.${_T}.${.TARGET}
	@exec mkdir -p -m 700 test.${_T}.${.TARGET}
	@exec cp ${ETCSRCDIR}/etc.${.TARGET}/MAKEDEV test.${_T}.${.TARGET}
	@exec ${SUDO} chown root:wheel test.${_T}.${.TARGET}
	@cd test.${_T}.${.TARGET}; \
	time ${SUDO} sh ./MAKEDEV ${_T} || true; \
	time ${SUDO} sh ./MAKEDEV ${_T} || true; \
	( ls -ln; ls -ln fd/; ls -ln altq/ ) 2> /dev/null | \
		awk '/^[bcps]/ {printf "%s %x.%x %x,%x%s\n",$$1,$$3,$$4,$$5,$$6,$$10} \
		     /^l/      {printf "%s %s.%s%s>%s\n",$$1,$$3,$$4,$$9,$$11}' | \
		sort +5 -n | \
		sed -e 's/rwx/7/g;s/rw-/6/g;s/r-x/5/g;s/r--/4/g' \
		    -e 's/-wx/3/g;s/-w-/2/g;s/--x/1/g;s/---/0/g' \
		    -e 's/^\([bcpsl]\)\([0-9][0-9]*\) /\2\1/' \
		> ../t1.${_T}.${.TARGET}.out
.endfor

clean:
	for f in ${MAKEDEVARCHS}; do rm -rf test.$$f; done
	rm -f ${CLEANFILES}

.PHONY: ${MAKEDEVARCHS}
CLEANFILES+=t1.*.out

.include <bsd.regress.mk>
