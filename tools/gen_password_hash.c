// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../lib/libsodium/src/libsodium/include/sodium.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: gen_password_hash password\n");
        return 1;
    }

    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return 1;
    }

    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, argv[1], strlen(argv[1]),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        fprintf(stderr, "Failed to hash password\n");
        return 1;
    }

    printf("PasswordHash = %s\n", hash);

    return 0;
}
