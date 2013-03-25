/* -*- buffer-read-only: t -*-
 *
 *    overload.h
 *
 *    Copyright (C) 1997, 1998, 2000, 2001, 2005, 2006, 2007, 2011
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
 * This file is built by regen/overload.pl.
 * Any changes made here will be lost!
 */

enum {
    fallback_amg,	/* 0x00 fallback */
    to_sv_amg,		/* 0x01 ${}      */
    to_av_amg,		/* 0x02 @{}      */
    to_hv_amg,		/* 0x03 %{}      */
    to_gv_amg,		/* 0x04 *{}      */
    to_cv_amg,		/* 0x05 &{}      */
    inc_amg,		/* 0x06 ++       */
    dec_amg,		/* 0x07 --       */
    bool__amg,		/* 0x08 bool     */
    numer_amg,		/* 0x09 0+       */
    string_amg,		/* 0x0a ""       */
    not_amg,		/* 0x0b !        */
    copy_amg,		/* 0x0c =        */
    abs_amg,		/* 0x0d abs      */
    neg_amg,		/* 0x0e neg      */
    iter_amg,		/* 0x0f <>       */
    int_amg,		/* 0x10 int      */
    lt_amg,		/* 0x11 <        */
    le_amg,		/* 0x12 <=       */
    gt_amg,		/* 0x13 >        */
    ge_amg,		/* 0x14 >=       */
    eq_amg,		/* 0x15 ==       */
    ne_amg,		/* 0x16 !=       */
    slt_amg,		/* 0x17 lt       */
    sle_amg,		/* 0x18 le       */
    sgt_amg,		/* 0x19 gt       */
    sge_amg,		/* 0x1a ge       */
    seq_amg,		/* 0x1b eq       */
    sne_amg,		/* 0x1c ne       */
    nomethod_amg,	/* 0x1d nomethod */
    add_amg,		/* 0x1e +        */
    add_ass_amg,	/* 0x1f +=       */
    subtr_amg,		/* 0x20 -        */
    subtr_ass_amg,	/* 0x21 -=       */
    mult_amg,		/* 0x22 *        */
    mult_ass_amg,	/* 0x23 *=       */
    div_amg,		/* 0x24 /        */
    div_ass_amg,	/* 0x25 /=       */
    modulo_amg,		/* 0x26 %        */
    modulo_ass_amg,	/* 0x27 %=       */
    pow_amg,		/* 0x28 **       */
    pow_ass_amg,	/* 0x29 **=      */
    lshift_amg,		/* 0x2a <<       */
    lshift_ass_amg,	/* 0x2b <<=      */
    rshift_amg,		/* 0x2c >>       */
    rshift_ass_amg,	/* 0x2d >>=      */
    band_amg,		/* 0x2e &        */
    band_ass_amg,	/* 0x2f &=       */
    bor_amg,		/* 0x30 |        */
    bor_ass_amg,	/* 0x31 |=       */
    bxor_amg,		/* 0x32 ^        */
    bxor_ass_amg,	/* 0x33 ^=       */
    ncmp_amg,		/* 0x34 <=>      */
    scmp_amg,		/* 0x35 cmp      */
    compl_amg,		/* 0x36 ~        */
    atan2_amg,		/* 0x37 atan2    */
    cos_amg,		/* 0x38 cos      */
    sin_amg,		/* 0x39 sin      */
    exp_amg,		/* 0x3a exp      */
    log_amg,		/* 0x3b log      */
    sqrt_amg,		/* 0x3c sqrt     */
    repeat_amg,		/* 0x3d x        */
    repeat_ass_amg,	/* 0x3e x=       */
    concat_amg,		/* 0x3f .        */
    concat_ass_amg,	/* 0x40 .=       */
    smart_amg,		/* 0x41 ~~       */
    ftest_amg,		/* 0x42 -X       */
    regexp_amg,		/* 0x43 qr       */
    DESTROY_amg,	/* 0x44 DESTROY  */
    max_amg_code
    /* Do not leave a trailing comma here.  C9X allows it, C89 doesn't. */
};

#define NofAMmeth max_amg_code

/* ex: set ro: */
