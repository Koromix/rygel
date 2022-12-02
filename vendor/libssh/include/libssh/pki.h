/*
 * This file is part of the SSH Library
 *
 * Copyright (c) 2010 by Aris Adamantiadis
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PKI_H_
#define PKI_H_

#include <stdint.h>
#include "priv.h"
#ifdef HAVE_OPENSSL_EC_H
#include <openssl/ec.h>
#endif
#ifdef HAVE_OPENSSL_ECDSA_H
#include <openssl/ecdsa.h>
#endif
#ifdef HAVE_LIBCRYPTO
#include <openssl/evp.h>
#endif
#include "crypto.h"
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_ED25519)
/* If using OpenSSL implementation, define the signature lenght which would be
 * defined in libssh/ed25519.h otherwise */
#define ED25519_SIG_LEN 64
#else
#include "ed25519.h"
#endif
/* This definition is used for both OpenSSL and internal implementations */
#define ED25519_KEY_LEN 32

#define MAX_PUBKEY_SIZE 0x100000 /* 1M */
#define MAX_PRIVKEY_SIZE 0x400000 /* 4M */

#define SSH_KEY_FLAG_EMPTY   0x0
#define SSH_KEY_FLAG_PUBLIC  0x0001
#define SSH_KEY_FLAG_PRIVATE 0x0002
#define SSH_KEY_FLAG_PKCS11_URI 0x0004

struct ssh_key_struct {
    enum ssh_keytypes_e type;
    int flags;
    const char *type_c; /* Don't free it ! it is static */
    int ecdsa_nid;
#if defined(HAVE_LIBGCRYPT)
    gcry_sexp_t dsa;
    gcry_sexp_t rsa;
    gcry_sexp_t ecdsa;
#elif defined(HAVE_LIBMBEDCRYPTO)
    mbedtls_pk_context *rsa;
    mbedtls_ecdsa_context *ecdsa;
    void *dsa;
#elif defined(HAVE_LIBCRYPTO)
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    DSA *dsa;
    RSA *rsa;
#endif /* OPENSSL_VERSION_NUMBER */
/* TODO Change to new API when the OpenSSL will support export of uncompressed EC keys
 * https://github.com/openssl/openssl/pull/16624
 * Move into the #if above
 */
# if defined(HAVE_OPENSSL_ECC)
    EC_KEY *ecdsa;
# else
    void *ecdsa;
# endif /* HAVE_OPENSSL_EC_H */
    /* This holds either ENGINE key for PKCS#11 support or just key in
     * high-level format required by OpenSSL 3.0 */
    EVP_PKEY *key;
#endif /* HAVE_LIBGCRYPT */
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_ED25519)
    uint8_t *ed25519_pubkey;
    uint8_t *ed25519_privkey;
#else
    ed25519_pubkey *ed25519_pubkey;
    ed25519_privkey *ed25519_privkey;
#endif
    ssh_string sk_application;
    void *cert;
    enum ssh_keytypes_e cert_type;
};

struct ssh_signature_struct {
    enum ssh_keytypes_e type;
    enum ssh_digest_e hash_type;
    const char *type_c;
#if defined(HAVE_LIBGCRYPT)
    gcry_sexp_t dsa_sig;
    gcry_sexp_t rsa_sig;
    gcry_sexp_t ecdsa_sig;
#elif defined(HAVE_LIBMBEDCRYPTO)
    ssh_string rsa_sig;
    struct mbedtls_ecdsa_sig ecdsa_sig;
#endif /* HAVE_LIBGCRYPT */
#if !defined(HAVE_LIBCRYPTO) || !defined(HAVE_OPENSSL_ED25519)
    ed25519_signature *ed25519_sig;
#endif
    ssh_string raw_sig;

    /* Security Key specific additions */
    uint8_t sk_flags;
    uint32_t sk_counter;
};

typedef struct ssh_signature_struct *ssh_signature;

/* SSH Key Functions */
void ssh_key_clean (ssh_key key);

const char *
ssh_key_get_signature_algorithm(ssh_session session,
                                enum ssh_keytypes_e type);
enum ssh_keytypes_e ssh_key_type_from_signature_name(const char *name);
enum ssh_keytypes_e ssh_key_type_plain(enum ssh_keytypes_e type);
enum ssh_digest_e ssh_key_type_to_hash(ssh_session session,
                                       enum ssh_keytypes_e type);
enum ssh_digest_e ssh_key_hash_from_name(const char *name);

#define is_ecdsa_key_type(t) \
    ((t) >= SSH_KEYTYPE_ECDSA_P256 && (t) <= SSH_KEYTYPE_ECDSA_P521)

#define is_cert_type(kt)\
      ((kt) == SSH_KEYTYPE_DSS_CERT01 ||\
       (kt) == SSH_KEYTYPE_RSA_CERT01 ||\
       (kt) == SSH_KEYTYPE_SK_ECDSA_CERT01 ||\
       (kt) == SSH_KEYTYPE_SK_ED25519_CERT01 ||\
      ((kt) >= SSH_KEYTYPE_ECDSA_P256_CERT01 &&\
       (kt) <= SSH_KEYTYPE_ED25519_CERT01))

/* SSH Signature Functions */
ssh_signature ssh_signature_new(void);
void ssh_signature_free(ssh_signature sign);
#define SSH_SIGNATURE_FREE(x) \
    do { ssh_signature_free(x); x = NULL; } while(0)

int ssh_pki_export_signature_blob(const ssh_signature sign,
                                  ssh_string *sign_blob);
int ssh_pki_import_signature_blob(const ssh_string sig_blob,
                                  const ssh_key pubkey,
                                  ssh_signature *psig);
int ssh_pki_signature_verify(ssh_session session,
                             ssh_signature sig,
                             const ssh_key key,
                             const unsigned char *digest,
                             size_t dlen);

/* SSH Public Key Functions */
int ssh_pki_export_pubkey_blob(const ssh_key key,
                               ssh_string *pblob);
int ssh_pki_import_pubkey_blob(const ssh_string key_blob,
                               ssh_key *pkey);

int ssh_pki_import_cert_blob(const ssh_string cert_blob,
                             ssh_key *pkey);


/* SSH Signing Functions */
ssh_string ssh_pki_do_sign(ssh_session session, ssh_buffer sigbuf,
    const ssh_key privatekey, enum ssh_digest_e hash_type);
ssh_string ssh_pki_do_sign_agent(ssh_session session,
                                 struct ssh_buffer_struct *buf,
                                 const ssh_key pubkey);
ssh_string ssh_srv_pki_do_sign_sessionid(ssh_session session,
                                         const ssh_key privkey,
                                         const enum ssh_digest_e digest);

/* Temporary functions, to be removed after migration to ssh_key */
ssh_public_key ssh_pki_convert_key_to_publickey(const ssh_key key);
ssh_private_key ssh_pki_convert_key_to_privatekey(const ssh_key key);

int ssh_key_algorithm_allowed(ssh_session session, const char *type);
bool ssh_key_size_allowed(ssh_session session, ssh_key key);

/* Return the key size in bits */
int ssh_key_size(ssh_key key);

/* PKCS11 URI function to check if filename is a path or a PKCS11 URI */
bool ssh_pki_is_uri(const char *filename);
char *ssh_pki_export_pub_uri_from_priv_uri(const char *priv_uri);

#endif /* PKI_H_ */
