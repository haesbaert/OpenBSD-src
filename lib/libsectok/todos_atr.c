/* $Id: todos_atr.c,v 1.5 2001/06/18 16:00:50 rees Exp $ */

/*
copyright 1997, 1999, 2000
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works 
and redistribute this software and such derivative works 
for any purpose, so long as the name of the university of 
michigan is not used in any advertising or publicity 
pertaining to the use or distribution of this software 
without specific, written prior authorization.  if the 
above copyright notice or any other identification of the 
university of michigan is included in any copy of any 
portion of this software, then the disclaimer below must 
also be included.

this software is provided as is, without representation 
from the university of michigan as to its fitness for any 
purpose, and without warranty by the university of 
michigan of any kind, either express or implied, including 
without limitation the implied warranties of 
merchantability and fitness for a particular purpose. the 
regents of the university of michigan shall not be liable 
for any damages, including special, indirect, incidental, or 
consequential damages, with respect to any claim arising 
out of or in connection with the use of the software, even 
if it has been or is hereafter advised of the possibility of 
such damages.
*/

/*
 * Parse smart card atr, return proto params
 *
 * Jim Rees, University of Michigan CITI
 */

#ifdef __palmos__
#include <Common.h>
#include <System/SysAll.h>
#include <System/Unix/unix_stdio.h>
#include <System/Unix/unix_stdlib.h>
#include <System/Unix/unix_string.h>
#include <UI/UIAll.h>
#include "field.h"
typedef long int32_t;
#else
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#endif

#include "sectok.h"
#include "todos_scrw.h"

#ifdef __unix__
#define SCPPS
#endif

/*
 * 7816 says ATR will appear within 40000 clocks (12 msec)
 * BUT some cards violate the spec and require more time
 */
#define ATRTIME 120
#define BYTETIME 1000

/* Global interface bytes */
#define TA1 (tpb[0][0])
#define TB1 (tpb[0][1])
#define TC1 (tpb[0][2])
#define TD1 (tpb[0][3])
#define TC2 (tpb[1][2])
#define TA2 (tpb[1][0])

static char dummyatr[] = {0x3b, 0x25, 0x00, 0x44, 0x55, 0x4d, 0x4d, 0x59};

/* Inversion table, for inverse convention */
unsigned char todos_scinvert[] = {
    0xff, 0x7f, 0xbf, 0x3f, 0xdf, 0x5f, 0x9f, 0x1f,
    0xef, 0x6f, 0xaf, 0x2f, 0xcf, 0x4f, 0x8f, 0x0f,
    0xf7, 0x77, 0xb7, 0x37, 0xd7, 0x57, 0x97, 0x17,
    0xe7, 0x67, 0xa7, 0x27, 0xc7, 0x47, 0x87, 0x07,
    0xfb, 0x7b, 0xbb, 0x3b, 0xdb, 0x5b, 0x9b, 0x1b,
    0xeb, 0x6b, 0xab, 0x2b, 0xcb, 0x4b, 0x8b, 0x0b,
    0xf3, 0x73, 0xb3, 0x33, 0xd3, 0x53, 0x93, 0x13,
    0xe3, 0x63, 0xa3, 0x23, 0xc3, 0x43, 0x83, 0x03,
    0xfd, 0x7d, 0xbd, 0x3d, 0xdd, 0x5d, 0x9d, 0x1d,
    0xed, 0x6d, 0xad, 0x2d, 0xcd, 0x4d, 0x8d, 0x0d,
    0xf5, 0x75, 0xb5, 0x35, 0xd5, 0x55, 0x95, 0x15,
    0xe5, 0x65, 0xa5, 0x25, 0xc5, 0x45, 0x85, 0x05,
    0xf9, 0x79, 0xb9, 0x39, 0xd9, 0x59, 0x99, 0x19,
    0xe9, 0x69, 0xa9, 0x29, 0xc9, 0x49, 0x89, 0x09,
    0xf1, 0x71, 0xb1, 0x31, 0xd1, 0x51, 0x91, 0x11,
    0xe1, 0x61, 0xa1, 0x21, 0xc1, 0x41, 0x81, 0x01,
    0xfe, 0x7e, 0xbe, 0x3e, 0xde, 0x5e, 0x9e, 0x1e,
    0xee, 0x6e, 0xae, 0x2e, 0xce, 0x4e, 0x8e, 0x0e,
    0xf6, 0x76, 0xb6, 0x36, 0xd6, 0x56, 0x96, 0x16,
    0xe6, 0x66, 0xa6, 0x26, 0xc6, 0x46, 0x86, 0x06,
    0xfa, 0x7a, 0xba, 0x3a, 0xda, 0x5a, 0x9a, 0x1a,
    0xea, 0x6a, 0xaa, 0x2a, 0xca, 0x4a, 0x8a, 0x0a,
    0xf2, 0x72, 0xb2, 0x32, 0xd2, 0x52, 0x92, 0x12,
    0xe2, 0x62, 0xa2, 0x22, 0xc2, 0x42, 0x82, 0x02,
    0xfc, 0x7c, 0xbc, 0x3c, 0xdc, 0x5c, 0x9c, 0x1c,
    0xec, 0x6c, 0xac, 0x2c, 0xcc, 0x4c, 0x8c, 0x0c,
    0xf4, 0x74, 0xb4, 0x34, 0xd4, 0x54, 0x94, 0x14,
    0xe4, 0x64, 0xa4, 0x24, 0xc4, 0x44, 0x84, 0x04,
    0xf8, 0x78, 0xb8, 0x38, 0xd8, 0x58, 0x98, 0x18,
    0xe8, 0x68, 0xa8, 0x28, 0xc8, 0x48, 0x88, 0x08,
    0xf0, 0x70, 0xb0, 0x30, 0xd0, 0x50, 0x90, 0x10,
    0xe0, 0x60, 0xa0, 0x20, 0xc0, 0x40, 0x80, 0x00,
};

