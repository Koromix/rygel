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

#ifndef DH_H_
#define DH_H_

#include "config.h"

#include "libssh/crypto.h"

struct dh_ctx;

#define DH_CLIENT_KEYPAIR 0
#define DH_SERVER_KEYPAIR 1

#ifdef __cplusplus
extern "C" {
#endif

/* functions implemented by crypto backends */
int ssh_dh_init_common(struct ssh_crypto_struct *crypto);
void ssh_dh_cleanup(struct ssh_crypto_struct *crypto);

#if !defined(HAVE_LIBCRYPTO) || OPENSSL_VERSION_NUMBER < 0x30000000L
int ssh_dh_get_parameters(struct dh_ctx *ctx,
                          const_bignum *modulus, const_bignum *generator);
#else
int ssh_dh_get_parameters(struct dh_ctx *ctx,
                          bignum *modulus, bignum *generator);
#endif /* OPENSSL_VERSION_NUMBER */
int ssh_dh_set_parameters(struct dh_ctx *ctx,
                          const bignum modulus, const bignum generator);

int ssh_dh_keypair_gen_keys(struct dh_ctx *ctx, int peer);
#if !defined(HAVE_LIBCRYPTO) || OPENSSL_VERSION_NUMBER < 0x30000000L
int ssh_dh_keypair_get_keys(struct dh_ctx *ctx, int peer,
                            const_bignum *priv, const_bignum *pub);
#else
int ssh_dh_keypair_get_keys(struct dh_ctx *ctx, int peer,
                            bignum *priv, bignum *pub);
#endif /* OPENSSL_VERSION_NUMBER */
int ssh_dh_keypair_set_keys(struct dh_ctx *ctx, int peer,
                            bignum priv, bignum pub);

int ssh_dh_compute_shared_secret(struct dh_ctx *ctx, int local, int remote,
                                 bignum *dest);

void ssh_dh_debug_crypto(struct ssh_crypto_struct *c);

/* common functions */
int ssh_dh_init(void);
void ssh_dh_finalize(void);

int ssh_dh_import_next_pubkey_blob(ssh_session session,
                                   ssh_string pubkey_blob);

ssh_key ssh_dh_get_current_server_publickey(ssh_session session);
int ssh_dh_get_current_server_publickey_blob(ssh_session session,
                                             ssh_string *pubkey_blob);
ssh_key ssh_dh_get_next_server_publickey(ssh_session session);
int ssh_dh_get_next_server_publickey_blob(ssh_session session,
                                          ssh_string *pubkey_blob);
int dh_handshake(ssh_session session);

int ssh_client_dh_init(ssh_session session);
void ssh_client_dh_remove_callbacks(ssh_session session);
#ifdef WITH_SERVER
void ssh_server_dh_init(ssh_session session);
#endif /* WITH_SERVER */
int ssh_server_dh_process_init(ssh_session session, ssh_buffer packet);
int ssh_fallback_group(uint32_t pmax, bignum *p, bignum *g);
bool ssh_dh_is_known_group(bignum modulus, bignum generator);

#ifdef __cplusplus
}
#endif

#endif /* DH_H_ */
