/*	$OpenBSD: kue_fw.h,v 1.2 2000/03/28 19:37:48 aaron Exp $ */
/*	$NetBSD: kue_fw.h,v 1.1 2000/01/17 01:38:43 augustss Exp $	*/
/*
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/usb/kue_fw.h,v 1.1 2000/01/05 04:27:07 wpaul Exp $
 */

/*
 * This file contains the firmware needed to make the KLSI chip work,
 * along with a few constants related to the QT Engine microcontroller
 * embedded in the KLSI part.
 *
 * Firmware is loaded using the vendor-specific 'send scan data'
 * command (0xFF). The basic operation is that we must load the
 * firmware, then issue some trigger commands to fix it up and start
 * it running. There are three transfers: load the binary code,
 * load the 'fixup' (data segment?), then issue a command to
 * start the code firmware running. The data itself is prefixed by
 * a 16-bit signature word, a 16-bit length value, a type byte
 * and an interrupt (command) byte. The code segment is of type
 * 0x02 (replacement interrupt vector data) and the fixup segment
 * is of type 0x03 (replacement interrupt fixup data). The interrupt
 * code is 0x64 (load new code). The length word is the total length
 * of the segment minus 7. I precomputed the values and stuck them
 * into the appropriate locations within the segments to save some
 * work in the driver.
 */

/* QT controller data block types. */
/* Write data into specific memory location. */
#define KUE_QTBTYPE_WRITE_DATA		0x00
/* Write data into interrupt vector location */
#define KUE_QTBTYPE_WRITE_INTVEC	0x01
/* Replace interrupt vector with this data */
#define KUE_QTBTYPE_REPL_INTVEC		0x02
/* Fixup interrupt vector code with this data */
#define KUE_QTBTYPE_FIXUP_INTVEC	0x03
/* Force jump to location */
#define KUE_QTBTYPE_JUMP		0x04
/* Force call to location */
#define KUE_QTBTYPE_CALL		0x05
/* Force interrupt call */
#define KUE_QTBTYPE_CALLINTR		0x06
/*
 * Cause data to be written using the specified QT engine
 * interrupt, from starting location in memory for a specified
 * number of bytes.
 */
#define KUE_QTBTYPE_WRITE_WITH_INTR	0x07
/* Cause data from stream to be written using specified QT interrupt. */
#define KUE_QTBTYPE_WRITE_STR_WITH_INTR 0x08
/* Cause data to be written to config locations. */
/* Addresses assume 0xc000 offset. */
#define KUE_QTBTYPE_WRITE_CONFIG	0x09

#define KUE_QTINTR_LOAD_CODE		0x64
#define KUE_QTINTR_TRIGGER_CODE		0x3B
#define KUE_QTINTR_LOAD_CODE_HIGH	0x9C

