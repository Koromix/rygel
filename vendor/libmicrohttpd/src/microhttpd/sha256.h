/* sha256.h

   The sha256 hash function.

   Copyright (C) 2001, 2012 Niels Möller
   Copyright (C) 2018 Christian Grothoff (extraction of minimal subset
     from GNU Nettle to work with GNU libmicrohttpd)

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#ifndef NETTLE_SHA2_H_INCLUDED
#define NETTLE_SHA2_H_INCLUDED

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE 64

/* Digest is kept internally as 8 32-bit words. */
#define _SHA256_DIGEST_LENGTH 8

struct sha256_ctx
{
  uint32_t state[_SHA256_DIGEST_LENGTH];    /* State variables */
  uint64_t count;                           /* 64-bit block count */
  uint8_t block[SHA256_BLOCK_SIZE];          /* SHA256 data buffer */
  unsigned int index;                       /* index into buffer */
};


/**
 * Start SHA256 calculation.
 *
 * @param ctx_ must be a `struct sha256_ctx *`
 */
void
sha256_init (void *ctx_);


/**
 * Update hash calculation.
 *
 * @param ctx_ must be a `struct sha256_ctx *`
 * @param length number of bytes in @a data
 * @param data bytes to add to hash
 */
void
sha256_update (void *ctx_,
               const uint8_t *data,
               size_t length);

/**
 * Complete SHA256 calculation.
 *
 * @param ctx_ must be a `struct sha256_ctx *`
 * @param digest[out] set to the hash, must be #SHA256_DIGEST_SIZE bytes
 */
void
sha256_digest (void *ctx_,
               uint8_t digest[SHA256_DIGEST_SIZE]);

#endif /* NETTLE_SHA2_H_INCLUDED */
