/* SPDX-License-Identifier: LGPL-2.1+ */

#include <stdlib.h>
#include <string.h>

#include "alloc-util.h"
#include "escape.h"
#include "hexdecoct.h"

int cescape_char(char c, char *buf) {
        char *buf_old = buf;

        /* Needs space for 4 characters in the buffer */

        switch (c) {

                case '\a':
                        *(buf++) = '\\';
                        *(buf++) = 'a';
                        break;
                case '\b':
                        *(buf++) = '\\';
                        *(buf++) = 'b';
                        break;
                case '\f':
                        *(buf++) = '\\';
                        *(buf++) = 'f';
                        break;
                case '\n':
                        *(buf++) = '\\';
                        *(buf++) = 'n';
                        break;
                case '\r':
                        *(buf++) = '\\';
                        *(buf++) = 'r';
                        break;
                case '\t':
                        *(buf++) = '\\';
                        *(buf++) = 't';
                        break;
                case '\v':
                        *(buf++) = '\\';
                        *(buf++) = 'v';
                        break;
                case '\\':
                        *(buf++) = '\\';
                        *(buf++) = '\\';
                        break;
                case '"':
                        *(buf++) = '\\';
                        *(buf++) = '"';
                        break;
                case '\'':
                        *(buf++) = '\\';
                        *(buf++) = '\'';
                        break;

                default:
                        /* For special chars we prefer octal over
                         * hexadecimal encoding, simply because glib's
                         * g_strescape() does the same */
                        if ((c < ' ') || (c >= 127)) {
                                *(buf++) = '\\';
                                *(buf++) = octchar((unsigned char) c >> 6);
                                *(buf++) = octchar((unsigned char) c >> 3);
                                *(buf++) = octchar((unsigned char) c);
                        } else
                                *(buf++) = c;
                        break;
        }

        return buf - buf_old;
}

char *cescape_length(const char *s, size_t n) {
        const char *f;
        char *r, *t;

        assert(s || n == 0);

        /* Does C style string escaping. May be reversed with
         * cunescape(). */

        r = new(char, n*4 + 1);
        if (!r)
                return NULL;

        for (f = s, t = r; f < s + n; f++)
                t += cescape_char(*f, t);

        *t = 0;

        return r;
}

char *cescape(const char *s) {
        assert(s);

        return cescape_length(s, strlen(s));
}
