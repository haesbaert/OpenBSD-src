# $OpenBSD$

NOPROG=
NOMAN=
CC=${HOSTCC}

# PCI capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sparc64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "hppa64") || (${MACHINE} == "sgi") || \
    (${MACHINE} == "loongson")

# old license
FIRM=	radeon-r100_cp radeon-r200_cp radeon-r300_cp \
	radeon-r420_cp radeon-r520_cp radeon-r600_me radeon-r600_pfp \
	radeon-rs600_cp radeon-rs690_cp radeon-rs780_me radeon-rs780_pfp \
	radeon-rv610_me radeon-rv610_pfp radeon-rv620_me radeon-rv620_pfp \
	radeon-rv630_me radeon-rv630_pfp radeon-rv635_me radeon-rv635_pfp \
	radeon-rv670_me radeon-rv670_pfp radeon-rv710_me radeon-rv710_pfp \
	radeon-rv730_me radeon-rv730_pfp radeon-rv770_me radeon-rv770_pfp 
# new license
FIRM+=	radeon-aruba_me radeon-aruba_pfp radeon-aruba_rlc \
	radeon-barts_mc radeon-barts_me radeon-barts_pfp \
	radeon-btc_rlc \
	radeon-caicos_mc radeon-caicos_me radeon-caicos_pfp \
	radeon-cayman_mc radeon-cayman_me radeon-cayman_pfp radeon-cayman_rlc \
	radeon-cedar_me radeon-cedar_pfp radeon-cedar_rlc \
	radeon-cypress_me radeon-cypress_pfp radeon-cypress_rlc \
	radeon-cypress_uvd \
	radeon-hainan_ce radeon-hainan_mc radeon-hainan_me radeon-hainan_pfp \
	radeon-hainan_rlc \
	radeon-juniper_me radeon-juniper_pfp radeon-juniper_rlc \
	radeon-oland_ce radeon-oland_mc radeon-oland_me radeon-oland_pfp \
	radeon-oland_rlc \
	radeon-palm_me radeon-palm_pfp \
	radeon-pitcairn_ce radeon-pitcairn_mc radeon-pitcairn_me \
	radeon-pitcairn_pfp radeon-pitcairn_rlc \
	radeon-r600_rlc radeon-r700_rlc \
	radeon-redwood_me radeon-redwood_pfp radeon-redwood_rlc \
	radeon-rv710_uvd \
	radeon-sumo2_me radeon-sumo2_pfp \
	radeon-sumo_me radeon-sumo_pfp radeon-sumo_rlc radeon-sumo_uvd \
	radeon-tahiti_ce radeon-tahiti_mc radeon-tahiti_me radeon-tahiti_pfp \
	radeon-tahiti_rlc radeon-tahiti_uvd \
	radeon-turks_mc radeon-turks_me radeon-turks_pfp \
	radeon-verde_ce radeon-verde_mc radeon-verde_me radeon-verde_pfp \
	radeon-verde_rlc 

PROG=	build

CLEANFILES+= ${FIRM} ${PROG}

all: ${FIRM}

${FIRM}: build
	${.OBJDIR}/build

realinstall:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/radeon-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>