/* 7816-3 1997 Table 7, 8 */
static short Ftab[] = { 372, 372, 558,  744, 1116, 1488, 1860, -1,
		  -1, 512, 768, 1024, 1536, 2048,   -1, -1 };
static short Dtab[] = { -1, 1, 2, 4, 8, 16, 32, -1, 12, 20, -1, -1, -1, -1, -1, -1 };

/*
 * Table generated by mkFDtab.
 */

static struct bps {
    unsigned char Fi, Di;
    int32_t bps;
} bps[] = {
    { 0x01, 0x08, 115464 },
    { 0x09, 0x05, 111856 },
    { 0x0b, 0x06, 111840 },
    { 0x03, 0x08,  57732 },
    { 0x09, 0x04,  55928 },
    { 0x0a, 0x08,  55920 },
    { 0x0b, 0x05,  55920 },
    { 0x0d, 0x06,  55904 },
    { 0x03, 0x04,  38488 },
    { 0x01, 0x03,  38488 },
    { 0x04, 0x08,  38484 },
    { 0x06, 0x09,  38480 },
    { 0x05, 0x05,  38480 },
    { 0x0a, 0x04,  37280 },
    { 0x0c, 0x05,  37280 },
    { 0x05, 0x08,  28860 },
    { 0x09, 0x03,  27964 },
    { 0x0c, 0x08,  27960 },
    { 0x0b, 0x04,  27960 },
    { 0x0d, 0x05,  27952 },
    { 0x06, 0x08,  23088 },
    { 0x01, 0x02,  19244 },
    { 0x03, 0x03,  19244 },
    { 0x05, 0x04,  19240 },
    { 0x0a, 0x03,  18640 },
    { 0x0c, 0x04,  18640 },
    { 0x09, 0x02,  13982 },
    { 0x0b, 0x03,  13980 },
    { 0x0d, 0x04,  13976 },
    { 0x02, 0x02,  12828 },
    { 0x04, 0x03,  12828 },
    { 0, 0, 0 }
};


#define SCGETC if (scgetc(ttyn, ap, (len ? BYTETIME : ATRTIME)) != SCEOK) goto timedout; else len++

