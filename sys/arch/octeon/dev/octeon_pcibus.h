/*	$OpenBSD: octeon_pcibus.h,v 1.2 2013/06/02 20:29:36 jasper Exp $	*/

/*
 * Copyright (c) 2003-2004 Opsycon AB (www.opsycon.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#ifndef ___OCTEON_PCIBUS_H__
#define ___OCTEON_PCIBUS_H__

#include <machine/octeonreg.h>

#define OCTEON_PCIBUS_PCIIO_BASE	0x11A0000000000ULL

#if _BYTE_ORDER == _BIG_ENDIAN
#define OCTEON_PCI_CFG0			0x80011F0000001804ULL
#define OCTEON_PCI_CFG1			0x80011F0000001800ULL
#define OCTEON_PCI_CONFIG_BASE0		0x8001190000000004ULL
#define OCTEON_PCI_CONFIG_BASE1		0x8001190000000000ULL
#else
#define OCTEON_PCI_CFG0			0x80011F0000001800ULL
#define OCTEON_PCI_CFG1			0x80011F0000001804ULL
#define OCTEON_PCI_CONFIG_BASE0		0x8001190000000000ULL
#define OCTEON_PCI_CONFIG_BASE1		0x8001190000000004ULL
#endif

#define OCTEON_PCI_CFG2			(OCTEON_PCI_CFG0 + 0x08UL)
#define OCTEON_PCI_CFG3			(OCTEON_PCI_CFG1 + 0x08UL)
#define OCTEON_PCI_CFG4			(OCTEON_PCI_CFG0 + 0x10UL)
#define OCTEON_PCI_CFG5			(OCTEON_PCI_CFG1 + 0x10UL)
#define OCTEON_PCI_CFG6			(OCTEON_PCI_CFG0 + 0x18UL)
#define OCTEON_PCI_CFG7			(OCTEON_PCI_CFG1 + 0x18UL)
#define OCTEON_PCI_CFG8			(OCTEON_PCI_CFG0 + 0x20UL)
#define OCTEON_PCI_CFG9			(OCTEON_PCI_CFG1 + 0x20UL)
#define OCTEON_PCI_CFG10		(OCTEON_PCI_CFG0 + 0x28UL)
#define OCTEON_PCI_CFG11		(OCTEON_PCI_CFG1 + 0x28UL)
#define OCTEON_PCI_CFG12		(OCTEON_PCI_CFG0 + 0x30UL)
#define OCTEON_PCI_CFG13		(OCTEON_PCI_CFG1 + 0x30UL)
#define OCTEON_PCI_CFG14		(OCTEON_PCI_CFG0 + 0x38UL)
#define OCTEON_PCI_CFG15		(OCTEON_PCI_CFG1 + 0x38UL)
#define OCTEON_PCI_CFG16		(OCTEON_PCI_CFG0 + 0x40UL)
#define OCTEON_PCI_CFG17		(OCTEON_PCI_CFG1 + 0x40UL)
#define OCTEON_PCI_CFG18		(OCTEON_PCI_CFG0 + 0x48UL)
#define OCTEON_PCI_CFG19		(OCTEON_PCI_CFG1 + 0x48UL)
#define OCTEON_PCI_CFG20		(OCTEON_PCI_CFG0 + 0x50UL)
#define OCTEON_PCI_CFG21		(OCTEON_PCI_CFG1 + 0x50UL)
#define OCTEON_PCI_CFG22		(OCTEON_PCI_CFG0 + 0x58UL)
#define OCTEON_PCI_CFG23		(OCTEON_PCI_CFG1 + 0x58UL)
#define OCTEON_PCI_CFG24		(OCTEON_PCI_CFG0 + 0x60UL)
#define OCTEON_PCI_CFG25		(OCTEON_PCI_CFG1 + 0x60UL)
#define OCTEON_PCI_CFG26		(OCTEON_PCI_CFG0 + 0x68UL)
#define OCTEON_PCI_CFG27		(OCTEON_PCI_CFG1 + 0x68UL)
#define OCTEON_PCI_CFG28		(OCTEON_PCI_CFG0 + 0x70UL)
#define OCTEON_PCI_CFG29		(OCTEON_PCI_CFG1 + 0x70UL)
#define OCTEON_PCI_CFG30		(OCTEON_PCI_CFG0 + 0x78UL)
#define OCTEON_PCI_CFG31		(OCTEON_PCI_CFG1 + 0x78UL)
#define OCTEON_PCI_CFG32		(OCTEON_PCI_CFG0 + 0x80UL)
#define OCTEON_PCI_CFG33		(OCTEON_PCI_CFG1 + 0x80UL)
#define OCTEON_PCI_CFG34		(OCTEON_PCI_CFG0 + 0x88UL)
#define OCTEON_PCI_CFG35		(OCTEON_PCI_CFG1 + 0x88UL)
#define OCTEON_PCI_CFG36		(OCTEON_PCI_CFG0 + 0x90UL)
#define OCTEON_PCI_CFG37		(OCTEON_PCI_CFG1 + 0x90UL)
#define OCTEON_PCI_CFG38		(OCTEON_PCI_CFG0 + 0x98UL)
#define OCTEON_PCI_CFG39		(OCTEON_PCI_CFG1 + 0x98UL)
#define OCTEON_PCI_CFG40		(OCTEON_PCI_CFG0 + 0xA0UL)
#define OCTEON_PCI_CFG41		(OCTEON_PCI_CFG1 + 0xA0UL)
#define OCTEON_PCI_CFG42		(OCTEON_PCI_CFG0 + 0xA8UL)
#define OCTEON_PCI_CFG43		(OCTEON_PCI_CFG1 + 0xA8UL)
#define OCTEON_PCI_CFG44		(OCTEON_PCI_CFG0 + 0xB0UL)
#define OCTEON_PCI_CFG45		(OCTEON_PCI_CFG1 + 0xB0UL)
#define OCTEON_PCI_CFG46		(OCTEON_PCI_CFG0 + 0xB8UL)
#define OCTEON_PCI_CFG47		(OCTEON_PCI_CFG1 + 0xB8UL)
#define OCTEON_PCI_CFG48		(OCTEON_PCI_CFG0 + 0xC0UL)
#define OCTEON_PCI_CFG49		(OCTEON_PCI_CFG1 + 0xC0UL)
#define OCTEON_PCI_CFG50		(OCTEON_PCI_CFG0 + 0xC8UL)
#define OCTEON_PCI_CFG51		(OCTEON_PCI_CFG1 + 0xC8UL)
#define OCTEON_PCI_CFG52		(OCTEON_PCI_CFG0 + 0xD0UL)
#define OCTEON_PCI_CFG53		(OCTEON_PCI_CFG1 + 0xD0UL)
#define OCTEON_PCI_CFG54		(OCTEON_PCI_CFG0 + 0xD8UL)
#define OCTEON_PCI_CFG55		(OCTEON_PCI_CFG1 + 0xD8UL)
#define OCTEON_PCI_CFG56		(OCTEON_PCI_CFG0 + 0xE0UL)
#define OCTEON_PCI_CFG57		(OCTEON_PCI_CFG1 + 0xE0UL)
#define OCTEON_PCI_CFG58		(OCTEON_PCI_CFG0 + 0xE8UL)
#define OCTEON_PCI_CFG59		(OCTEON_PCI_CFG1 + 0xE8UL)
#define OCTEON_PCI_CFG60		(OCTEON_PCI_CFG0 + 0xF0UL)
#define OCTEON_PCI_CFG61		(OCTEON_PCI_CFG1 + 0xF0UL)
#define OCTEON_PCI_CFG62		(OCTEON_PCI_CFG0 + 0xF8UL)
#define OCTEON_PCI_CFG63		(OCTEON_PCI_CFG1 + 0xF8UL)

#define OCTEON_PCIBUS_PCIMAPCFG_TYPE1	0x00010000

#endif /* ___OCTEON_PCIBUS_H__ */
