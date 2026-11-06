#ifndef SHA1_H
#define SHA1_H

/*
   SHA-1 in C
   By Steve Reid <steve@edmweb.com>
   100% Public Domain
 */

#include "stdint.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct
{
    uint32_t state[5];
    size_t count[2];
    unsigned char buffer[64];
} sha1_CTX;

void sha1_Transform(
    uint32_t state[5],
    const unsigned char buffer[64]
    );

void sha1_Init(
    sha1_CTX * context
    );

void sha1_Update(
    sha1_CTX * context,
    const unsigned char *data,
    size_t len
    );

void sha1_Final(
    unsigned char digest[20],
    sha1_CTX * context
    );

void sha1_Hash(
    unsigned char digest[20],
    const unsigned char *data,
    size_t len);

#if defined(__cplusplus)
}
#endif

#endif /* SHA1_H */
