/* SPDX-License-Identifier: LGPL-2.1+ */

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "alloc-util.h"
#include "hexdecoct.h"
#include "string-util.h"

char octchar(int x) {
        return '0' + (x & 7);
}

int undecchar(char c) {

        if (c >= '0' && c <= '9')
                return c - '0';

        return -EINVAL;
}

char hexchar(int x) {
        static const char table[16] = "0123456789abcdef";

        return table[x & 15];
}

int unhexchar(char c) {

        if (c >= '0' && c <= '9')
                return c - '0';

        if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;

        if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;

        return -EINVAL;
}

char *hexmem(const void *p, size_t l) {
        const uint8_t *x;
        char *r, *z;

        z = r = new(char, l * 2 + 1);
        if (!r)
                return NULL;

        for (x = p; x < (const uint8_t*) p + l; x++) {
                *(z++) = hexchar(*x >> 4);
                *(z++) = hexchar(*x & 15);
        }

        *z = 0;
        return r;
}

static int unhex_next(const char **p, size_t *l) {
        int r;

        assert(p);
        assert(l);

        /* Find the next non-whitespace character, and decode it. We
         * greedily skip all preceding and all following whitespace. */

        for (;;) {
                if (*l == 0)
                        return -EPIPE;

                if (!strchr(WHITESPACE, **p))
                        break;

                /* Skip leading whitespace */
                (*p)++, (*l)--;
        }

        r = unhexchar(**p);
        if (r < 0)
                return r;

        for (;;) {
                (*p)++, (*l)--;

                if (*l == 0 || !strchr(WHITESPACE, **p))
                        break;

                /* Skip following whitespace */
        }

        return r;
}

int unhexmem(const char *p, size_t l, void **ret, size_t *ret_len) {
        _cleanup_free_ uint8_t *buf = NULL;
        const char *x;
        uint8_t *z;

        assert(ret);
        assert(ret_len);
        assert(p || l == 0);

        if (l == (size_t) -1)
                l = strlen(p);

        /* Note that the calculation of memory size is an upper boundary, as we ignore whitespace while decoding */
        buf = malloc((l + 1) / 2 + 1);
        if (!buf)
                return -ENOMEM;

        for (x = p, z = buf;;) {
                int a, b;

                a = unhex_next(&x, &l);
                if (a == -EPIPE) /* End of string */
                        break;
                if (a < 0)
                        return a;

                b = unhex_next(&x, &l);
                if (b < 0)
                        return b;

                *(z++) = (uint8_t) a << 4 | (uint8_t) b;
        }

        *z = 0;

        *ret_len = (size_t) (z - buf);
        *ret = TAKE_PTR(buf);

        return 0;
}