/* Firmware code segment */
static unsigned char kue_code_seg[] =
{
    /******************************************/
    /* NOTE: B6/C3 is data header signature   */
    /*	     0xAA/0xBB is data length = total */
    /*	     bytes - 7, 0xCC is type, 0xDD is */
    /*	     interrupt to use.		      */
    /******************************************/
    0xB6, 0xC3, 0xf7, 0x0e, 0x02, 0x64,
    0x9f, 0xcf, 0xbc, 0x08, 0xe7, 0x57, 0x00, 0x00,
    0x9a, 0x08, 0x97, 0xc1, 0xe7, 0x67, 0xff, 0x1f,
    0x28, 0xc0, 0xe7, 0x87, 0x00, 0x04, 0x24, 0xc0,
    0xe7, 0x67, 0xff, 0xf9, 0x22, 0xc0, 0x97, 0xcf,
    0xe7, 0x09, 0xa2, 0xc0, 0x94, 0x08, 0xd7, 0x09,
    0x00, 0xc0, 0xe7, 0x59, 0xba, 0x08, 0x94, 0x08,
    0x03, 0xc1, 0xe7, 0x67, 0xff, 0xf7, 0x24, 0xc0,
    0xe7, 0x05, 0x00, 0xc0, 0xa7, 0xcf, 0x92, 0x08,
    0xe7, 0x57, 0x00, 0x00, 0x8e, 0x08, 0xa7, 0xa1,
    0x8e, 0x08, 0x97, 0xcf, 0xe7, 0x57, 0x00, 0x00,
    0xf2, 0x09, 0x0a, 0xc0, 0xe7, 0x57, 0x00, 0x00,
    0xa4, 0xc0, 0xa7, 0xc0, 0x56, 0x08, 0x9f, 0xaf,
    0x70, 0x09, 0xe7, 0x07, 0x00, 0x00, 0xf2, 0x09,
    0xe7, 0x57, 0xff, 0xff, 0x90, 0x08, 0x9f, 0xa0,
    0x40, 0x00, 0xe7, 0x59, 0x90, 0x08, 0x94, 0x08,
    0x9f, 0xa0, 0x40, 0x00, 0xc8, 0x09, 0xa2, 0x08,
    0x08, 0x62, 0x9f, 0xa1, 0x14, 0x0a, 0xe7, 0x57,
    0x00, 0x00, 0x52, 0x08, 0xa7, 0xc0, 0x56, 0x08,
    0x9f, 0xaf, 0x04, 0x00, 0xe7, 0x57, 0x00, 0x00,
    0x8e, 0x08, 0xa7, 0xc1, 0x56, 0x08, 0xc0, 0x09,
    0xa8, 0x08, 0x00, 0x60, 0x05, 0xc4, 0xc0, 0x59,
    0x94, 0x08, 0x02, 0xc0, 0x9f, 0xaf, 0xee, 0x00,
    0xe7, 0x59, 0xae, 0x08, 0x94, 0x08, 0x02, 0xc1,
    0x9f, 0xaf, 0xf6, 0x00, 0x9f, 0xaf, 0x9e, 0x03,
    0xef, 0x57, 0x00, 0x00, 0xf0, 0x09, 0x9f, 0xa1,
    0xde, 0x01, 0xe7, 0x57, 0x00, 0x00, 0x78, 0x08,
    0x9f, 0xa0, 0xe4, 0x03, 0x9f, 0xaf, 0x2c, 0x04,
    0xa7, 0xcf, 0x56, 0x08, 0x48, 0x02, 0xe7, 0x09,
    0x94, 0x08, 0xa8, 0x08, 0xc8, 0x37, 0x04, 0x00,
    0x9f, 0xaf, 0x68, 0x04, 0x97, 0xcf, 0xe7, 0x57,
    0x00, 0x00, 0xa6, 0x08, 0x97, 0xc0, 0xd7, 0x09,
    0x00, 0xc0, 0xc1, 0xdf, 0xc8, 0x09, 0x9c, 0x08,
    0x08, 0x62, 0x1d, 0xc0, 0x27, 0x04, 0x9c, 0x08,
    0x10, 0x94, 0xf0, 0x07, 0xee, 0x09, 0x02, 0x00,
    0xc1, 0x07, 0x01, 0x00, 0x70, 0x00, 0x04, 0x00,
    0xf0, 0x07, 0x44, 0x01, 0x06, 0x00, 0x50, 0xaf,
    0xe7, 0x09, 0x94, 0x08, 0xae, 0x08, 0xe7, 0x17,
    0x14, 0x00, 0xae, 0x08, 0xe7, 0x67, 0xff, 0x07,
    0xae, 0x08, 0xe7, 0x07, 0xff, 0xff, 0xa8, 0x08,
    0xe7, 0x07, 0x00, 0x00, 0xa6, 0x08, 0xe7, 0x05,
    0x00, 0xc0, 0x97, 0xcf, 0xd7, 0x09, 0x00, 0xc0,
    0xc1, 0xdf, 0x48, 0x02, 0xd0, 0x09, 0x9c, 0x08,
    0x27, 0x02, 0x9c, 0x08, 0xe7, 0x09, 0x20, 0xc0,
    0xee, 0x09, 0xe7, 0xd0, 0xee, 0x09, 0xe7, 0x05,
    0x00, 0xc0, 0x97, 0xcf, 0x48, 0x02, 0xc8, 0x37,
    0x04, 0x00, 0x00, 0x0c, 0x0c, 0x00, 0x00, 0x60,
    0x21, 0xc0, 0xc0, 0x37, 0x3e, 0x00, 0x23, 0xc9,
    0xc0, 0x57, 0xb4, 0x05, 0x1b, 0xc8, 0xc0, 0x17,
    0x3f, 0x00, 0xc0, 0x67, 0xc0, 0xff, 0x30, 0x00,
    0x08, 0x00, 0xf0, 0x07, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x02, 0xc0, 0x17, 0x4c, 0x00, 0x30, 0x00,
    0x06, 0x00, 0xf0, 0x07, 0xbe, 0x01, 0x0a, 0x00,
    0x48, 0x02, 0xc1, 0x07, 0x02, 0x00, 0xd7, 0x09,
    0x00, 0xc0, 0xc1, 0xdf, 0x51, 0xaf, 0xe7, 0x05,
    0x00, 0xc0, 0x97, 0xcf, 0x9f, 0xaf, 0x68, 0x04,
    0x9f, 0xaf, 0xe4, 0x03, 0x97, 0xcf, 0x9f, 0xaf,
    0xe4, 0x03, 0xc9, 0x37, 0x04, 0x00, 0xc1, 0xdf,
    0xc8, 0x09, 0x70, 0x08, 0x50, 0x02, 0x67, 0x02,
    0x70, 0x08, 0xd1, 0x07, 0x00, 0x00, 0xc0, 0xdf,
    0x9f, 0xaf, 0xde, 0x01, 0x97, 0xcf, 0xe7, 0x57,
    0x00, 0x00, 0xaa, 0x08, 0x97, 0xc1, 0xe7, 0x57,
    0x01, 0x00, 0x7a, 0x08, 0x97, 0xc0, 0xc8, 0x09,
    0x6e, 0x08, 0x08, 0x62, 0x97, 0xc0, 0x00, 0x02,
    0xc0, 0x17, 0x0e, 0x00, 0x27, 0x00, 0x34, 0x01,
    0x27, 0x0c, 0x0c, 0x00, 0x36, 0x01, 0xef, 0x57,
    0x00, 0x00, 0xf0, 0x09, 0x9f, 0xc0, 0xbe, 0x02,
    0xe7, 0x57, 0x00, 0x00, 0xb0, 0x08, 0x97, 0xc1,
    0xe7, 0x07, 0x09, 0x00, 0x12, 0xc0, 0xe7, 0x77,
    0x00, 0x08, 0x20, 0xc0, 0x9f, 0xc1, 0xb6, 0x02,
    0xe7, 0x57, 0x09, 0x00, 0x12, 0xc0, 0x77, 0xc9,
    0xd7, 0x09, 0x00, 0xc0, 0xc1, 0xdf, 0xe7, 0x77,
    0x00, 0x08, 0x20, 0xc0, 0x2f, 0xc1, 0xe7, 0x07,
    0x00, 0x00, 0x42, 0xc0, 0xe7, 0x07, 0x05, 0x00,
    0x90, 0xc0, 0xc8, 0x07, 0x0a, 0x00, 0xe7, 0x77,
    0x04, 0x00, 0x20, 0xc0, 0x09, 0xc1, 0x08, 0xda,
    0x7a, 0xc1, 0xe7, 0x07, 0x00, 0x01, 0x42, 0xc0,
    0xe7, 0x07, 0x04, 0x00, 0x90, 0xc0, 0x1a, 0xcf,
    0xe7, 0x07, 0x01, 0x00, 0x7a, 0x08, 0x00, 0xd8,
    0x27, 0x50, 0x34, 0x01, 0x17, 0xc1, 0xe7, 0x77,
    0x02, 0x00, 0x20, 0xc0, 0x79, 0xc1, 0x27, 0x50,
    0x34, 0x01, 0x10, 0xc1, 0xe7, 0x77, 0x02, 0x00,
    0x20, 0xc0, 0x79, 0xc0, 0x9f, 0xaf, 0xd8, 0x02,
    0xe7, 0x05, 0x00, 0xc0, 0x00, 0x60, 0x9f, 0xc0,
    0xde, 0x01, 0x97, 0xcf, 0xe7, 0x07, 0x01, 0x00,
    0xb8, 0x08, 0x06, 0xcf, 0xe7, 0x07, 0x30, 0x0e,
    0x02, 0x00, 0xe7, 0x07, 0x50, 0xc3, 0x12, 0xc0,
    0xe7, 0x05, 0x00, 0xc0, 0x97, 0xcf, 0xe7, 0x07,
    0x01, 0x00, 0xb8, 0x08, 0x97, 0xcf, 0xe7, 0x07,
    0x50, 0xc3, 0x12, 0xc0, 0xe7, 0x07, 0x30, 0x0e,
    0x02, 0x00, 0xe7, 0x07, 0x01, 0x00, 0x7a, 0x08,
    0xe7, 0x07, 0x05, 0x00, 0x90, 0xc0, 0x97, 0xcf,
    0xe7, 0x07, 0x00, 0x01, 0x42, 0xc0, 0xe7, 0x07,
    0x04, 0x00, 0x90, 0xc0, 0xe7, 0x07, 0x00, 0x00,
    0x7a, 0x08, 0xe7, 0x57, 0x0f, 0x00, 0xb2, 0x08,
    0x13, 0xc1, 0x9f, 0xaf, 0x2e, 0x08, 0xca, 0x09,
    0xac, 0x08, 0xf2, 0x17, 0x01, 0x00, 0x5c, 0x00,
    0xf2, 0x27, 0x00, 0x00, 0x5e, 0x00, 0xe7, 0x07,
    0x00, 0x00, 0xb2, 0x08, 0xe7, 0x07, 0x01, 0x00,
    0xb4, 0x08, 0xc0, 0x07, 0xff, 0xff, 0x97, 0xcf,
    0x9f, 0xaf, 0x4c, 0x03, 0xc0, 0x69, 0xb4, 0x08,
    0x57, 0x00, 0x9f, 0xde, 0x33, 0x00, 0xc1, 0x05,
    0x27, 0xd8, 0xb2, 0x08, 0x27, 0xd2, 0xb4, 0x08,
    0xe7, 0x87, 0x01, 0x00, 0xb4, 0x08, 0xe7, 0x67,
    0xff, 0x03, 0xb4, 0x08, 0x00, 0x60, 0x97, 0xc0,
    0xe7, 0x07, 0x01, 0x00, 0xb0, 0x08, 0x27, 0x00,
    0x12, 0xc0, 0x97, 0xcf, 0xc0, 0x09, 0xb6, 0x08,
    0x00, 0xd2, 0x02, 0xc3, 0xc0, 0x97, 0x05, 0x80,
    0x27, 0x00, 0xb6, 0x08, 0xc0, 0x99, 0x82, 0x08,
    0xc0, 0x99, 0xa2, 0xc0, 0x97, 0xcf, 0xe7, 0x07,
    0x00, 0x00, 0xb0, 0x08, 0xc0, 0xdf, 0x97, 0xcf,
    0xc8, 0x09, 0x72, 0x08, 0x08, 0x62, 0x02, 0xc0,
    0x10, 0x64, 0x07, 0xc1, 0xe7, 0x07, 0x00, 0x00,
    0x64, 0x08, 0xe7, 0x07, 0xc8, 0x05, 0x24, 0x00,
    0x97, 0xcf, 0x27, 0x04, 0x72, 0x08, 0xc8, 0x17,
    0x0e, 0x00, 0x27, 0x02, 0x64, 0x08, 0xe7, 0x07,
    0xd6, 0x05, 0x24, 0x00, 0x97, 0xcf, 0xd7, 0x09,
    0x00, 0xc0, 0xc1, 0xdf, 0xe7, 0x57, 0x00, 0x00,
    0x62, 0x08, 0x13, 0xc1, 0x9f, 0xaf, 0x70, 0x03,
    0xe7, 0x57, 0x00, 0x00, 0x64, 0x08, 0x13, 0xc0,
    0xe7, 0x09, 0x64, 0x08, 0x30, 0x01, 0xe7, 0x07,
    0xf2, 0x05, 0x32, 0x01, 0xe7, 0x07, 0x10, 0x00,
    0x96, 0xc0, 0xe7, 0x09, 0x64, 0x08, 0x62, 0x08,
    0x04, 0xcf, 0xe7, 0x57, 0x00, 0x00, 0x64, 0x08,
    0x02, 0xc1, 0x9f, 0xaf, 0x70, 0x03, 0xe7, 0x05,
    0x00, 0xc0, 0x97, 0xcf, 0xd7, 0x09, 0x00, 0xc0,
    0xc1, 0xdf, 0xc8, 0x09, 0x72, 0x08, 0x27, 0x02,
    0x78, 0x08, 0x08, 0x62, 0x03, 0xc1, 0xe7, 0x05,
    0x00, 0xc0, 0x97, 0xcf, 0x27, 0x04, 0x72, 0x08,
    0xe7, 0x05, 0x00, 0xc0, 0xf0, 0x07, 0x40, 0x00,
    0x08, 0x00, 0xf0, 0x07, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x02, 0xc0, 0x17, 0x0c, 0x00, 0x30, 0x00,
    0x06, 0x00, 0xf0, 0x07, 0x64, 0x01, 0x0a, 0x00,
    0xc8, 0x17, 0x04, 0x00, 0xc1, 0x07, 0x02, 0x00,
    0x51, 0xaf, 0x97, 0xcf, 0xe7, 0x57, 0x00, 0x00,
    0x6a, 0x08, 0x97, 0xc0, 0xc1, 0xdf, 0xc8, 0x09,
    0x6a, 0x08, 0x27, 0x04, 0x6a, 0x08, 0x27, 0x52,
    0x6c, 0x08, 0x03, 0xc1, 0xe7, 0x07, 0x6a, 0x08,
    0x6c, 0x08, 0xc0, 0xdf, 0x17, 0x02, 0xc8, 0x17,
    0x0e, 0x00, 0x9f, 0xaf, 0x16, 0x05, 0xc8, 0x05,
    0x00, 0x60, 0x03, 0xc0, 0x9f, 0xaf, 0x80, 0x04,
    0x97, 0xcf, 0x9f, 0xaf, 0x68, 0x04, 0x97, 0xcf,
    0xd7, 0x09, 0x00, 0xc0, 0xc1, 0xdf, 0x08, 0x62,
    0x1c, 0xc0, 0xd0, 0x09, 0x72, 0x08, 0x27, 0x02,
    0x72, 0x08, 0xe7, 0x05, 0x00, 0xc0, 0x97, 0xcf,
    0x97, 0x02, 0xca, 0x09, 0xac, 0x08, 0xf2, 0x17,
    0x01, 0x00, 0x04, 0x00, 0xf2, 0x27, 0x00, 0x00,
    0x06, 0x00, 0xca, 0x17, 0x2c, 0x00, 0xf8, 0x77,
    0x01, 0x00, 0x0e, 0x00, 0x06, 0xc0, 0xca, 0xd9,
    0xf8, 0x57, 0xff, 0x00, 0x0e, 0x00, 0x01, 0xc1,
    0xca, 0xd9, 0x22, 0x1c, 0x0c, 0x00, 0xe2, 0x27,
    0x00, 0x00, 0xe2, 0x17, 0x01, 0x00, 0xe2, 0x27,
    0x00, 0x00, 0xca, 0x05, 0x00, 0x0c, 0x0c, 0x00,
    0xc0, 0x17, 0x41, 0x00, 0xc0, 0x67, 0xc0, 0xff,
    0x30, 0x00, 0x08, 0x00, 0x00, 0x02, 0xc0, 0x17,
    0x0c, 0x00, 0x30, 0x00, 0x06, 0x00, 0xf0, 0x07,
    0xdc, 0x00, 0x0a, 0x00, 0xf0, 0x07, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x0c, 0x08, 0x00, 0x40, 0xd1,
    0x01, 0x00, 0xc0, 0x19, 0xa6, 0x08, 0xc0, 0x59,
    0x98, 0x08, 0x04, 0xc9, 0x49, 0xaf, 0x9f, 0xaf,
    0xee, 0x00, 0x4a, 0xaf, 0x67, 0x10, 0xa6, 0x08,
    0xc8, 0x17, 0x04, 0x00, 0xc1, 0x07, 0x01, 0x00,
    0xd7, 0x09, 0x00, 0xc0, 0xc1, 0xdf, 0x50, 0xaf,
    0xe7, 0x05, 0x00, 0xc0, 0x97, 0xcf, 0xc0, 0x07,
    0x01, 0x00, 0xc1, 0x09, 0x7c, 0x08, 0xc1, 0x77,
    0x01, 0x00, 0x97, 0xc1, 0xd8, 0x77, 0x01, 0x00,
    0x12, 0xc0, 0xc9, 0x07, 0x4c, 0x08, 0x9f, 0xaf,
    0x64, 0x05, 0x04, 0xc1, 0xc1, 0x77, 0x08, 0x00,
    0x13, 0xc0, 0x97, 0xcf, 0xc1, 0x77, 0x02, 0x00,
    0x97, 0xc1, 0xc1, 0x77, 0x10, 0x00, 0x0c, 0xc0,
    0x9f, 0xaf, 0x86, 0x05, 0x97, 0xcf, 0xc1, 0x77,
    0x04, 0x00, 0x06, 0xc0, 0xc9, 0x07, 0x7e, 0x08,
    0x9f, 0xaf, 0x64, 0x05, 0x97, 0xc0, 0x00, 0xcf,
    0x00, 0x90, 0x97, 0xcf, 0x50, 0x54, 0x97, 0xc1,
    0x70, 0x5c, 0x02, 0x00, 0x02, 0x00, 0x97, 0xc1,
    0x70, 0x5c, 0x04, 0x00, 0x04, 0x00, 0x97, 0xcf,
    0xc0, 0x00, 0x60, 0x00, 0x30, 0x00, 0x18, 0x00,
    0x0c, 0x00, 0x06, 0x00, 0x00, 0x00, 0xcb, 0x09,
    0x88, 0x08, 0xcc, 0x09, 0x8a, 0x08, 0x0b, 0x53,
    0x11, 0xc0, 0xc9, 0x02, 0xca, 0x07, 0x78, 0x05,
    0x9f, 0xaf, 0x64, 0x05, 0x97, 0xc0, 0x0a, 0xc8,
    0x82, 0x08, 0x0a, 0xcf, 0x82, 0x08, 0x9f, 0xaf,
    0x64, 0x05, 0x97, 0xc0, 0x05, 0xc2, 0x89, 0x30,
    0x82, 0x60, 0x78, 0xc1, 0x00, 0x90, 0x97, 0xcf,
    0x89, 0x10, 0x09, 0x53, 0x79, 0xc2, 0x89, 0x30,
    0x82, 0x08, 0x7a, 0xcf, 0xc0, 0xdf, 0x97, 0xcf,
    0xe7, 0x09, 0x96, 0xc0, 0x66, 0x08, 0xe7, 0x09,
    0x98, 0xc0, 0x68, 0x08, 0x0f, 0xcf, 0xe7, 0x09,
    0x96, 0xc0, 0x66, 0x08, 0xe7, 0x09, 0x98, 0xc0,
    0x68, 0x08, 0xe7, 0x09, 0x64, 0x08, 0x30, 0x01,
    0xe7, 0x07, 0xf2, 0x05, 0x32, 0x01, 0xe7, 0x07,
    0x10, 0x00, 0x96, 0xc0, 0xd7, 0x09, 0x00, 0xc0,
    0x17, 0x02, 0xc8, 0x09, 0x62, 0x08, 0xc8, 0x37,
    0x0e, 0x00, 0xe7, 0x57, 0x04, 0x00, 0x68, 0x08,
    0x3d, 0xc0, 0xe7, 0x87, 0x00, 0x08, 0x24, 0xc0,
    0xe7, 0x09, 0x94, 0x08, 0xba, 0x08, 0xe7, 0x17,
    0x64, 0x00, 0xba, 0x08, 0xe7, 0x67, 0xff, 0x07,
    0xba, 0x08, 0xe7, 0x77, 0x2a, 0x00, 0x66, 0x08,
    0x30, 0xc0, 0x97, 0x02, 0xca, 0x09, 0xac, 0x08,
    0xe7, 0x77, 0x20, 0x00, 0x66, 0x08, 0x0e, 0xc0,
    0xf2, 0x17, 0x01, 0x00, 0x10, 0x00, 0xf2, 0x27,
    0x00, 0x00, 0x12, 0x00, 0xe7, 0x77, 0x0a, 0x00,
    0x66, 0x08, 0xca, 0x05, 0x1e, 0xc0, 0x97, 0x02,
    0xca, 0x09, 0xac, 0x08, 0xf2, 0x17, 0x01, 0x00,
    0x0c, 0x00, 0xf2, 0x27, 0x00, 0x00, 0x0e, 0x00,
    0xe7, 0x77, 0x02, 0x00, 0x66, 0x08, 0x07, 0xc0,
    0xf2, 0x17, 0x01, 0x00, 0x44, 0x00, 0xf2, 0x27,
    0x00, 0x00, 0x46, 0x00, 0x06, 0xcf, 0xf2, 0x17,
    0x01, 0x00, 0x60, 0x00, 0xf2, 0x27, 0x00, 0x00,
    0x62, 0x00, 0xca, 0x05, 0x9f, 0xaf, 0x68, 0x04,
    0x0f, 0xcf, 0x57, 0x02, 0x09, 0x02, 0xf1, 0x09,
    0x68, 0x08, 0x0c, 0x00, 0xf1, 0xda, 0x0c, 0x00,
    0xc8, 0x09, 0x6c, 0x08, 0x50, 0x02, 0x67, 0x02,
    0x6c, 0x08, 0xd1, 0x07, 0x00, 0x00, 0xc9, 0x05,
    0xe7, 0x09, 0x64, 0x08, 0x62, 0x08, 0xe7, 0x57,
    0x00, 0x00, 0x62, 0x08, 0x02, 0xc0, 0x9f, 0xaf,
    0x70, 0x03, 0xc8, 0x05, 0xe7, 0x05, 0x00, 0xc0,
    0xc0, 0xdf, 0x97, 0xcf, 0xd7, 0x09, 0x00, 0xc0,
    0x17, 0x00, 0x17, 0x02, 0x97, 0x02, 0xc0, 0x09,
    0x92, 0xc0, 0xe7, 0x87, 0x00, 0x08, 0x24, 0xc0,
    0xe7, 0x09, 0x94, 0x08, 0xba, 0x08, 0xe7, 0x17,
    0x64, 0x00, 0xba, 0x08, 0xe7, 0x67, 0xff, 0x07,
    0xba, 0x08, 0xe7, 0x07, 0x04, 0x00, 0x90, 0xc0,
    0xca, 0x09, 0xac, 0x08, 0xe7, 0x07, 0x00, 0x00,
    0x7a, 0x08, 0xe7, 0x07, 0x66, 0x03, 0x02, 0x00,
    0xc0, 0x77, 0x02, 0x00, 0x10, 0xc0, 0xef, 0x57,
    0x00, 0x00, 0xf0, 0x09, 0x04, 0xc0, 0x9f, 0xaf,
    0xd8, 0x02, 0x9f, 0xcf, 0x12, 0x08, 0xf2, 0x17,
    0x01, 0x00, 0x50, 0x00, 0xf2, 0x27, 0x00, 0x00,
    0x52, 0x00, 0x9f, 0xcf, 0x12, 0x08, 0xef, 0x57,
    0x00, 0x00, 0xf0, 0x09, 0x08, 0xc0, 0xe7, 0x57,
    0x00, 0x00, 0xb8, 0x08, 0xe7, 0x07, 0x00, 0x00,
    0xb8, 0x08, 0x0a, 0xc0, 0x03, 0xcf, 0xc0, 0x77,
    0x10, 0x00, 0x06, 0xc0, 0xf2, 0x17, 0x01, 0x00,
    0x58, 0x00, 0xf2, 0x27, 0x00, 0x00, 0x5a, 0x00,
    0xc0, 0x77, 0x80, 0x00, 0x06, 0xc0, 0xf2, 0x17,
    0x01, 0x00, 0x70, 0x00, 0xf2, 0x27, 0x00, 0x00,
    0x72, 0x00, 0xc0, 0x77, 0x08, 0x00, 0x1d, 0xc1,
    0xf2, 0x17, 0x01, 0x00, 0x08, 0x00, 0xf2, 0x27,
    0x00, 0x00, 0x0a, 0x00, 0xc0, 0x77, 0x00, 0x02,
    0x06, 0xc0, 0xf2, 0x17, 0x01, 0x00, 0x64, 0x00,
    0xf2, 0x27, 0x00, 0x00, 0x66, 0x00, 0xc0, 0x77,
    0x40, 0x00, 0x06, 0xc0, 0xf2, 0x17, 0x01, 0x00,
    0x5c, 0x00, 0xf2, 0x27, 0x00, 0x00, 0x5e, 0x00,
    0xc0, 0x77, 0x01, 0x00, 0x01, 0xc0, 0x37, 0xcf,
    0x36, 0xcf, 0xf2, 0x17, 0x01, 0x00, 0x00, 0x00,
    0xf2, 0x27, 0x00, 0x00, 0x02, 0x00, 0xef, 0x57,
    0x00, 0x00, 0xf0, 0x09, 0x18, 0xc0, 0xe7, 0x57,
    0x01, 0x00, 0xb2, 0x08, 0x0e, 0xc2, 0x07, 0xc8,
    0xf2, 0x17, 0x01, 0x00, 0x50, 0x00, 0xf2, 0x27,
    0x00, 0x00, 0x52, 0x00, 0x06, 0xcf, 0xf2, 0x17,
    0x01, 0x00, 0x54, 0x00, 0xf2, 0x27, 0x00, 0x00,
    0x56, 0x00, 0xe7, 0x07, 0x00, 0x00, 0xb2, 0x08,
    0xe7, 0x07, 0x01, 0x00, 0xb4, 0x08, 0xc8, 0x09,
    0x34, 0x01, 0xca, 0x17, 0x14, 0x00, 0xd8, 0x77,
    0x01, 0x00, 0x05, 0xc0, 0xca, 0xd9, 0xd8, 0x57,
    0xff, 0x00, 0x01, 0xc0, 0xca, 0xd9, 0xe2, 0x19,
    0x94, 0xc0, 0xe2, 0x27, 0x00, 0x00, 0xe2, 0x17,
    0x01, 0x00, 0xe2, 0x27, 0x00, 0x00, 0x9f, 0xaf,
    0x2e, 0x08, 0x9f, 0xaf, 0xde, 0x01, 0xe7, 0x57,
    0x00, 0x00, 0xaa, 0x08, 0x9f, 0xa1, 0xf0, 0x0b,
    0xca, 0x05, 0xc8, 0x05, 0xc0, 0x05, 0xe7, 0x05,
    0x00, 0xc0, 0xc0, 0xdf, 0x97, 0xcf, 0xc8, 0x09,
    0x6e, 0x08, 0x08, 0x62, 0x97, 0xc0, 0x27, 0x04,
    0x6e, 0x08, 0x27, 0x52, 0x70, 0x08, 0x03, 0xc1,
    0xe7, 0x07, 0x6e, 0x08, 0x70, 0x08, 0x9f, 0xaf,
    0x68, 0x04, 0x97, 0xcf, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x33, 0xcc,
    0x00, 0x00, 0x00, 0x00, 0xe7, 0x57, 0x00, 0x80,
    0xb2, 0x00, 0x06, 0xc2, 0xe7, 0x07, 0x52, 0x0e,
    0x12, 0x00, 0xe7, 0x07, 0x98, 0x0e, 0xb2, 0x00,
    0xe7, 0x07, 0xa4, 0x09, 0xf2, 0x02, 0xc8, 0x09,
    0xb4, 0x00, 0xf8, 0x07, 0x02, 0x00, 0x0d, 0x00,
    0xd7, 0x09, 0x0e, 0xc0, 0xe7, 0x07, 0x00, 0x00,
    0x0e, 0xc0, 0xc8, 0x09, 0xdc, 0x00, 0xf0, 0x07,
    0xff, 0xff, 0x09, 0x00, 0xf0, 0x07, 0xfb, 0x13,
    0x0b, 0x00, 0xe7, 0x09, 0xc0, 0x00, 0x58, 0x08,
    0xe7, 0x09, 0xbe, 0x00, 0x54, 0x08, 0xe7, 0x09,
    0x10, 0x00, 0x92, 0x08, 0xc8, 0x07, 0xb4, 0x09,
    0x9f, 0xaf, 0x8c, 0x09, 0x9f, 0xaf, 0xe2, 0x0b,
    0xc0, 0x07, 0x80, 0x01, 0x44, 0xaf, 0x27, 0x00,
    0x88, 0x08, 0x27, 0x00, 0x8a, 0x08, 0x27, 0x00,
    0x8c, 0x08, 0xc0, 0x07, 0x74, 0x00, 0x44, 0xaf,
    0x27, 0x00, 0xac, 0x08, 0x08, 0x00, 0x00, 0x90,
    0xc1, 0x07, 0x1d, 0x00, 0x20, 0x00, 0x20, 0x00,
    0x01, 0xda, 0x7c, 0xc1, 0x9f, 0xaf, 0x8a, 0x0b,
    0xc0, 0x07, 0x4c, 0x00, 0x48, 0xaf, 0x27, 0x00,
    0x56, 0x08, 0x9f, 0xaf, 0x72, 0x0c, 0xe7, 0x07,
    0x00, 0x80, 0x96, 0x08, 0xef, 0x57, 0x00, 0x00,
    0xf0, 0x09, 0x03, 0xc0, 0xe7, 0x07, 0x01, 0x00,
    0x1c, 0xc0, 0xe7, 0x05, 0x0e, 0xc0, 0x97, 0xcf,
    0x49, 0xaf, 0xe7, 0x87, 0x43, 0x00, 0x0e, 0xc0,
    0xe7, 0x07, 0xff, 0xff, 0x94, 0x08, 0x9f, 0xaf,
    0x8a, 0x0c, 0xc0, 0x07, 0x01, 0x00, 0x60, 0xaf,
    0x4a, 0xaf, 0x97, 0xcf, 0x00, 0x08, 0x09, 0x08,
    0x11, 0x08, 0x00, 0xda, 0x7c, 0xc1, 0x97, 0xcf,
    0x67, 0x04, 0xcc, 0x02, 0xc0, 0xdf, 0x51, 0x94,
    0xb1, 0xaf, 0x06, 0x00, 0xc1, 0xdf, 0xc9, 0x09,
    0xcc, 0x02, 0x49, 0x62, 0x75, 0xc1, 0xc0, 0xdf,
    0xa7, 0xcf, 0xd6, 0x02, 0x0e, 0x00, 0x24, 0x00,
    0xd6, 0x05, 0x22, 0x00, 0xc4, 0x06, 0xd0, 0x00,
    0xf0, 0x0b, 0xaa, 0x00, 0x0e, 0x0a, 0xbe, 0x00,
    0x2c, 0x0c, 0x10, 0x00, 0x20, 0x00, 0x04, 0x00,
    0xc4, 0x05, 0x02, 0x00, 0x66, 0x03, 0x06, 0x00,
    0x00, 0x00, 0x24, 0xc0, 0x04, 0x04, 0x28, 0xc0,
    0xfe, 0xfb, 0x1e, 0xc0, 0x00, 0x04, 0x22, 0xc0,
    0xff, 0xf0, 0xc0, 0x00, 0x60, 0x0b, 0x00, 0x00,
    0x00, 0x00, 0xff, 0xff, 0x34, 0x0a, 0x3e, 0x0a,
    0x9e, 0x0a, 0xa8, 0x0a, 0xce, 0x0a, 0xd2, 0x0a,
    0xd6, 0x0a, 0x00, 0x0b, 0x10, 0x0b, 0x1e, 0x0b,
    0x20, 0x0b, 0x28, 0x0b, 0x28, 0x0b, 0x27, 0x02,
    0xa2, 0x08, 0x97, 0xcf, 0xe7, 0x07, 0x00, 0x00,
    0xa2, 0x08, 0x0a, 0x0e, 0x01, 0x00, 0xca, 0x57,
    0x0e, 0x00, 0x9f, 0xc3, 0x2a, 0x0b, 0xca, 0x37,
    0x00, 0x00, 0x9f, 0xc2, 0x2a, 0x0b, 0x0a, 0xd2,
    0xb2, 0xcf, 0xf4, 0x09, 0xc8, 0x09, 0xde, 0x00,
    0x07, 0x06, 0x9f, 0xcf, 0x3c, 0x0b, 0xf0, 0x57,
    0x80, 0x01, 0x06, 0x00, 0x9f, 0xc8, 0x2a, 0x0b,
    0x27, 0x0c, 0x02, 0x00, 0x86, 0x08, 0xc0, 0x09,
    0x88, 0x08, 0x27, 0x00, 0x8a, 0x08, 0xe7, 0x07,
    0x00, 0x00, 0x84, 0x08, 0x27, 0x00, 0x5c, 0x08,
    0x00, 0x1c, 0x06, 0x00, 0x27, 0x00, 0x8c, 0x08,
    0x41, 0x90, 0x67, 0x50, 0x86, 0x08, 0x0d, 0xc0,
    0x67, 0x00, 0x5a, 0x08, 0x27, 0x0c, 0x06, 0x00,
    0x5e, 0x08, 0xe7, 0x07, 0x8a, 0x0a, 0x60, 0x08,
    0xc8, 0x07, 0x5a, 0x08, 0x41, 0x90, 0x51, 0xaf,
    0x97, 0xcf, 0x9f, 0xaf, 0xac, 0x0e, 0xe7, 0x09,
    0x8c, 0x08, 0x8a, 0x08, 0xe7, 0x09, 0x86, 0x08,
    0x84, 0x08, 0x59, 0xaf, 0x97, 0xcf, 0x27, 0x0c,
    0x02, 0x00, 0x7c, 0x08, 0x59, 0xaf, 0x97, 0xcf,
    0x09, 0x0c, 0x02, 0x00, 0x09, 0xda, 0x49, 0xd2,
    0xc9, 0x19, 0xac, 0x08, 0xc8, 0x07, 0x5a, 0x08,
    0xe0, 0x07, 0x00, 0x00, 0x60, 0x02, 0xe0, 0x07,
    0x04, 0x00, 0xd0, 0x07, 0x9a, 0x0a, 0x48, 0xdb,
    0x41, 0x90, 0x50, 0xaf, 0x97, 0xcf, 0x59, 0xaf,
    0x97, 0xcf, 0x59, 0xaf, 0x97, 0xcf, 0xf0, 0x57,
    0x06, 0x00, 0x06, 0x00, 0x26, 0xc1, 0xe7, 0x07,
    0x7e, 0x08, 0x5c, 0x08, 0x41, 0x90, 0x67, 0x00,
    0x5a, 0x08, 0x27, 0x0c, 0x06, 0x00, 0x5e, 0x08,
    0xe7, 0x07, 0x5c, 0x0b, 0x60, 0x08, 0xc8, 0x07,
    0x5a, 0x08, 0x41, 0x90, 0x51, 0xaf, 0x97, 0xcf,
    0x07, 0x0c, 0x06, 0x00, 0xc7, 0x57, 0x06, 0x00,
    0x10, 0xc1, 0xc8, 0x07, 0x7e, 0x08, 0x16, 0xcf,
    0x00, 0x0c, 0x02, 0x00, 0x00, 0xda, 0x40, 0xd1,
    0x27, 0x00, 0x98, 0x08, 0x1f, 0xcf, 0x1e, 0xcf,
    0x27, 0x0c, 0x02, 0x00, 0xa4, 0x08, 0x1a, 0xcf,
    0x00, 0xcf, 0x27, 0x02, 0x20, 0x01, 0xe7, 0x07,
    0x08, 0x00, 0x22, 0x01, 0xe7, 0x07, 0x13, 0x00,
    0xb0, 0xc0, 0x97, 0xcf, 0x41, 0x90, 0x67, 0x00,
    0x5a, 0x08, 0xe7, 0x01, 0x5e, 0x08, 0x27, 0x02,
    0x5c, 0x08, 0xe7, 0x07, 0x5c, 0x0b, 0x60, 0x08,
    0xc8, 0x07, 0x5a, 0x08, 0xc1, 0x07, 0x00, 0x80,
    0x50, 0xaf, 0x97, 0xcf, 0x59, 0xaf, 0x97, 0xcf,
    0x00, 0x60, 0x05, 0xc0, 0xe7, 0x07, 0x00, 0x00,
    0x9a, 0x08, 0xa7, 0xcf, 0x58, 0x08, 0x9f, 0xaf,
    0xe2, 0x0b, 0xe7, 0x07, 0x01, 0x00, 0x9a, 0x08,
    0x49, 0xaf, 0xd7, 0x09, 0x00, 0xc0, 0x07, 0xaf,
    0xe7, 0x05, 0x00, 0xc0, 0x4a, 0xaf, 0xa7, 0xcf,
    0x58, 0x08, 0xc0, 0x07, 0x40, 0x00, 0x44, 0xaf,
    0x27, 0x00, 0xa0, 0x08, 0x08, 0x00, 0xc0, 0x07,
    0x20, 0x00, 0x20, 0x94, 0x00, 0xda, 0x7d, 0xc1,
    0xc0, 0x07, 0xfe, 0x7f, 0x44, 0xaf, 0x40, 0x00,
    0x41, 0x90, 0xc0, 0x37, 0x08, 0x00, 0xdf, 0xde,
    0x50, 0x06, 0xc0, 0x57, 0x10, 0x00, 0x02, 0xc2,
    0xc0, 0x07, 0x10, 0x00, 0x27, 0x00, 0x76, 0x08,
    0x41, 0x90, 0x9f, 0xde, 0x40, 0x06, 0x44, 0xaf,
    0x27, 0x00, 0x74, 0x08, 0xc0, 0x09, 0x76, 0x08,
    0x41, 0x90, 0x00, 0xd2, 0x00, 0xd8, 0x9f, 0xde,
    0x08, 0x00, 0x44, 0xaf, 0x27, 0x00, 0x9e, 0x08,
    0x97, 0xcf, 0xe7, 0x87, 0x00, 0x84, 0x28, 0xc0,
    0xe7, 0x67, 0xff, 0xf3, 0x24, 0xc0, 0x97, 0xcf,
    0xe7, 0x87, 0x01, 0x00, 0xaa, 0x08, 0xe7, 0x57,
    0x00, 0x00, 0x7a, 0x08, 0x97, 0xc1, 0x9f, 0xaf,
    0xe2, 0x0b, 0xe7, 0x87, 0x00, 0x06, 0x22, 0xc0,
    0xe7, 0x07, 0x00, 0x00, 0x90, 0xc0, 0xe7, 0x67,
    0xfe, 0xff, 0x3e, 0xc0, 0xe7, 0x07, 0x2e, 0x00,
    0x0a, 0xc0, 0xe7, 0x87, 0x01, 0x00, 0x3e, 0xc0,
    0xe7, 0x07, 0xff, 0xff, 0x94, 0x08, 0x9f, 0xaf,
    0xf0, 0x0c, 0x97, 0xcf, 0x17, 0x00, 0xa7, 0xaf,
    0x54, 0x08, 0xc0, 0x05, 0x27, 0x00, 0x52, 0x08,
    0xe7, 0x87, 0x01, 0x00, 0xaa, 0x08, 0x9f, 0xaf,
    0xe2, 0x0b, 0xe7, 0x07, 0x0c, 0x00, 0x40, 0xc0,
    0x9f, 0xaf, 0xf0, 0x0c, 0xe7, 0x07, 0x00, 0x00,
    0x78, 0x08, 0x00, 0x90, 0xe7, 0x09, 0x88, 0x08,
    0x8a, 0x08, 0x27, 0x00, 0x84, 0x08, 0x27, 0x00,
    0x7c, 0x08, 0x9f, 0xaf, 0x8a, 0x0c, 0xe7, 0x07,
    0x00, 0x00, 0xb2, 0x02, 0xe7, 0x07, 0x00, 0x00,
    0xb4, 0x02, 0xc0, 0x07, 0x06, 0x00, 0xc8, 0x09,
    0xde, 0x00, 0xc8, 0x17, 0x03, 0x00, 0xc9, 0x07,
    0x7e, 0x08, 0x29, 0x0a, 0x00, 0xda, 0x7d, 0xc1,
    0x97, 0xcf, 0xd7, 0x09, 0x00, 0xc0, 0xc1, 0xdf,
    0x00, 0x90, 0x27, 0x00, 0x6a, 0x08, 0xe7, 0x07,
    0x6a, 0x08, 0x6c, 0x08, 0x27, 0x00, 0x6e, 0x08,
    0xe7, 0x07, 0x6e, 0x08, 0x70, 0x08, 0x27, 0x00,
    0x78, 0x08, 0x27, 0x00, 0x62, 0x08, 0x27, 0x00,
    0x64, 0x08, 0xc8, 0x09, 0x74, 0x08, 0xc1, 0x09,
    0x76, 0x08, 0xc9, 0x07, 0x72, 0x08, 0x11, 0x02,
    0x09, 0x02, 0xc8, 0x17, 0x40, 0x06, 0x01, 0xda,
    0x7a, 0xc1, 0x51, 0x94, 0xc8, 0x09, 0x9e, 0x08,
    0xc9, 0x07, 0x9c, 0x08, 0xc1, 0x09, 0x76, 0x08,
    0x01, 0xd2, 0x01, 0xd8, 0x11, 0x02, 0x09, 0x02,
    0xc8, 0x17, 0x08, 0x00, 0x01, 0xda, 0x7a, 0xc1,
    0x51, 0x94, 0xe7, 0x05, 0x00, 0xc0, 0x97, 0xcf,
    0xe7, 0x57, 0x00, 0x00, 0x52, 0x08, 0x97, 0xc0,
    0x9f, 0xaf, 0x04, 0x00, 0xe7, 0x09, 0x94, 0x08,
    0x90, 0x08, 0xe7, 0x57, 0xff, 0xff, 0x90, 0x08,
    0x04, 0xc1, 0xe7, 0x07, 0xf0, 0x0c, 0x8e, 0x08,
    0x97, 0xcf, 0xe7, 0x17, 0x32, 0x00, 0x90, 0x08,
    0xe7, 0x67, 0xff, 0x07, 0x90, 0x08, 0xe7, 0x07,
    0x26, 0x0d, 0x8e, 0x08, 0x97, 0xcf, 0xd7, 0x09,
    0x00, 0xc0, 0xc1, 0xdf, 0xe7, 0x57, 0x00, 0x00,
    0x96, 0x08, 0x23, 0xc0, 0xe7, 0x07, 0x00, 0x80,
    0x80, 0xc0, 0xe7, 0x07, 0x04, 0x00, 0x90, 0xc0,
    0xe7, 0x07, 0x00, 0x00, 0x80, 0xc0, 0xe7, 0x07,
    0x00, 0x80, 0x80, 0xc0, 0xc0, 0x07, 0x00, 0x00,
    0xc0, 0x07, 0x00, 0x00, 0xc0, 0x07, 0x00, 0x00,
    0xe7, 0x07, 0x00, 0x00, 0x80, 0xc0, 0xe7, 0x07,
    0x00, 0x80, 0x80, 0xc0, 0xe7, 0x07, 0x00, 0x80,
    0x40, 0xc0, 0xc0, 0x07, 0x00, 0x00, 0xe7, 0x07,
    0x00, 0x00, 0x40, 0xc0, 0xe7, 0x07, 0x00, 0x00,
    0x80, 0xc0, 0xef, 0x57, 0x00, 0x00, 0xf1, 0x09,
    0x9f, 0xa0, 0xc0, 0x0d, 0xe7, 0x07, 0x04, 0x00,
    0x90, 0xc0, 0xe7, 0x07, 0x00, 0x02, 0x40, 0xc0,
    0xe7, 0x07, 0x0c, 0x02, 0x40, 0xc0, 0xe7, 0x07,
    0x00, 0x00, 0x96, 0x08, 0xe7, 0x07, 0x00, 0x00,
    0x8e, 0x08, 0xe7, 0x07, 0x00, 0x00, 0xaa, 0x08,
    0xd7, 0x09, 0x00, 0xc0, 0xc1, 0xdf, 0x9f, 0xaf,
    0x9e, 0x03, 0xe7, 0x05, 0x00, 0xc0, 0x9f, 0xaf,
    0xde, 0x01, 0xe7, 0x05, 0x00, 0xc0, 0x97, 0xcf,
    0x9f, 0xaf, 0xde, 0x0d, 0xef, 0x77, 0x00, 0x00,
    0xf1, 0x09, 0x97, 0xc1, 0x9f, 0xaf, 0xde, 0x0d,
    0xef, 0x77, 0x00, 0x00, 0xf1, 0x09, 0x97, 0xc1,
    0xef, 0x07, 0x01, 0x00, 0xf1, 0x09, 0xe7, 0x87,
    0x00, 0x08, 0x1e, 0xc0, 0xe7, 0x87, 0x00, 0x08,
    0x22, 0xc0, 0xe7, 0x67, 0xff, 0xf7, 0x22, 0xc0,
    0xe7, 0x77, 0x00, 0x08, 0x20, 0xc0, 0x11, 0xc0,
    0xe7, 0x67, 0xff, 0xf7, 0x1e, 0xc0, 0xe7, 0x87,
    0x00, 0x08, 0x22, 0xc0, 0xe7, 0x67, 0xff, 0xf7,
    0x22, 0xc0, 0xe7, 0x77, 0x00, 0x08, 0x20, 0xc0,
    0x04, 0xc1, 0xe7, 0x87, 0x00, 0x08, 0x22, 0xc0,
    0x97, 0xcf, 0xe7, 0x07, 0x01, 0x01, 0xf0, 0x09,
    0xef, 0x57, 0x18, 0x00, 0xfe, 0xff, 0x97, 0xc2,
    0xef, 0x07, 0x00, 0x00, 0xf0, 0x09, 0x97, 0xcf,
    0xd7, 0x09, 0x00, 0xc0, 0x17, 0x00, 0x17, 0x02,
    0x97, 0x02, 0xe7, 0x57, 0x00, 0x00, 0x7a, 0x08,
    0x06, 0xc0, 0xc0, 0x09, 0x92, 0xc0, 0xc0, 0x77,
    0x09, 0x02, 0x9f, 0xc1, 0xea, 0x06, 0x9f, 0xcf,
    0x20, 0x08, 0xd7, 0x09, 0x0e, 0xc0, 0xe7, 0x07,
    0x00, 0x00, 0x0e, 0xc0, 0x9f, 0xaf, 0x66, 0x0e,
    0xe7, 0x05, 0x0e, 0xc0, 0x97, 0xcf, 0xd7, 0x09,
    0x00, 0xc0, 0x17, 0x02, 0xc8, 0x09, 0xb0, 0xc0,
    0xe7, 0x67, 0xfe, 0x7f, 0xb0, 0xc0, 0xc8, 0x77,
    0x00, 0x20, 0x9f, 0xc1, 0x64, 0xeb, 0xe7, 0x57,
    0x00, 0x00, 0xc8, 0x02, 0x9f, 0xc1, 0x80, 0xeb,
    0xc8, 0x99, 0xca, 0x02, 0xc8, 0x67, 0x04, 0x00,
    0x9f, 0xc1, 0x96, 0xeb, 0x9f, 0xcf, 0x4c, 0xeb,
    0xe7, 0x07, 0x00, 0x00, 0xa6, 0xc0, 0xe7, 0x09,
    0xb0, 0xc0, 0xc8, 0x02, 0xe7, 0x07, 0x03, 0x00,
    0xb0, 0xc0, 0x97, 0xcf, 0xc0, 0x09, 0x86, 0x08,
    0xc0, 0x37, 0x01, 0x00, 0x97, 0xc9, 0xc9, 0x09,
    0x88, 0x08, 0x02, 0x00, 0x41, 0x90, 0x48, 0x02,
    0xc9, 0x17, 0x06, 0x00, 0x9f, 0xaf, 0x64, 0x05,
    0x9f, 0xa2, 0xd6, 0x0e, 0x02, 0xda, 0x77, 0xc1,
    0x41, 0x60, 0x71, 0xc1, 0x97, 0xcf, 0x17, 0x02,
    0x57, 0x02, 0x43, 0x04, 0x21, 0x04, 0xe0, 0x00,
    0x43, 0x04, 0x21, 0x04, 0xe0, 0x00, 0x43, 0x04,
    0x21, 0x04, 0xe0, 0x00, 0xc1, 0x07, 0x01, 0x00,
    0xc9, 0x05, 0xc8, 0x05, 0x97, 0xcf,
    0,	  0
};

