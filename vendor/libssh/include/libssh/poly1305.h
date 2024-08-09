/*
 * Public Domain poly1305 from Andrew Moon
 * poly1305-donna-unrolled.c from https://github.com/floodyberry/poly1305-donna
 */

#ifndef POLY1305_H
#define POLY1305_H
#include "libssh/chacha20-poly1305-common.h"

#ifdef __cplusplus
extern "C" {
#endif

void poly1305_auth(uint8_t out[POLY1305_TAGLEN], const uint8_t *m, size_t inlen,
    const uint8_t key[POLY1305_KEYLEN])
#ifdef HAVE_GCC_BOUNDED_ATTRIBUTE
    __attribute__((__bounded__(__minbytes__, 1, POLY1305_TAGLEN)))
    __attribute__((__bounded__(__buffer__, 2, 3)))
    __attribute__((__bounded__(__minbytes__, 4, POLY1305_KEYLEN)))
#endif
    ;

#ifdef __cplusplus
}
#endif

#endif	/* POLY1305_H */
