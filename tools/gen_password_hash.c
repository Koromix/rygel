// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../lib/libsodium/src/libsodium/include/sodium.h"

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#ifdef _WIN32
    #include <malloc.h>
    #include <conio.h>
#else
    #include <alloca.h>
    #include <termios.h>
    #include <unistd.h>
#endif

bool get_password_safe(char *out_buf, int out_buf_size)
{
#ifdef _WIN32
    int len = 0;
    while (len < out_buf_size - 1) {
        int c = getch();
        switch (c) {
            case '\r':
            case '\n':
            case EOF: { goto exit; } break;

            case '\b': {
                if (len)
                    len--;
            } break;

            default: {
                out_buf[len++] = (char)c;
            } break;
        }
    }

exit:
    out_buf[len] = 0;
    return true;
#else
    bool success = false;

    struct termios old_tio;
    if (tcgetattr(STDIN_FILENO, &old_tio) != 0) {
        fprintf(stderr, "tcgetattr() failed: %s\n", strerror(errno));
        goto exit;
    }

    struct termios new_tio;
    new_tio = old_tio;
    new_tio.c_lflag &= ~(unsigned int)ECHO;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_tio) != 0) {
        fprintf(stderr, "tcsetattr() failed: %s\n", strerror(errno));
        goto exit;
    }

    success = fgets(out_buf, out_buf_size, stdin);
    if (!success) {
        fprintf(stderr, "fgets() failed: %s\n", strerror(errno));
        goto exit;
    }

    int offset = (int)strlen(out_buf) - 1;
    while (offset >= 0 && (out_buf[offset] == '\r' || out_buf[offset] == '\n')) {
        out_buf[offset--] = 0;
    }

exit:
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_tio);
    return success;
#endif
}

static void print_usage(FILE *fp)
{
    fprintf(fp, "Usage: gen_password_hash [-p password]\n");
}

int main(int argc, char **argv)
{
    const char *password = NULL;
    if (argc >= 2) {
        if (!strcmp(argv[1], "--help")) {
            print_usage(stdout);
            return 0;
        } else if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--password")) {
            if (argc < 3) {
                fprintf(stderr, "Missing argument for --password\n");
                return 1;
            }

            password = argv[2];
        }
    }

    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return 1;
    }

    if (!password) {
        printf("Password: ");
        fflush(stdout);

        char *buf = alloca(1024);
        if (!get_password_safe(buf, 1024))
            return 1;
        password = buf;

        fputc('\n', stdout);
    }

    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, password, strlen(password),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        fprintf(stderr, "Failed to hash password\n");
        return 1;
    }

    printf("PasswordHash = %s\n", hash);

    return 0;
}