/* Firmware fixup (data?) segment */
static unsigned char kue_fix_seg[] =
{
    /******************************************/
    /* NOTE: B6/C3 is data header signature   */
    /*	     0xAA/0xBB is data length = total */
    /*	     bytes - 7, 0xCC is type, 0xDD is */
    /*	     interrupt to use.		      */
    /******************************************/
    0xB6, 0xC3, 0xc9, 0x02, 0x03, 0x64,
    0x02, 0x00, 0x08, 0x00, 0x24, 0x00, 0x2e, 0x00,
    0x2c, 0x00, 0x3e, 0x00, 0x44, 0x00, 0x48, 0x00,
    0x50, 0x00, 0x5c, 0x00, 0x60, 0x00, 0x66, 0x00,
    0x6c, 0x00, 0x70, 0x00, 0x76, 0x00, 0x74, 0x00,
    0x7a, 0x00, 0x7e, 0x00, 0x84, 0x00, 0x8a, 0x00,
    0x8e, 0x00, 0x92, 0x00, 0x98, 0x00, 0x9c, 0x00,
    0xa0, 0x00, 0xa8, 0x00, 0xae, 0x00, 0xb4, 0x00,
    0xb2, 0x00, 0xba, 0x00, 0xbe, 0x00, 0xc4, 0x00,
    0xc8, 0x00, 0xce, 0x00, 0xd2, 0x00, 0xd6, 0x00,
    0xda, 0x00, 0xe2, 0x00, 0xe0, 0x00, 0xea, 0x00,
    0xf2, 0x00, 0xfe, 0x00, 0x06, 0x01, 0x0c, 0x01,
    0x1a, 0x01, 0x24, 0x01, 0x22, 0x01, 0x2a, 0x01,
    0x30, 0x01, 0x36, 0x01, 0x3c, 0x01, 0x4e, 0x01,
    0x52, 0x01, 0x58, 0x01, 0x5c, 0x01, 0x9c, 0x01,
    0xb6, 0x01, 0xba, 0x01, 0xc0, 0x01, 0xca, 0x01,
    0xd0, 0x01, 0xda, 0x01, 0xe2, 0x01, 0xea, 0x01,
    0xf0, 0x01, 0x0a, 0x02, 0x0e, 0x02, 0x14, 0x02,
    0x26, 0x02, 0x6c, 0x02, 0x8e, 0x02, 0x98, 0x02,
    0xa0, 0x02, 0xa6, 0x02, 0xba, 0x02, 0xc6, 0x02,
    0xce, 0x02, 0xe8, 0x02, 0xee, 0x02, 0xf4, 0x02,
    0xf8, 0x02, 0x0a, 0x03, 0x10, 0x03, 0x1a, 0x03,
    0x1e, 0x03, 0x2a, 0x03, 0x2e, 0x03, 0x34, 0x03,
    0x3a, 0x03, 0x44, 0x03, 0x4e, 0x03, 0x5a, 0x03,
    0x5e, 0x03, 0x6a, 0x03, 0x72, 0x03, 0x80, 0x03,
    0x84, 0x03, 0x8c, 0x03, 0x94, 0x03, 0x98, 0x03,
    0xa8, 0x03, 0xae, 0x03, 0xb4, 0x03, 0xba, 0x03,
    0xce, 0x03, 0xcc, 0x03, 0xd6, 0x03, 0xdc, 0x03,
    0xec, 0x03, 0xf0, 0x03, 0xfe, 0x03, 0x1c, 0x04,
    0x30, 0x04, 0x38, 0x04, 0x3c, 0x04, 0x40, 0x04,
    0x48, 0x04, 0x46, 0x04, 0x54, 0x04, 0x5e, 0x04,
    0x64, 0x04, 0x74, 0x04, 0x78, 0x04, 0x84, 0x04,
    0xd8, 0x04, 0xec, 0x04, 0xf0, 0x04, 0xf8, 0x04,
    0xfe, 0x04, 0x1c, 0x05, 0x2c, 0x05, 0x30, 0x05,
    0x4a, 0x05, 0x56, 0x05, 0x5a, 0x05, 0x88, 0x05,
    0x8c, 0x05, 0x96, 0x05, 0x9a, 0x05, 0xa8, 0x05,
    0xcc, 0x05, 0xd2, 0x05, 0xda, 0x05, 0xe0, 0x05,
    0xe4, 0x05, 0xfc, 0x05, 0x06, 0x06, 0x14, 0x06,
    0x12, 0x06, 0x1a, 0x06, 0x20, 0x06, 0x26, 0x06,
    0x2e, 0x06, 0x34, 0x06, 0x48, 0x06, 0x52, 0x06,
    0x64, 0x06, 0x86, 0x06, 0x90, 0x06, 0x9a, 0x06,
    0xa0, 0x06, 0xac, 0x06, 0xaa, 0x06, 0xb2, 0x06,
    0xb8, 0x06, 0xdc, 0x06, 0xda, 0x06, 0xe2, 0x06,
    0xe8, 0x06, 0xf2, 0x06, 0xf8, 0x06, 0xfc, 0x06,
    0x0a, 0x07, 0x10, 0x07, 0x14, 0x07, 0x24, 0x07,
    0x2a, 0x07, 0x32, 0x07, 0x38, 0x07, 0xb2, 0x07,
    0xba, 0x07, 0xde, 0x07, 0xe4, 0x07, 0x10, 0x08,
    0x14, 0x08, 0x1a, 0x08, 0x1e, 0x08, 0x30, 0x08,
    0x38, 0x08, 0x3c, 0x08, 0x44, 0x08, 0x42, 0x08,
    0x48, 0x08, 0xc6, 0x08, 0xcc, 0x08, 0xd2, 0x08,
    0xfe, 0x08, 0x04, 0x09, 0x0a, 0x09, 0x0e, 0x09,
    0x12, 0x09, 0x16, 0x09, 0x20, 0x09, 0x24, 0x09,
    0x28, 0x09, 0x32, 0x09, 0x46, 0x09, 0x4a, 0x09,
    0x50, 0x09, 0x54, 0x09, 0x5a, 0x09, 0x60, 0x09,
    0x7c, 0x09, 0x80, 0x09, 0xb8, 0x09, 0xbc, 0x09,
    0xc0, 0x09, 0xc4, 0x09, 0xc8, 0x09, 0xcc, 0x09,
    0xd0, 0x09, 0xd4, 0x09, 0xec, 0x09, 0xf4, 0x09,
    0xf6, 0x09, 0xf8, 0x09, 0xfa, 0x09, 0xfc, 0x09,
    0xfe, 0x09, 0x00, 0x0a, 0x02, 0x0a, 0x04, 0x0a,
    0x06, 0x0a, 0x08, 0x0a, 0x0a, 0x0a, 0x0c, 0x0a,
    0x10, 0x0a, 0x18, 0x0a, 0x24, 0x0a, 0x2c, 0x0a,
    0x32, 0x0a, 0x3c, 0x0a, 0x46, 0x0a, 0x4c, 0x0a,
    0x50, 0x0a, 0x54, 0x0a, 0x5a, 0x0a, 0x5e, 0x0a,
    0x66, 0x0a, 0x6c, 0x0a, 0x72, 0x0a, 0x78, 0x0a,
    0x7e, 0x0a, 0x7c, 0x0a, 0x82, 0x0a, 0x8c, 0x0a,
    0x92, 0x0a, 0x90, 0x0a, 0x98, 0x0a, 0x96, 0x0a,
    0xa2, 0x0a, 0xb2, 0x0a, 0xb6, 0x0a, 0xc4, 0x0a,
    0xe2, 0x0a, 0xe0, 0x0a, 0xe8, 0x0a, 0xee, 0x0a,
    0xf4, 0x0a, 0xf2, 0x0a, 0xf8, 0x0a, 0x0c, 0x0b,
    0x1a, 0x0b, 0x24, 0x0b, 0x40, 0x0b, 0x44, 0x0b,
    0x48, 0x0b, 0x4e, 0x0b, 0x4c, 0x0b, 0x52, 0x0b,
    0x68, 0x0b, 0x6c, 0x0b, 0x70, 0x0b, 0x76, 0x0b,
    0x88, 0x0b, 0x92, 0x0b, 0xbe, 0x0b, 0xca, 0x0b,
    0xce, 0x0b, 0xde, 0x0b, 0xf4, 0x0b, 0xfa, 0x0b,
    0x00, 0x0c, 0x24, 0x0c, 0x28, 0x0c, 0x30, 0x0c,
    0x36, 0x0c, 0x3c, 0x0c, 0x40, 0x0c, 0x4a, 0x0c,
    0x50, 0x0c, 0x58, 0x0c, 0x56, 0x0c, 0x5c, 0x0c,
    0x60, 0x0c, 0x64, 0x0c, 0x80, 0x0c, 0x94, 0x0c,
    0x9a, 0x0c, 0x98, 0x0c, 0x9e, 0x0c, 0xa4, 0x0c,
    0xa2, 0x0c, 0xa8, 0x0c, 0xac, 0x0c, 0xb0, 0x0c,
    0xb4, 0x0c, 0xb8, 0x0c, 0xbc, 0x0c, 0xce, 0x0c,
    0xd2, 0x0c, 0xd6, 0x0c, 0xf4, 0x0c, 0xfa, 0x0c,
    0x00, 0x0d, 0xfe, 0x0c, 0x06, 0x0d, 0x0e, 0x0d,
    0x0c, 0x0d, 0x16, 0x0d, 0x1c, 0x0d, 0x22, 0x0d,
    0x20, 0x0d, 0x30, 0x0d, 0x7e, 0x0d, 0x82, 0x0d,
    0x9a, 0x0d, 0xa0, 0x0d, 0xa6, 0x0d, 0xb0, 0x0d,
    0xb8, 0x0d, 0xc2, 0x0d, 0xc8, 0x0d, 0xce, 0x0d,
    0xd4, 0x0d, 0xdc, 0x0d, 0x1e, 0x0e, 0x2c, 0x0e,
    0x3e, 0x0e, 0x4c, 0x0e, 0x50, 0x0e, 0x5e, 0x0e,
    0xae, 0x0e, 0xb8, 0x0e, 0xc6, 0x0e, 0xca, 0x0e,
    0,	  0
};

/* Fixup command. */
#define KUE_TRIGCMD_OFFSET	5
static unsigned char kue_trig_seg[] = {
    0xb6, 0xc3, 0x01, 0x00, 0x06, 0x64, 0x00, 0x00
};
