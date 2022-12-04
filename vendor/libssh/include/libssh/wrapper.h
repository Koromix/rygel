/*
 * This file is part of the SSH Library
 *
 * Copyright (c) 2009 by Aris Adamantiadis
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

#ifndef WRAPPER_H_
#define WRAPPER_H_

#include <stdbool.h>

#include "config.h"
#include "libssh.h"
#include "libcrypto.h"
#include "libgcrypt.h"
#include "libmbedcrypto.h"

enum ssh_kdf_digest {
    SSH_KDF_SHA1=1,
    SSH_KDF_SHA256,
    SSH_KDF_SHA384,
    SSH_KDF_SHA512
};

enum ssh_hmac_e {
  SSH_HMAC_SHA1 = 1,
  SSH_HMAC_SHA256,
  SSH_HMAC_SHA512,
  SSH_HMAC_MD5,
  SSH_HMAC_AEAD_POLY1305,
  SSH_HMAC_AEAD_GCM,
  SSH_HMAC_NONE,
};

enum ssh_des_e {
  SSH_3DES,
  SSH_DES
};

struct ssh_hmac_struct {
  const char* name;
  enum ssh_hmac_e hmac_type;
  bool etm;
};

enum ssh_crypto_direction_e {
    SSH_DIRECTION_IN = 1,
    SSH_DIRECTION_OUT = 2,
    SSH_DIRECTION_BOTH = 3,
};

struct ssh_cipher_struct;
struct ssh_crypto_struct;

typedef struct ssh_mac_ctx_struct *ssh_mac_ctx;
MD5CTX _ssh_md5_init(void);
void _ssh_md5_update(MD5CTX c, const void *data, size_t len);
void _ssh_md5_final(unsigned char *md,MD5CTX c);

SHACTX _ssh_sha1_init(void);
void _ssh_sha1_update(SHACTX c, const void *data, size_t len);
void _ssh_sha1_final(unsigned char *md,SHACTX c);
void _ssh_sha1(const unsigned char *digest,size_t len,unsigned char *hash);

SHA256CTX _ssh_sha256_init(void);
void _ssh_sha256_update(SHA256CTX c, const void *data, size_t len);
void _ssh_sha256_final(unsigned char *md,SHA256CTX c);
void _ssh_sha256(const unsigned char *digest, size_t len, unsigned char *hash);

SHA384CTX _ssh_sha384_init(void);
void _ssh_sha384_update(SHA384CTX c, const void *data, size_t len);
void _ssh_sha384_final(unsigned char *md,SHA384CTX c);
void _ssh_sha384(const unsigned char *digest, size_t len, unsigned char *hash);

SHA512CTX _ssh_sha512_init(void);
void _ssh_sha512_update(SHA512CTX c, const void *data, size_t len);
void _ssh_sha512_final(unsigned char *md,SHA512CTX c);
void _ssh_sha512(const unsigned char *digest, size_t len, unsigned char *hash);

void evp(int nid, unsigned char *digest, size_t len, unsigned char *hash, unsigned int *hlen);
EVPCTX evp_init(int nid);
void evp_update(EVPCTX ctx, const void *data, size_t len);
void evp_final(EVPCTX ctx, unsigned char *md, unsigned int *mdlen);

HMACCTX hmac_init(const void *key,size_t len, enum ssh_hmac_e type);
int hmac_update(HMACCTX c, const void *data, size_t len);
int hmac_final(HMACCTX ctx, unsigned char *hashmacbuf, size_t *len);
size_t hmac_digest_len(enum ssh_hmac_e type);

int ssh_kdf(struct ssh_crypto_struct *crypto,
            unsigned char *key, size_t key_len,
            uint8_t key_type, unsigned char *output,
            size_t requested_len);

int crypt_set_algorithms_client(ssh_session session);
int crypt_set_algorithms_server(ssh_session session);
struct ssh_crypto_struct *crypto_new(void);
void crypto_free(struct ssh_crypto_struct *crypto);

void ssh_reseed(void);
int ssh_crypto_init(void);
void ssh_crypto_finalize(void);

void ssh_cipher_clear(struct ssh_cipher_struct *cipher);
struct ssh_hmac_struct *ssh_get_hmactab(void);
struct ssh_cipher_struct *ssh_get_ciphertab(void);
const char *ssh_hmac_type_to_string(enum ssh_hmac_e hmac_type, bool etm);

#if defined(HAVE_LIBCRYPTO) && OPENSSL_VERSION_NUMBER >= 0x30000000L
int evp_build_pkey(const char* name, OSSL_PARAM_BLD *param_bld, EVP_PKEY **pkey, int selection);
int evp_dup_dsa_pkey(const ssh_key key, ssh_key new, int demote);
int evp_dup_rsa_pkey(const ssh_key key, ssh_key new, int demote);
int evp_dup_ecdsa_pkey(const ssh_key key, ssh_key new, int demote);
#endif /* HAVE_LIBCRYPTO && OPENSSL_VERSION_NUMBER */

#endif /* WRAPPER_H_ */
