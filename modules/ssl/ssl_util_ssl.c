/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*                      _             _
 *  _ __ ___   ___   __| |    ___ ___| |  mod_ssl
 * | '_ ` _ \ / _ \ / _` |   / __/ __| |  Apache Interface to OpenSSL
 * | | | | | | (_) | (_| |   \__ \__ \ |
 * |_| |_| |_|\___/ \__,_|___|___/___/_|
 *                      |_____|
 *  ssl_util_ssl.c
 *  Additional Utility Functions for OpenSSL
 */

#include "ssl_private.h"

/*  _________________________________________________________________
**
**  Additional High-Level Functions for OpenSSL
**  _________________________________________________________________
*/

/* we initialize this index at startup time
 * and never write to it at request time,
 * so this static is thread safe.
 * also note that OpenSSL increments at static variable when
 * SSL_get_ex_new_index() is called, so we _must_ do this at startup.
 */
static int SSL_app_data2_idx = -1;

void SSL_init_app_data2_idx(void)
{
    int i;

    if (SSL_app_data2_idx > -1) {
        return;
    }

    /* we _do_ need to call this twice */
    for (i=0; i<=1; i++) {
        SSL_app_data2_idx =
            SSL_get_ex_new_index(0,
                                 "Second Application Data for SSL",
                                 NULL, NULL, NULL);
    }
}

void *SSL_get_app_data2(SSL *ssl)
{
    return (void *)SSL_get_ex_data(ssl, SSL_app_data2_idx);
}

void SSL_set_app_data2(SSL *ssl, void *arg)
{
    SSL_set_ex_data(ssl, SSL_app_data2_idx, (char *)arg);
    return;
}

/*  _________________________________________________________________
**
**  High-Level Certificate / Private Key Loading
**  _________________________________________________________________
*/

X509 *SSL_read_X509(char* filename, X509 **x509, pem_password_cb *cb)
{
    X509 *rc;
    BIO *bioS;
    BIO *bioF;

    /* 1. try PEM (= DER+Base64+headers) */
    if ((bioS=BIO_new_file(filename, "r")) == NULL)
        return NULL;
    rc = PEM_read_bio_X509 (bioS, x509, cb, NULL);
    BIO_free(bioS);

    if (rc == NULL) {
        /* 2. try DER+Base64 */
        if ((bioS=BIO_new_file(filename, "r")) == NULL)
            return NULL;

        if ((bioF = BIO_new(BIO_f_base64())) == NULL) {
            BIO_free(bioS);
            return NULL;
        }
        bioS = BIO_push(bioF, bioS);
        rc = d2i_X509_bio(bioS, NULL);
        BIO_free_all(bioS);

        if (rc == NULL) {
            /* 3. try plain DER */
            if ((bioS=BIO_new_file(filename, "r")) == NULL)
                return NULL;
            rc = d2i_X509_bio(bioS, NULL);
            BIO_free(bioS);
        }
    }
    if (rc != NULL && x509 != NULL) {
        if (*x509 != NULL)
            X509_free(*x509);
        *x509 = rc;
    }
    return rc;
}

#if SSL_LIBRARY_VERSION <= 0x00904100
static EVP_PKEY *d2i_PrivateKey_bio(BIO *bio, EVP_PKEY **key)
{
     return ((EVP_PKEY *)ASN1_d2i_bio(
             (char *(*)())EVP_PKEY_new,
             (char *(*)())d2i_PrivateKey,
             (bio), (unsigned char **)(key)));
}
#endif

EVP_PKEY *SSL_read_PrivateKey(char* filename, EVP_PKEY **key, pem_password_cb *cb, void *s)
{
    EVP_PKEY *rc;
    BIO *bioS;
    BIO *bioF;

    /* 1. try PEM (= DER+Base64+headers) */
    if ((bioS=BIO_new_file(filename, "r")) == NULL)
        return NULL;
    rc = PEM_read_bio_PrivateKey(bioS, key, cb, s);
    BIO_free(bioS);

    if (rc == NULL) {
        /* 2. try DER+Base64 */
        if ((bioS = BIO_new_file(filename, "r")) == NULL)
            return NULL;

        if ((bioF = BIO_new(BIO_f_base64())) == NULL) {
            BIO_free(bioS);
            return NULL;
        }
        bioS = BIO_push(bioF, bioS);
        rc = d2i_PrivateKey_bio(bioS, NULL);
        BIO_free_all(bioS);

        if (rc == NULL) {
            /* 3. try plain DER */
            if ((bioS = BIO_new_file(filename, "r")) == NULL)
                return NULL;
            rc = d2i_PrivateKey_bio(bioS, NULL);
            BIO_free(bioS);
        }
    }
    if (rc != NULL && key != NULL) {
        if (*key != NULL)
            EVP_PKEY_free(*key);
        *key = rc;
    }
    return rc;
}

/*  _________________________________________________________________
**
**  Smart shutdown
**  _________________________________________________________________
*/

int SSL_smart_shutdown(SSL *ssl)
{
    int i;
    int rc;

    /*
     * Repeat the calls, because SSL_shutdown internally dispatches through a
     * little state machine. Usually only one or two interation should be
     * needed, so we restrict the total number of restrictions in order to
     * avoid process hangs in case the client played bad with the socket
     * connection and OpenSSL cannot recognize it.
     */
    rc = 0;
    for (i = 0; i < 4 /* max 2x pending + 2x data = 4 */; i++) {
        if ((rc = SSL_shutdown(ssl)))
            break;
    }
    return rc;
}

/*  _________________________________________________________________
**
**  Certificate Revocation List (CRL) Storage
**  _________________________________________________________________
*/

X509_STORE *SSL_X509_STORE_create(char *cpFile, char *cpPath)
{
    X509_STORE *pStore;
    X509_LOOKUP *pLookup;
    int rv = 1;

    ERR_clear_error();

    if (cpFile == NULL && cpPath == NULL)
        return NULL;
    if ((pStore = X509_STORE_new()) == NULL)
        return NULL;
    if (cpFile != NULL) {
        pLookup = X509_STORE_add_lookup(pStore, X509_LOOKUP_file());
        if (pLookup == NULL) {
            X509_STORE_free(pStore);
            return NULL;
        }
        rv = X509_LOOKUP_load_file(pLookup, cpFile, X509_FILETYPE_PEM);
    }
    if (cpPath != NULL && rv == 1) {
        pLookup = X509_STORE_add_lookup(pStore, X509_LOOKUP_hash_dir());
        if (pLookup == NULL) {
            X509_STORE_free(pStore);
            return NULL;
        }
        rv = X509_LOOKUP_add_dir(pLookup, cpPath, X509_FILETYPE_PEM);
    }
    return rv == 1 ? pStore : NULL;
}

int SSL_X509_STORE_lookup(X509_STORE *pStore, int nType,
                          X509_NAME *pName, X509_OBJECT *pObj)
{
    X509_STORE_CTX pStoreCtx;
    int rc;

    X509_STORE_CTX_init(&pStoreCtx, pStore, NULL, NULL);
    rc = X509_STORE_get_by_subject(&pStoreCtx, nType, pName, pObj);
    X509_STORE_CTX_cleanup(&pStoreCtx);
    return rc;
}

/*  _________________________________________________________________
**
**  Cipher Suite Spec String Creation
**  _________________________________________________________________
*/

char *SSL_make_ciphersuite(apr_pool_t *p, SSL *ssl)
{
    STACK_OF(SSL_CIPHER) *sk;
    SSL_CIPHER *c;
    int i;
    int l;
    char *cpCipherSuite;
    char *cp;

    if (ssl == NULL)
        return "";
    if ((sk = (STACK_OF(SSL_CIPHER) *)SSL_get_ciphers(ssl)) == NULL)
        return "";
    l = 0;
    for (i = 0; i < sk_SSL_CIPHER_num(sk); i++) {
        c = sk_SSL_CIPHER_value(sk, i);
        l += strlen(SSL_CIPHER_get_name(c))+2+1;
    }
    if (l == 0)
        return "";
    cpCipherSuite = (char *)apr_palloc(p, l+1);
    cp = cpCipherSuite;
    for (i = 0; i < sk_SSL_CIPHER_num(sk); i++) {
        c = sk_SSL_CIPHER_value(sk, i);
        l = strlen(SSL_CIPHER_get_name(c));
        memcpy(cp, SSL_CIPHER_get_name(c), l);
        cp += l;
        *cp++ = '/';
        *cp++ = (c->valid == 1 ? '1' : '0');
        *cp++ = ':';
    }
    *(cp-1) = NUL;
    return cpCipherSuite;
}

/*  _________________________________________________________________
**
**  Certificate Checks
**  _________________________________________________________________
*/

/* check whether cert contains extended key usage with a SGC tag */
BOOL SSL_X509_isSGC(X509 *cert)
{
#ifdef HAVE_SSL_X509V3_EXT_d2i
    int ext_nid;
    EXTENDED_KEY_USAGE *sk;
    BOOL is_sgc;
    int i;

    is_sgc = FALSE;
    sk = X509_get_ext_d2i(cert, NID_ext_key_usage, NULL, NULL);
    if (sk) {
        for (i = 0; i < sk_ASN1_OBJECT_num(sk); i++) {
            ext_nid = OBJ_obj2nid(sk_ASN1_OBJECT_value(sk, i));
            if (ext_nid == NID_ms_sgc || ext_nid == NID_ns_sgc) {
                is_sgc = TRUE;
                break;
            }
        }
    EXTENDED_KEY_USAGE_free(sk);
    }
    return is_sgc;
#else
    return FALSE;
#endif
}

/* retrieve basic constraints ingredients */
BOOL SSL_X509_getBC(X509 *cert, int *ca, int *pathlen)
{
#ifdef HAVE_SSL_X509V3_EXT_d2i
    BASIC_CONSTRAINTS *bc;
    BIGNUM *bn = NULL;
    char *cp;

    bc = X509_get_ext_d2i(cert, NID_basic_constraints, NULL, NULL);
    if (bc == NULL)
        return FALSE;
    *ca = bc->ca;
    *pathlen = -1 /* unlimited */;
    if (bc->pathlen != NULL) {
        if ((bn = ASN1_INTEGER_to_BN(bc->pathlen, NULL)) == NULL)
            return FALSE;
        if ((cp = BN_bn2dec(bn)) == NULL)
            return FALSE;
        *pathlen = atoi(cp);
        free(cp);
        BN_free(bn);
    }
    BASIC_CONSTRAINTS_free(bc);
    return TRUE;
#else
    return FALSE;
#endif
}

/* convert a NAME_ENTRY to UTF8 string */
char *SSL_X509_NAME_ENTRY_to_string(apr_pool_t *p, X509_NAME_ENTRY *xsne)
{
    char *result = NULL;
    BIO* bio;
    int len;

    if ((bio = BIO_new(BIO_s_mem())) == NULL)
        return NULL;
    ASN1_STRING_print_ex(bio, X509_NAME_ENTRY_get_data(xsne),
                         ASN1_STRFLGS_ESC_CTRL|ASN1_STRFLGS_UTF8_CONVERT);
    len = BIO_pending(bio);
    result = apr_palloc(p, len+1);
    len = BIO_read(bio, result, len);
    result[len] = NUL;
    BIO_free(bio);
    ap_xlate_proto_from_ascii(result, len);
    return result;
}

/* retrieve subject CommonName of certificate */
BOOL SSL_X509_getCN(apr_pool_t *p, X509 *xs, char **cppCN)
{
    X509_NAME *xsn;
    X509_NAME_ENTRY *xsne;
    int i, nid;

    xsn = X509_get_subject_name(xs);
    for (i = 0; i < sk_X509_NAME_ENTRY_num((STACK_OF(X509_NAME_ENTRY) *)
                                           xsn->entries); i++) {
        xsne = sk_X509_NAME_ENTRY_value((STACK_OF(X509_NAME_ENTRY) *)
                                        xsn->entries, i);
        nid = OBJ_obj2nid((ASN1_OBJECT *)X509_NAME_ENTRY_get_object(xsne));
        if (nid == NID_commonName) {
            *cppCN = SSL_X509_NAME_ENTRY_to_string(p, xsne);
            return TRUE;
        }
    }
    return FALSE;
}

/*  _________________________________________________________________
**
**  Low-Level CA Certificate Loading
**  _________________________________________________________________
*/

BOOL SSL_X509_INFO_load_file(apr_pool_t *ptemp,
                             STACK_OF(X509_INFO) *sk,
                             const char *filename)
{
    BIO *in;

    if (!(in = BIO_new(BIO_s_file()))) {
        return FALSE;
    }

    if (BIO_read_filename(in, filename) <= 0) {
        BIO_free(in);
        return FALSE;
    }

    ERR_clear_error();

    PEM_X509_INFO_read_bio(in, sk, NULL, NULL);

    BIO_free(in);

    return TRUE;
}

BOOL SSL_X509_INFO_load_path(apr_pool_t *ptemp,
                             STACK_OF(X509_INFO) *sk,
                             const char *pathname)
{
    /* XXX: this dir read code is exactly the same as that in
     * ssl_engine_init.c, only the call to handle the fullname is different,
     * should fold the duplication.
     */
    apr_dir_t *dir;
    apr_finfo_t dirent;
    apr_int32_t finfo_flags = APR_FINFO_TYPE|APR_FINFO_NAME;
    const char *fullname;
    BOOL ok = FALSE;

    if (apr_dir_open(&dir, pathname, ptemp) != APR_SUCCESS) {
        return FALSE;
    }

    while ((apr_dir_read(&dirent, finfo_flags, dir)) == APR_SUCCESS) {
        if (dirent.filetype == APR_DIR) {
            continue; /* don't try to load directories */
        }

        fullname = apr_pstrcat(ptemp,
                               pathname, "/", dirent.name,
                               NULL);

        if (SSL_X509_INFO_load_file(ptemp, sk, fullname)) {
            ok = TRUE;
        }
    }

    apr_dir_close(dir);

    return ok;
}

/*  _________________________________________________________________
**
**  Extra Server Certificate Chain Support
**  _________________________________________________________________
*/

/*
 * Read a file that optionally contains the server certificate in PEM
 * format, possibly followed by a sequence of CA certificates that
 * should be sent to the peer in the SSL Certificate message.
 */
int SSL_CTX_use_certificate_chain(
    SSL_CTX *ctx, char *file, int skipfirst, pem_password_cb *cb)
{
    BIO *bio;
    X509 *x509;
    unsigned long err;
    int n;
    STACK_OF(X509) *extra_certs;

    if ((bio = BIO_new(BIO_s_file_internal())) == NULL)
        return -1;
    if (BIO_read_filename(bio, file) <= 0) {
        BIO_free(bio);
        return -1;
    }
    /* optionally skip a leading server certificate */
    if (skipfirst) {
        if ((x509 = PEM_read_bio_X509(bio, NULL, cb, NULL)) == NULL) {
            BIO_free(bio);
            return -1;
        }
        X509_free(x509);
    }
    /* free a perhaps already configured extra chain */
    extra_certs = ctx->extra_certs;
    if (extra_certs != NULL) {
        sk_X509_pop_free((STACK_OF(X509) *)extra_certs, X509_free);
        ctx->extra_certs = NULL;
    }
    /* create new extra chain by loading the certs */
    n = 0;
    while ((x509 = PEM_read_bio_X509(bio, NULL, cb, NULL)) != NULL) {
        if (!SSL_CTX_add_extra_chain_cert(ctx, x509)) {
            X509_free(x509);
            BIO_free(bio);
            return -1;
        }
        n++;
    }
    /* Make sure that only the error is just an EOF */
    if ((err = ERR_peek_error()) > 0) {
        if (!(   ERR_GET_LIB(err) == ERR_LIB_PEM
              && ERR_GET_REASON(err) == PEM_R_NO_START_LINE)) {
            BIO_free(bio);
            return -1;
        }
        while (ERR_get_error() > 0) ;
    }
    BIO_free(bio);
    return n;
}

/*  _________________________________________________________________
**
**  Session Stuff
**  _________________________________________________________________
*/

char *SSL_SESSION_id2sz(unsigned char *id, int idlen,
                        char *str, int strsize)
{
    char *cp;
    int n;

    cp = str;
    for (n = 0; n < idlen && n < SSL_MAX_SSL_SESSION_ID_LENGTH; n++) {
        apr_snprintf(cp, strsize - (cp-str), "%02X", id[n]);
        cp += 2;
    }
    *cp = NUL;
    return str;
}
