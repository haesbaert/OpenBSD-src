/* apps/apps.h */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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

#ifndef HEADER_APPS_H
#define HEADER_APPS_H

#include "e_os.h"

#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/lhash.h>
#include <openssl/conf.h>
#include <openssl/txt_db.h>
#include <openssl/engine.h>
#include <openssl/ossl_typ.h>

int app_RAND_load_file(const char *file, BIO *bio_e, int dont_warn);
int app_RAND_write_file(const char *file, BIO *bio_e);
/* When `file' is NULL, use defaults.
 * `bio_e' is for error messages. */
void app_RAND_allow_write_file(void);
long app_RAND_load_files(char *file); /* `file' is a list of files to read,
                                       * separated by LIST_SEPARATOR_CHAR
                                       * (see e_os.h).  The string is
                                       * destroyed! */

#ifdef OPENSSL_NO_STDIO
BIO_METHOD *BIO_s_file();
#endif

#ifdef OPENSSL_SYS_WIN32
#define rename(from,to) WIN32_rename((from),(to))
int WIN32_rename(char *oldname,char *newname);
#endif

#ifndef MONOLITH

#define MAIN(a,v)	main(a,v)

#ifndef NON_MAIN
BIO *bio_err=NULL;
#else
extern BIO *bio_err;
#endif

#else

#define MAIN(a,v)	PROG(a,v)
extern CONF *config;
extern char *default_config_file;
extern BIO *bio_err;

#endif

#include <signal.h>

#ifdef SIGPIPE
#define do_pipe_sig()	signal(SIGPIPE,SIG_IGN)
#else
#define do_pipe_sig()
#endif

#if defined(MONOLITH) && !defined(OPENSSL_C)
#  define apps_startup() \
		do_pipe_sig()
#  define apps_shutdown()
#else
#  if defined(OPENSSL_SYS_MSDOS) || defined(OPENSSL_SYS_WIN16) || \
   defined(OPENSSL_SYS_WIN32)
#    ifdef _O_BINARY
#      define apps_startup() \
		do { _fmode=_O_BINARY; do_pipe_sig(); CRYPTO_malloc_init(); \
		ERR_load_crypto_strings(); OpenSSL_add_all_algorithms(); \
		ENGINE_load_builtin_engines(); setup_ui_method(); } while(0)
#    else
#      define apps_startup() \
		do { _fmode=O_BINARY; do_pipe_sig(); CRYPTO_malloc_init(); \
		ERR_load_crypto_strings(); OpenSSL_add_all_algorithms(); \
		ENGINE_load_builtin_engines(); setup_ui_method(); } while(0)
#    endif
#  else
#    define apps_startup() \
		do { do_pipe_sig(); OpenSSL_add_all_algorithms(); \
		ERR_load_crypto_strings(); ENGINE_load_builtin_engines(); \
		setup_ui_method(); } while(0)
#  endif
#  define apps_shutdown() \
		do { CONF_modules_unload(1); destroy_ui_method(); \
		EVP_cleanup(); ENGINE_cleanup(); \
		CRYPTO_cleanup_all_ex_data(); ERR_remove_state(0); \
		ERR_free_strings(); } while(0)
#endif

typedef struct args_st
	{
	char **data;
	int count;
	} ARGS;

#define PW_MIN_LENGTH 4
typedef struct pw_cb_data
	{
	const void *password;
	const char *prompt_info;
	} PW_CB_DATA;

int password_callback(char *buf, int bufsiz, int verify,
	PW_CB_DATA *cb_data);

int setup_ui_method();
void destroy_ui_method();

int should_retry(int i);
int args_from_file(char *file, int *argc, char **argv[]);
int str2fmt(char *s);
void program_name(char *in,char *out,int size);
int chopup_args(ARGS *arg,char *buf, int *argc, char **argv[]);
#ifdef HEADER_X509_H
int dump_cert_text(BIO *out, X509 *x);
void print_name(BIO *out, char *title, X509_NAME *nm, unsigned long lflags);
#endif
int set_cert_ex(unsigned long *flags, const char *arg);
int set_name_ex(unsigned long *flags, const char *arg);
int set_ext_copy(int *copy_type, const char *arg);
int copy_extensions(X509 *x, X509_REQ *req, int copy_type);
int app_passwd(BIO *err, char *arg1, char *arg2, char **pass1, char **pass2);
int add_oid_section(BIO *err, CONF *conf);
X509 *load_cert(BIO *err, const char *file, int format,
	const char *pass, ENGINE *e, const char *cert_descrip);
EVP_PKEY *load_key(BIO *err, const char *file, int format,
	const char *pass, ENGINE *e, const char *key_descrip);
EVP_PKEY *load_pubkey(BIO *err, const char *file, int format,
	const char *pass, ENGINE *e, const char *key_descrip);
STACK_OF(X509) *load_certs(BIO *err, const char *file, int format,
	const char *pass, ENGINE *e, const char *cert_descrip);
X509_STORE *setup_verify(BIO *bp, char *CAfile, char *CApath);
ENGINE *setup_engine(BIO *err, const char *engine, int debug);

int load_config(BIO *err, CONF *cnf);

/* Functions defined in ca.c and also used in ocsp.c */
int unpack_revinfo(ASN1_TIME **prevtm, int *preason, ASN1_OBJECT **phold,
			ASN1_GENERALIZEDTIME **pinvtm, char *str);
int make_serial_index(TXT_DB *db);

#define FORMAT_UNDEF    0
#define FORMAT_ASN1     1
#define FORMAT_TEXT     2
#define FORMAT_PEM      3
#define FORMAT_NETSCAPE 4
#define FORMAT_PKCS12   5
#define FORMAT_SMIME    6
#define FORMAT_ENGINE   7
#define FORMAT_IISSGC	8	/* XXX this stupid macro helps us to avoid
				 * adding yet another param to load_*key() */

#define EXT_COPY_NONE	0
#define EXT_COPY_ADD	1
#define EXT_COPY_ALL	2

#define NETSCAPE_CERT_HDR	"certificate"

#define APP_PASS_LEN	1024

#endif
