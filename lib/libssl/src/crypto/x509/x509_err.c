/* crypto/x509/x509_err.c */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

/* NOTE: this file was auto generated by the mkerr.pl script: any changes
 * made to it will be overwritten when the script next updates this file.
 */

#include <stdio.h>
#include <openssl/err.h>
#include <openssl/x509.h>

/* BEGIN ERROR CODES */
#ifndef NO_ERR
static ERR_STRING_DATA X509_str_functs[]=
	{
{ERR_PACK(0,X509_F_ADD_CERT_DIR,0),	"ADD_CERT_DIR"},
{ERR_PACK(0,X509_F_BY_FILE_CTRL,0),	"BY_FILE_CTRL"},
{ERR_PACK(0,X509_F_DIR_CTRL,0),	"DIR_CTRL"},
{ERR_PACK(0,X509_F_GET_CERT_BY_SUBJECT,0),	"GET_CERT_BY_SUBJECT"},
{ERR_PACK(0,X509_F_NETSCAPE_SPKI_B64_DECODE,0),	"NETSCAPE_SPKI_b64_decode"},
{ERR_PACK(0,X509_F_NETSCAPE_SPKI_B64_ENCODE,0),	"NETSCAPE_SPKI_b64_encode"},
{ERR_PACK(0,X509_F_X509V3_ADD_EXT,0),	"X509v3_add_ext"},
{ERR_PACK(0,X509_F_X509_ADD_ATTR,0),	"X509_ADD_ATTR"},
{ERR_PACK(0,X509_F_X509_ATTRIBUTE_CREATE_BY_NID,0),	"X509_ATTRIBUTE_create_by_NID"},
{ERR_PACK(0,X509_F_X509_ATTRIBUTE_CREATE_BY_OBJ,0),	"X509_ATTRIBUTE_create_by_OBJ"},
{ERR_PACK(0,X509_F_X509_ATTRIBUTE_CREATE_BY_TXT,0),	"X509_ATTRIBUTE_create_by_txt"},
{ERR_PACK(0,X509_F_X509_ATTRIBUTE_GET0_DATA,0),	"X509_ATTRIBUTE_get0_data"},
{ERR_PACK(0,X509_F_X509_ATTRIBUTE_SET1_DATA,0),	"X509_ATTRIBUTE_set1_data"},
{ERR_PACK(0,X509_F_X509_CHECK_PRIVATE_KEY,0),	"X509_check_private_key"},
{ERR_PACK(0,X509_F_X509_EXTENSION_CREATE_BY_NID,0),	"X509_EXTENSION_create_by_NID"},
{ERR_PACK(0,X509_F_X509_EXTENSION_CREATE_BY_OBJ,0),	"X509_EXTENSION_create_by_OBJ"},
{ERR_PACK(0,X509_F_X509_GET_PUBKEY_PARAMETERS,0),	"X509_get_pubkey_parameters"},
{ERR_PACK(0,X509_F_X509_LOAD_CERT_CRL_FILE,0),	"X509_load_cert_crl_file"},
{ERR_PACK(0,X509_F_X509_LOAD_CERT_FILE,0),	"X509_load_cert_file"},
{ERR_PACK(0,X509_F_X509_LOAD_CRL_FILE,0),	"X509_load_crl_file"},
{ERR_PACK(0,X509_F_X509_NAME_ADD_ENTRY,0),	"X509_NAME_add_entry"},
{ERR_PACK(0,X509_F_X509_NAME_ENTRY_CREATE_BY_NID,0),	"X509_NAME_ENTRY_create_by_NID"},
{ERR_PACK(0,X509_F_X509_NAME_ENTRY_CREATE_BY_TXT,0),	"X509_NAME_ENTRY_create_by_txt"},
{ERR_PACK(0,X509_F_X509_NAME_ENTRY_SET_OBJECT,0),	"X509_NAME_ENTRY_set_object"},
{ERR_PACK(0,X509_F_X509_NAME_ONELINE,0),	"X509_NAME_oneline"},
{ERR_PACK(0,X509_F_X509_NAME_PRINT,0),	"X509_NAME_print"},
{ERR_PACK(0,X509_F_X509_PRINT_FP,0),	"X509_print_fp"},
{ERR_PACK(0,X509_F_X509_PUBKEY_GET,0),	"X509_PUBKEY_get"},
{ERR_PACK(0,X509_F_X509_PUBKEY_SET,0),	"X509_PUBKEY_set"},
{ERR_PACK(0,X509_F_X509_REQ_PRINT,0),	"X509_REQ_print"},
{ERR_PACK(0,X509_F_X509_REQ_PRINT_FP,0),	"X509_REQ_print_fp"},
{ERR_PACK(0,X509_F_X509_REQ_TO_X509,0),	"X509_REQ_to_X509"},
{ERR_PACK(0,X509_F_X509_STORE_ADD_CERT,0),	"X509_STORE_add_cert"},
{ERR_PACK(0,X509_F_X509_STORE_ADD_CRL,0),	"X509_STORE_add_crl"},
{ERR_PACK(0,X509_F_X509_STORE_CTX_PURPOSE_INHERIT,0),	"X509_STORE_CTX_purpose_inherit"},
{ERR_PACK(0,X509_F_X509_TO_X509_REQ,0),	"X509_to_X509_REQ"},
{ERR_PACK(0,X509_F_X509_TRUST_ADD,0),	"X509_TRUST_add"},
{ERR_PACK(0,X509_F_X509_VERIFY_CERT,0),	"X509_verify_cert"},
{0,NULL}
	};

