::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:
:  !!!! Do NOT edit this file directly! -- Edit devel/mkapidoc.sh instead. !!!!
:
:  This file was automatically generated from the API documentation scattered
:  all over the Perl source code. To learn more about how all this works,
:  please read the F<HACKERS> file that came with this distribution.
:
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

:
: This file lists all API functions/macros that are documented in the Perl
: source code, but are not contained in F<embed.fnc>.
:

Ama|char*|savepvs|const char* s
Ama|char*|savesharedpvs|const char* s
Ama|SV*|newSVpvs|const char* s
Ama|SV*|newSVpvs_flags|const char* s|U32 flags
Ama|SV*|newSVpvs_share|const char* s
Am|bool|isALPHA|char ch
Am|bool|isASCII|char ch
Am|bool|isDIGIT|char ch
Am|bool|isLOWER|char ch
Am|bool|isOCTAL|char ch
Am|bool|isSPACE|char ch
Am|bool|isUPPER|char ch
Am|bool|isWORDCHAR|char ch
Am|bool|isXDIGIT|char ch
Am|bool|strEQ|char* s1|char* s2
Am|bool|strGE|char* s1|char* s2
Am|bool|strGT|char* s1|char* s2
Am|bool|strLE|char* s1|char* s2
Am|bool|strLT|char* s1|char* s2
Am|bool|strNE|char* s1|char* s2
Am|bool|strnEQ|char* s1|char* s2|STRLEN len
Am|bool|strnNE|char* s1|char* s2|STRLEN len
Am|bool|SvIOK_notUV|SV* sv
Am|bool|SvIOK_UV|SV* sv
Am|bool|SvIsCOW_shared_hash|SV* sv
Am|bool|SvIsCOW|SV* sv
Am|bool|SvRXOK|SV* sv
Am|bool|SvTAINTED|SV* sv
Am|bool|SvTRUE_nomg|SV* sv
Am|bool|SvTRUE|SV* sv
Am|bool|SvUOK|SV* sv
Am|bool|SvVOK|SV* sv
Am|char*|HePV|HE* he|STRLEN len
Am|char*|HeUTF8|HE* he
Am|char*|HvENAME|HV* stash
Am|char*|HvNAME|HV* stash
Am|char*|SvEND|SV* sv
Am|char *|SvGROW|SV* sv|STRLEN len
Am|char*|SvPVbyte_force|SV* sv|STRLEN len
Am|char*|SvPVbyte_nolen|SV* sv
Am|char*|SvPVbyte|SV* sv|STRLEN len
Am|char*|SvPVbytex_force|SV* sv|STRLEN len
Am|char*|SvPVbytex|SV* sv|STRLEN len
Am|char*|SvPV_force_nomg|SV* sv|STRLEN len
Am|char*|SvPV_force|SV* sv|STRLEN len
Am|char*|SvPV_nolen|SV* sv
Am|char*|SvPV_nomg_nolen|SV* sv
Am|char*|SvPV_nomg|SV* sv|STRLEN len
Am|char*|SvPV|SV* sv|STRLEN len
Am|char*|SvPVutf8_force|SV* sv|STRLEN len
Am|char*|SvPVutf8_nolen|SV* sv
Am|char*|SvPVutf8|SV* sv|STRLEN len
Am|char*|SvPVutf8x_force|SV* sv|STRLEN len
Am|char*|SvPVutf8x|SV* sv|STRLEN len
Am|char*|SvPVX|SV* sv
Am|char*|SvPVx|SV* sv|STRLEN len
Am|char|toLOWER|char ch
Am|char|toUPPER|char ch
Am|const char *|OP_DESC|OP *o
Am|const char *|OP_NAME|OP *o
Am|HV *|cop_hints_2hv|const COP *cop|U32 flags
Am|HV*|CvSTASH|CV* cv
Am|HV*|gv_stashpvs|const char* name|I32 create
Am|HV*|SvSTASH|SV* sv
Am|int|AvFILL|AV* av
Am|IV|SvIV_nomg|SV* sv
Am|IV|SvIV|SV* sv
Am|IV|SvIVx|SV* sv
Am|IV|SvIVX|SV* sv
Amn|char*|CLASS
Amn|char*|POPp
Amn|char*|POPpbytex
Amn|char*|POPpx
Amn|HV*|PL_modglobal
Amn|I32|ax
Amn|I32|items
Amn|I32|ix
Amn|IV|POPi
Amn|long|POPl
Amn|NV|POPn
Amn|peep_t|PL_peepp
Amn|peep_t|PL_rpeepp
Amn|Perl_ophook_t|PL_opfreehook
Amn|STRLEN|PL_na
Amn|SV|PL_sv_no
Amn|SV|PL_sv_undef
Amn|SV|PL_sv_yes
Amn|SV*|POPs
Amn|U32|GIMME
Amn|U32|GIMME_V
Am|NV|SvNV_nomg|SV* sv
Am|NV|SvNV|SV* sv
Am|NV|SvNVx|SV* sv
Am|NV|SvNVX|SV* sv
Amn|(whatever)|RETVAL
Amn|(whatever)|THIS
Am|OP*|LINKLIST|OP *o
Am|REGEXP *|SvRX|SV *sv
Ams||dAX
Ams||dAXMARK
Ams||dITEMS
Ams||dMARK
Ams||dMULTICALL
Ams||dORIGMARK
Ams||dSP
Ams||dUNDERBAR
Ams||dXCPT
Ams||dXSARGS
Ams||dXSI32
Ams||ENTER
Ams||FREETMPS
Ams||LEAVE
Ams||MULTICALL
Ams||POP_MULTICALL
Ams||PUSH_MULTICALL
Ams||PUTBACK
Ams||SAVETMPS
Ams||SPAGAIN
Am|STRLEN|HeKLEN|HE* he
Am|STRLEN|SvCUR|SV* sv
Am|STRLEN|SvLEN|SV* sv
Am|SV *|cop_hints_fetch_pv|const COP *cop|const char *key|U32 hash|U32 flags
Am|SV *|cop_hints_fetch_pvn|const COP *cop|const char *keypv|STRLEN keylen|U32 hash|U32 flags
Am|SV *|cop_hints_fetch_pvs|const COP *cop|const char *key|U32 flags
Am|SV *|cop_hints_fetch_sv|const COP *cop|SV *key|U32 hash|U32 flags
Am|SV*|GvSV|GV* gv
Am|SV*|HeSVKEY_force|HE* he
Am|SV*|HeSVKEY|HE* he
Am|SV*|HeSVKEY_set|HE* he|SV* sv
Am|SV*|HeVAL|HE* he
Am|SV**|hv_fetchs|HV* tb|const char* key|I32 lval
Am|SV**|hv_stores|HV* tb|const char* key|NULLOK SV* val
Am|SV*|newRV_inc|SV* sv
Am|SV*|newSVpvn_utf8|NULLOK const char* s|STRLEN len|U32 utf8
Am|SV*|ST|int ix
Am|SV*|SvREFCNT_inc_NN|SV* sv
Am|SV*|SvREFCNT_inc_simple_NN|SV* sv
Am|SV*|SvREFCNT_inc_simple|SV* sv
Am|SV*|SvREFCNT_inc|SV* sv
Am|SV*|SvRV|SV* sv
Am|SV *|sv_setref_pvs|SV *rv|const char* classname|const char* s
Am|svtype|SvTYPE|SV* sv
Ams||XCPT_RETHROW
Ams||XS_APIVERSION_BOOTCHECK
Ams||XSRETURN_EMPTY
Ams||XSRETURN_NO
Ams||XSRETURN_UNDEF
Ams||XSRETURN_YES
Ams||XS_VERSION_BOOTCHECK
Am|U32|HeHASH|HE* he
Am|U32|OP_CLASS|OP *o
Am|U32|SvGAMAGIC|SV* sv
Am|U32|SvIOKp|SV* sv
Am|U32|SvIOK|SV* sv
Am|U32|SvNIOKp|SV* sv
Am|U32|SvNIOK|SV* sv
Am|U32|SvNOKp|SV* sv
Am|U32|SvNOK|SV* sv
Am|U32|SvOK|SV* sv
Am|U32|SvOOK|SV* sv
Am|U32|SvPOKp|SV* sv
Am|U32|SvPOK|SV* sv
Am|U32|SvREFCNT|SV* sv
Am|U32|SvROK|SV* sv
Am|U32|SvUTF8|SV* sv
Am|U32|XopFLAGS|XOP *xop
AmU||G_ARRAY
AmU||G_DISCARD
AmU||G_EVAL
AmU||G_NOARGS
AmU||G_SCALAR
AmU||G_VOID
AmU||HEf_SVKEY
AmU||MARK
AmU||newXSproto|char* name|XSUBADDR_t f|char* filename|const char *proto
AmU||Nullav
AmU||Nullch
AmU||Nullcv
AmU||Nullhv
AmU||Nullsv
AmU||ORIGMARK
AmU||SP
AmU||SVt_IV
AmU||SVt_NV
AmU||SVt_PV
AmU||SVt_PVAV
AmU||SVt_PVCV
AmU||SVt_PVHV
AmU||SVt_PVMG
AmU||svtype
AmU||UNDERBAR
Am|UV|SvUV_nomg|SV* sv
Am|UV|SvUV|SV* sv
Am|UV|SvUVx|SV* sv
Am|UV|SvUVX|SV* sv
AmU||XCPT_CATCH
AmU||XCPT_TRY_END
AmU||XCPT_TRY_START
AmUx|Perl_keyword_plugin_t|PL_keyword_plugin
AmU||XS
AmU||XS_VERSION
AmU|yy_parser *|PL_parser
Am|void *|CopyD|void* src|void* dest|int nitems|type
Am|void|Copy|void* src|void* dest|int nitems|type
Am|void|EXTEND|SP|int nitems
Am|void*|HeKEY|HE* he
Am|void *|MoveD|void* src|void* dest|int nitems|type
Am|void|Move|void* src|void* dest|int nitems|type
Am|void|mPUSHi|IV iv
Am|void|mPUSHn|NV nv
Am|void|mPUSHp|char* str|STRLEN len
Am|void|mPUSHs|SV* sv
Am|void|mPUSHu|UV uv
Am|void|mXPUSHi|IV iv
Am|void|mXPUSHn|NV nv
Am|void|mXPUSHp|char* str|STRLEN len
Am|void|mXPUSHs|SV* sv
Am|void|mXPUSHu|UV uv
Am|void|Newxc|void* ptr|int nitems|type|cast
Am|void|Newx|void* ptr|int nitems|type
Am|void|Newxz|void* ptr|int nitems|type
Am|void|PERL_SYS_INIT3|int argc|char** argv|char** env
Am|void|PERL_SYS_INIT|int argc|char** argv
Am|void|PERL_SYS_TERM|
Am|void|PoisonFree|void* dest|int nitems|type
Am|void|PoisonNew|void* dest|int nitems|type
Am|void|Poison|void* dest|int nitems|type
Am|void|PoisonWith|void* dest|int nitems|type|U8 byte
Am|void|PUSHi|IV iv
Am|void|PUSHMARK|SP
Am|void|PUSHmortal
Am|void|PUSHn|NV nv
Am|void|PUSHp|char* str|STRLEN len
Am|void|PUSHs|SV* sv
Am|void|PUSHu|UV uv
Am|void|Renewc|void* ptr|int nitems|type|cast
Am|void|Renew|void* ptr|int nitems|type
Am|void|Safefree|void* ptr
Am|void|StructCopy|type src|type dest|type
Am|void|sv_catpvn_nomg|SV* sv|const char* ptr|STRLEN len
Am|void|sv_catpv_nomg|SV* sv|const char* ptr
Am|void|sv_catpvs_flags|SV* sv|const char* s|I32 flags
Am|void|sv_catpvs_mg|SV* sv|const char* s
Am|void|sv_catpvs_nomg|SV* sv|const char* s
Am|void|sv_catpvs|SV* sv|const char* s
Am|void|sv_catsv_nomg|SV* dsv|SV* ssv
Am|void|SvCUR_set|SV* sv|STRLEN len
Am|void|SvGETMAGIC|SV* sv
Am|void|SvIOK_off|SV* sv
Am|void|SvIOK_only|SV* sv
Am|void|SvIOK_only_UV|SV* sv
Am|void|SvIOK_on|SV* sv
Am|void|SvIV_set|SV* sv|IV val
Am|void|SvLEN_set|SV* sv|STRLEN len
Am|void|SvLOCK|SV* sv
Am|void|SvMAGIC_set|SV* sv|MAGIC* val
Am|void|SvNIOK_off|SV* sv
Am|void|SvNOK_off|SV* sv
Am|void|SvNOK_only|SV* sv
Am|void|SvNOK_on|SV* sv
Am|void|SvNV_set|SV* sv|NV val
Am|void|SvOOK_offset|NN SV*sv|STRLEN len
Am|void|SvPOK_off|SV* sv
Am|void|SvPOK_only|SV* sv
Am|void|SvPOK_only_UTF8|SV* sv
Am|void|SvPOK_on|SV* sv
Am|void|SvPV_set|SV* sv|char* val
Am|void|SvREFCNT_dec|SV* sv
Am|void|SvREFCNT_inc_simple_void_NN|SV* sv
Am|void|SvREFCNT_inc_simple_void|SV* sv
Am|void|SvREFCNT_inc_void_NN|SV* sv
Am|void|SvREFCNT_inc_void|SV* sv
Am|void|SvROK_off|SV* sv
Am|void|SvROK_on|SV* sv
Am|void|SvRV_set|SV* sv|SV* val
Am|void|SvSetMagicSV_nosteal|SV* dsv|SV* ssv
Am|void|SvSETMAGIC|SV* sv
Am|void|SvSetMagicSV|SV* dsb|SV* ssv
Am|void|sv_setpvs_mg|SV* sv|const char* s
Am|void|sv_setpvs|SV* sv|const char* s
Am|void|sv_setsv_nomg|SV* dsv|SV* ssv
Am|void|SvSetSV_nosteal|SV* dsv|SV* ssv
Am|void|SvSetSV|SV* dsb|SV* ssv
Am|void|SvSHARE|SV* sv
Am|void|SvSTASH_set|SV* sv|HV* val
Am|void|SvTAINTED_off|SV* sv
Am|void|SvTAINTED_on|SV* sv
Am|void|SvTAINT|SV* sv
Am|void|SvUNLOCK|SV* sv
Am|void|SvUPGRADE|SV* sv|svtype type
Am|void|SvUTF8_off|SV *sv
Am|void|SvUTF8_on|SV *sv
Am|void|SvUV_set|SV* sv|UV val
Am|void|XopDISABLE|XOP *xop|which
Am|void|XopENABLE|XOP *xop|which
Am|void|XopENTRY_set|XOP *xop|which|value
Am|void|XPUSHi|IV iv
Am|void|XPUSHmortal
Am|void|XPUSHn|NV nv
Am|void|XPUSHp|char* str|STRLEN len
Am|void|XPUSHs|SV* sv
Am|void|XPUSHu|UV uv
Am|void|XSRETURN|int nitems
Am|void|XSRETURN_IV|IV iv
Am|void|XSRETURN_NV|NV nv
Am|void|XSRETURN_PV|char* str
Am|void|XSRETURN_UV|IV uv
Am|void|XST_mIV|int pos|IV iv
Am|void|XST_mNO|int pos
Am|void|XST_mNV|int pos|NV nv
Am|void|XST_mPV|int pos|char* str
Am|void|XST_mUNDEF|int pos
Am|void|XST_mYES|int pos
Am|void *|ZeroD|void* dest|int nitems|type
Am|void|Zero|void* dest|int nitems|type
Amx|COPHH *|cophh_copy|COPHH *cophh
Amx|COPHH *|cophh_delete_pv|const COPHH *cophh|const char *key|U32 hash|U32 flags
Amx|COPHH *|cophh_delete_pvn|COPHH *cophh|const char *keypv|STRLEN keylen|U32 hash|U32 flags
Amx|COPHH *|cophh_delete_pvs|const COPHH *cophh|const char *key|U32 flags
Amx|COPHH *|cophh_delete_sv|const COPHH *cophh|SV *key|U32 hash|U32 flags
Amx|COPHH *|cophh_new_empty
Amx|COPHH *|cophh_store_pv|const COPHH *cophh|const char *key|U32 hash|SV *value|U32 flags
Amx|COPHH *|cophh_store_pvn|COPHH *cophh|const char *keypv|STRLEN keylen|U32 hash|SV *value|U32 flags
Amx|COPHH *|cophh_store_pvs|const COPHH *cophh|const char *key|SV *value|U32 flags
Amx|COPHH *|cophh_store_sv|const COPHH *cophh|SV *key|U32 hash|SV *value|U32 flags
Amx|HV *|cophh_2hv|const COPHH *cophh|U32 flags
Am||XopENTRY|XOP *xop|which
Amx|SV *|cophh_fetch_pv|const COPHH *cophh|const char *key|U32 hash|U32 flags
Amx|SV *|cophh_fetch_pvn|const COPHH *cophh|const char *keypv|STRLEN keylen|U32 hash|U32 flags
Amx|SV *|cophh_fetch_pvs|const COPHH *cophh|const char *key|U32 flags
Amx|SV *|cophh_fetch_sv|const COPHH *cophh|SV *key|U32 hash|U32 flags
AmxU|char *|PL_parser-E<gt>bufend
AmxU|char *|PL_parser-E<gt>bufptr
AmxU|char *|PL_parser-E<gt>linestart
AmxU|SV *|PL_parser-E<gt>linestr
Amx|void|BhkDISABLE|BHK *hk|which
Amx|void|BhkENABLE|BHK *hk|which
Amx|void|BhkENTRY_set|BHK *hk|which|void *ptr
Amx|void|cophh_free|COPHH *cophh
Amx|void|lex_stuff_pvs|const char *pv|U32 flags
m|AV *|CvPADLIST|CV *cv
m|bool|CvWEAKOUTSIDE|CV *cv
m|char *|PAD_COMPNAME_PV|PADOFFSET po
m|HV *|PAD_COMPNAME_OURSTASH|PADOFFSET po
m|HV *|PAD_COMPNAME_TYPE|PADOFFSET po
mn|bool|PL_dowarn
mn|GV *|PL_DBsub
mn|GV*|PL_last_in_gv
mn|GV*|PL_ofsgv
mn|SV *|PL_DBsingle
mn|SV *|PL_DBtrace
mn|SV*|PL_rs
ms||djSP
m|STRLEN|PAD_COMPNAME_GEN|PADOFFSET po
m|STRLEN|PAD_COMPNAME_GEN_set|PADOFFSET po|int gen
m|struct refcounted_he *|refcounted_he_new_pvs|struct refcounted_he *parent|const char *key|SV *value|U32 flags
m|SV *|CX_CURPAD_SV|struct context|PADOFFSET po
m|SV *|PAD_BASE_SV	|PADLIST padlist|PADOFFSET po
m|SV *|PAD_SETSV	|PADOFFSET po|SV* sv
m|SV *|PAD_SVl	|PADOFFSET po
m|SV *|refcounted_he_fetch_pvs|const struct refcounted_he *chain|const char *key|U32 flags
m|U32|PAD_COMPNAME_FLAGS|PADOFFSET po
mU||LVRET
m|void|CX_CURPAD_SAVE|struct context
m|void|PAD_CLONE_VARS|PerlInterpreter *proto_perl|CLONE_PARAMS* param
m|void|PAD_DUP|PADLIST dstpad|PADLIST srcpad|CLONE_PARAMS* param
m|void|PAD_RESTORE_LOCAL|PAD *opad
m|void|PAD_SAVE_LOCAL|PAD *opad|PAD *npad
m|void|PAD_SAVE_SETNULLPAD
m|void|PAD_SET_CUR_NOSAVE	|PADLIST padlist|I32 n
m|void|PAD_SET_CUR	|PADLIST padlist|I32 n
m|void|PAD_SV	|PADOFFSET po
m|void|SAVECLEARSV	|SV **svp
m|void|SAVECOMPPAD
m|void|SAVEPADSV	|PADOFFSET po
mx|U32|BhkFLAGS|BHK *hk
mx|void *|BhkENTRY|BHK *hk|which
mx|void|CALL_BLOCK_HOOKS|which|arg
