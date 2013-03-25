/* -*- buffer-read-only: t -*-
   !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
   This file is built by regen/feature.pl.
   Any changes made here will be lost!
 */


#if defined(PERL_CORE) || defined (PERL_EXT)

#define HINT_FEATURE_SHIFT	26

#define FEATURE_BUNDLE_DEFAULT	0
#define FEATURE_BUNDLE_510	1
#define FEATURE_BUNDLE_511	2
#define FEATURE_BUNDLE_515	3
#define FEATURE_BUNDLE_CUSTOM	(HINT_FEATURE_MASK >> HINT_FEATURE_SHIFT)

#define CURRENT_HINTS \
    (PL_curcop == &PL_compiling ? PL_hints : PL_curcop->cop_hints)
#define CURRENT_FEATURE_BUNDLE \
    ((CURRENT_HINTS & HINT_FEATURE_MASK) >> HINT_FEATURE_SHIFT)

/* Avoid using ... && Perl_feature_is_enabled(...) as that triggers a bug in
   the HP-UX cc on PA-RISC */
#define FEATURE_IS_ENABLED(name)				        \
	((CURRENT_HINTS							 \
	   & HINT_LOCALIZE_HH)						  \
	    ? Perl_feature_is_enabled(aTHX_ STR_WITH_LEN(name)) : FALSE)
/* The longest string we pass in.  */
#define MAX_FEATURE_LEN (sizeof("evalbytes")-1)

#define FEATURE_FC_IS_ENABLED \
    ( \
	CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_515 \
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \
	 FEATURE_IS_ENABLED("fc")) \
    )

#define FEATURE_SAY_IS_ENABLED \
    ( \
	(CURRENT_FEATURE_BUNDLE >= FEATURE_BUNDLE_510 && \
	 CURRENT_FEATURE_BUNDLE <= FEATURE_BUNDLE_515) \
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \
	 FEATURE_IS_ENABLED("say")) \
    )

#define FEATURE_STATE_IS_ENABLED \
    ( \
	(CURRENT_FEATURE_BUNDLE >= FEATURE_BUNDLE_510 && \
	 CURRENT_FEATURE_BUNDLE <= FEATURE_BUNDLE_515) \
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \
	 FEATURE_IS_ENABLED("state")) \
    )

#define FEATURE_SWITCH_IS_ENABLED \
    ( \
	(CURRENT_FEATURE_BUNDLE >= FEATURE_BUNDLE_510 && \
	 CURRENT_FEATURE_BUNDLE <= FEATURE_BUNDLE_515) \
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \
	 FEATURE_IS_ENABLED("switch")) \
    )

#define FEATURE_EVALBYTES_IS_ENABLED \
    ( \
	CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_515 \
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \
	 FEATURE_IS_ENABLED("evalbytes")) \
    )

#define FEATURE_ARYBASE_IS_ENABLED \
    ( \
	CURRENT_FEATURE_BUNDLE <= FEATURE_BUNDLE_511 \
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \
	 FEATURE_IS_ENABLED("arybase")) \
    )

#define FEATURE___SUB___IS_ENABLED \
    ( \
	CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_515 \
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \
	 FEATURE_IS_ENABLED("__SUB__")) \
    )

#define FEATURE_UNIEVAL_IS_ENABLED \
    ( \
	CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_515 \
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \
	 FEATURE_IS_ENABLED("unieval")) \
    )

#define FEATURE_UNICODE_IS_ENABLED \
    ( \
	(CURRENT_FEATURE_BUNDLE >= FEATURE_BUNDLE_511 && \
	 CURRENT_FEATURE_BUNDLE <= FEATURE_BUNDLE_515) \
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \
	 FEATURE_IS_ENABLED("unicode")) \
    )


#endif /* PERL_CORE or PERL_EXT */

#ifdef PERL_IN_OP_C
PERL_STATIC_INLINE void
S_enable_feature_bundle(pTHX_ SV *ver)
{
    SV *comp_ver = sv_newmortal();
    PL_hints = (PL_hints &~ HINT_FEATURE_MASK)
	     | (
		  (sv_setnv(comp_ver, 5.015),
		   vcmp(ver, upg_version(comp_ver, FALSE)) >= 0)
			? FEATURE_BUNDLE_515 :
		  (sv_setnv(comp_ver, 5.011),
		   vcmp(ver, upg_version(comp_ver, FALSE)) >= 0)
			? FEATURE_BUNDLE_511 :
		  (sv_setnv(comp_ver, 5.009005),
		   vcmp(ver, upg_version(comp_ver, FALSE)) >= 0)
			? FEATURE_BUNDLE_510 :
			  FEATURE_BUNDLE_DEFAULT
	       ) << HINT_FEATURE_SHIFT;
    /* special case */
    assert(PL_curcop == &PL_compiling);
    if (FEATURE_UNICODE_IS_ENABLED) PL_hints |=  HINT_UNI_8_BIT;
    else			    PL_hints &= ~HINT_UNI_8_BIT;
}
#endif /* PERL_IN_OP_C */

/* ex: set ro: */