int
todos_get_atr(int ttyn, int flags, unsigned char *atr, struct scparam *param)
{
    int len, i, t, ts, t0, tck, nhb, pbn;
    int F, D, Fi, Di, N, etu, WI;
    unsigned char *ap;
    unsigned char tpb[8][4];
    int hiproto = 0;

    if (flags & SCRFORCE) {
	len = sizeof dummyatr;
	memcpy(atr, dummyatr, len);
	param->t = 0;
	param->etu = 100;
	param->n = 0;
	param->cwt = 1000;
	return len;
    }

#ifndef DEBUG
    flags &= ~SCRV;
#endif

    ap = atr;
    len = 0;

    /* TS */
    SCGETC;
    ts = *ap++;
    if (ts == 0x3) {
	if (flags & SCRV)
	    printf("inverse conversion\n");
	todos_scsetflags(ttyn, SCOINVRT, SCOINVRT);
	ts = todos_scinvert[ts];
    }
    if (ts != 0x3b && ts != 0x3f) {
	if (flags & SCRV)
	    printf("TS=%02x (not default timing)\n", ts);
	param->t = -1;
	return 0;
    }

    /* T0 */
    SCGETC;
    t0 = *ap++;
    nhb = t0 & 0xf;

    /* Fill in defaults */
    TA1 = 0x11;
    TB1 = 0x4d;
    TC1 = 0x00;
    TC2 = 10;
    TA2 = 0xff;
    TD1 = 0;

    Fi = 1;
    Di = 1;

    /* Get up to 8 sets of protocol bytes */
    for (i = 0; i < 8; i++) {
	tpb[i][3] = 0;
	for (pbn = 0; pbn < 4; pbn++) {
	    /* If T0 (or TD(i)) indicates presence of proto byte, get it */
	    if (t0 & (1 << (4 + pbn))) {
		SCGETC;
		tpb[i][pbn] = *ap++;
	    }
	}
	t = tpb[i][3] & 0xf;
	if (t > hiproto)
	    hiproto = t;

	if (flags & SCRV) {
	    printf("proto %d T=%d", i + 1, t);
	    for (pbn = 0; pbn < 4; pbn++)
		if (t0 & (1 << (4 + pbn)))
		    printf(" T%c%d=%02x", 'A' + pbn, i + 1, tpb[i][pbn]);
	    printf("\n");
	}

	t0 = tpb[i][3];
	if (!(t0 & 0xf0))
	    break;
    }

    /* Historical bytes */
    if (nhb) {
	for (i = 0; i < nhb; i++) {
	    SCGETC;
	    ap++;
	}
	if ((flags & SCRV))
	    printf("%d historical bytes\n", nhb);
    }

    /* TCK */
    if (hiproto > 0) {
	SCGETC;
	ap++;
	tck = 0;
	for (i = 1; i < len; i++)
	    tck ^= atr[i];
	if (tck != 0 && (flags & SCRV))
	    printf("Checksum failed, TCK=%x sum=%x\n", atr[len-1], tck);
    }

    /*
     * I'm a little unclear on this.  If TA2 is present, it indicates a specific mode.
     * Else it's negotiable, and starts out with proto 1?
     * 7816-3 6.6.1
     */
    if (TA2 != 0xff)
	t = TA2 & 0xf;
    else
	t = TD1 & 0xf;

    /* Todos reader won't do higher speeds */
    if (!(flags & SCRTODOS)) {
	for (i = 0; bps[i].bps; i++) {
	    if (((TA1 >> 4) & 0xf) >= bps[i].Fi && (TA1 & 0xf) >= bps[i].Di) {
		int j;
		unsigned char c;
		static unsigned char pps[4] = {0xff, 0x10, 0, 0};

		pps[2] = (bps[i].Fi << 4) | bps[i].Di;
		pps[3] = 0;

		if (flags & SCRV)
		    printf("speed %ld\n", (long) bps[i].bps);

#ifdef SCPPS
		/* Compute checksum */
		for (j = 0; j < 3; j++)
		    pps[3] ^= pps[j];

		for (j = 0; j < 4; j++)
		    scputc(ttyn, pps[j]);
		for (j = 0; j < 4; j++)
		    if (scgetc(ttyn, &c, 100) != SCEOK || c != pps[j])
			break;
		if (j != 4)
		    continue;
		if (todos_scsetspeed(ttyn, bps[i].bps) < 0) {
		    /* We already sent the pps, can't back out now, so fail. */
		    if (flags & SCRV)
			printf("scsetspeed %ld failed\n", (long) bps[i].bps);
		    param->t = -1;
		    return len;
		}
#endif
		Fi = bps[i].Fi;
		Di = bps[i].Di;
		break;
	    }
	}
    }

    F  = Ftab[Fi];
    D  = Dtab[Di];
    N  = TC1;

    /* 1/f = 1/3.579545 ~= 50/179; etu in microsec */
    param->etu = etu = (F * 50L) / (D * 179L);
    param->n = (N < 255) ? N : 0;

    if (flags & SCRV) {
	printf("%d etu = %d F / %d D * 3.58 f\n", etu, F, D);
	if (N)
	    printf("%d N\n", N);
    }

    if (t == 0) {
	WI = TC2;

	/* cwt is in milliseconds */
	param->cwt = (960L * WI * F) / 3580L;
	if ((flags & SCRV) && WI != 10)
	    printf("%d cwt = (960 * %d WI * %d F) / 3.58 f / 1000\n",
		   param->cwt, WI, F);
    } else if (t == 1) {
	/* add 100 to each for engineering safety margin */
	param->cwt = (11L + (1 << (TB1 & 0xf))) * etu / 1000 + 100;
	param->bwt = (11L * etu / 1000L) + ((1 << ((TB1 >> 4) & 0xf)) * 100) + 100;
	if (flags & SCRV)
	    printf("%d cwt, %d bwt\n", param->cwt, param->bwt);
    }
    param->t = t;
    return len;

 timedout:
    if (flags & SCRV)
	printf("timed out after %d atr bytes\n", len);
    param->t = -1;
    return 0;
}