static ERR_STRING_DATA X509_str_reasons[]=
	{
{X509_R_BAD_X509_FILETYPE                ,"bad x509 filetype"},
{X509_R_BASE64_DECODE_ERROR              ,"base64 decode error"},
{X509_R_CANT_CHECK_DH_KEY                ,"cant check dh key"},
{X509_R_CERT_ALREADY_IN_HASH_TABLE       ,"cert already in hash table"},
{X509_R_ERR_ASN1_LIB                     ,"err asn1 lib"},
{X509_R_INVALID_DIRECTORY                ,"invalid directory"},
{X509_R_INVALID_FIELD_NAME               ,"invalid field name"},
{X509_R_KEY_TYPE_MISMATCH                ,"key type mismatch"},
{X509_R_KEY_VALUES_MISMATCH              ,"key values mismatch"},
{X509_R_LOADING_CERT_DIR                 ,"loading cert dir"},
{X509_R_LOADING_DEFAULTS                 ,"loading defaults"},
{X509_R_NO_CERT_SET_FOR_US_TO_VERIFY     ,"no cert set for us to verify"},
{X509_R_SHOULD_RETRY                     ,"should retry"},
{X509_R_UNABLE_TO_FIND_PARAMETERS_IN_CHAIN,"unable to find parameters in chain"},
{X509_R_UNABLE_TO_GET_CERTS_PUBLIC_KEY   ,"unable to get certs public key"},
{X509_R_UNKNOWN_KEY_TYPE                 ,"unknown key type"},
{X509_R_UNKNOWN_NID                      ,"unknown nid"},
{X509_R_UNKNOWN_PURPOSE_ID               ,"unknown purpose id"},
{X509_R_UNKNOWN_TRUST_ID                 ,"unknown trust id"},
{X509_R_UNSUPPORTED_ALGORITHM            ,"unsupported algorithm"},
{X509_R_WRONG_LOOKUP_TYPE                ,"wrong lookup type"},
{X509_R_WRONG_TYPE                       ,"wrong type"},
{0,NULL}
	};

#endif

void ERR_load_X509_strings(void)
	{
	static int init=1;

	if (init)
		{
		init=0;
#ifndef NO_ERR
		ERR_load_strings(ERR_LIB_X509,X509_str_functs);
		ERR_load_strings(ERR_LIB_X509,X509_str_reasons);
#endif

		}
	}
