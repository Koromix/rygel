/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc-util.h"
#include "escape.h"
#include "gunicode.h"
#include "locale-util.h"
#include "string-util.h"
#include "utf8.h"

int strcmp_ptr(const char *a, const char *b) {

        /* Like strcmp(), but tries to make sense of NULL pointers */
        if (a && b)
                return strcmp(a, b);

        if (!a && b)
                return -1;

        if (a && !b)
                return 1;

        return 0;
}

char* endswith(const char *s, const char *postfix) {
        size_t sl, pl;

        assert(s);
        assert(postfix);

        sl = strlen(s);
        pl = strlen(postfix);

        if (pl == 0)
                return (char*) s + sl;

        if (sl < pl)
                return NULL;

        if (memcmp(s + sl - pl, postfix, pl) != 0)
                return NULL;

        return (char*) s + sl - pl;
}

char *strnappend(const char *s, const char *suffix, size_t b) {
        size_t a;
        char *r;

        if (!s && !suffix)
                return strdup("");

        if (!s)
                return strndup(suffix, b);

        if (!suffix)
                return strdup(s);

        assert(s);
        assert(suffix);

        a = strlen(s);
        if (b > ((size_t) -1) - a)
                return NULL;

        r = new(char, a+b+1);
        if (!r)
                return NULL;

        memcpy(r, s, a);
        memcpy(r+a, suffix, b);
        r[a+b] = 0;

        return r;
}

char *strappend(const char *s, const char *suffix) {
        return strnappend(s, suffix, strlen_ptr(suffix));
}

char *strjoin_real(const char *x, ...) {
        va_list ap;
        size_t l;
        char *r, *p;

        va_start(ap, x);

        if (x) {
                l = strlen(x);

                for (;;) {
                        const char *t;
                        size_t n;

                        t = va_arg(ap, const char *);
                        if (!t)
                                break;

                        n = strlen(t);
                        if (n > ((size_t) -1) - l) {
                                va_end(ap);
                                return NULL;
                        }

                        l += n;
                }
        } else
                l = 0;

        va_end(ap);

        r = new(char, l+1);
        if (!r)
                return NULL;

        if (x) {
                p = stpcpy(r, x);

                va_start(ap, x);

                for (;;) {
                        const char *t;

                        t = va_arg(ap, const char *);
                        if (!t)
                                break;

                        p = stpcpy(p, t);
                }

                va_end(ap);
        } else
                r[0] = 0;

        return r;
}

char ascii_tolower(char x) {

        if (x >= 'A' && x <= 'Z')
                return x - 'A' + 'a';

        return x;
}

char *ascii_strlower(char *t) {
        char *p;

        assert(t);

        for (p = t; *p; p++)
                *p = ascii_tolower(*p);

        return t;
}

static int write_ellipsis(char *buf, bool unicode) {
        if (unicode || is_locale_utf8()) {
                buf[0] = 0xe2; /* tri-dot ellipsis: … */
                buf[1] = 0x80;
                buf[2] = 0xa6;
        } else {
                buf[0] = '.';
                buf[1] = '.';
                buf[2] = '.';
        }

        return 3;
}

static char *ascii_ellipsize_mem(const char *s, size_t old_length, size_t new_length, unsigned percent) {
        size_t x, need_space, suffix_len;
        char *t;

        assert(s);
        assert(percent <= 100);
        assert(new_length != (size_t) -1);

        if (old_length <= new_length)
                return strndup(s, old_length);

        /* Special case short ellipsations */
        switch (new_length) {

        case 0:
                return strdup("");

        case 1:
                if (is_locale_utf8())
                        return strdup("…");
                else
                        return strdup(".");

        case 2:
                if (!is_locale_utf8())
                        return strdup("..");

                break;

        default:
                break;
        }

        /* Calculate how much space the ellipsis will take up. If we are in UTF-8 mode we only need space for one
         * character ("…"), otherwise for three characters ("..."). Note that in both cases we need 3 bytes of storage,
         * either for the UTF-8 encoded character or for three ASCII characters. */
        need_space = is_locale_utf8() ? 1 : 3;

        t = new(char, new_length+3);
        if (!t)
                return NULL;

        assert(new_length >= need_space);

        x = ((new_length - need_space) * percent + 50) / 100;
        assert(x <= new_length - need_space);

        memcpy(t, s, x);
        write_ellipsis(t + x, false);
        suffix_len = new_length - x - need_space;
        memcpy(t + x + 3, s + old_length - suffix_len, suffix_len);
        *(t + x + 3 + suffix_len) = '\0';

        return t;
}

char *ellipsize_mem(const char *s, size_t old_length, size_t new_length, unsigned percent) {
        size_t x, k, len, len2;
        const char *i, *j;
        char *e;
        int r;

        /* Note that 'old_length' refers to bytes in the string, while 'new_length' refers to character cells taken up
         * on screen. This distinction doesn't matter for ASCII strings, but it does matter for non-ASCII UTF-8
         * strings.
         *
         * Ellipsation is done in a locale-dependent way:
         * 1. If the string passed in is fully ASCII and the current locale is not UTF-8, three dots are used ("...")
         * 2. Otherwise, a unicode ellipsis is used ("…")
         *
         * In other words: you'll get a unicode ellipsis as soon as either the string contains non-ASCII characters or
         * the current locale is UTF-8.
         */

        assert(s);
        assert(percent <= 100);

        if (new_length == (size_t) -1)
                return strndup(s, old_length);

        if (new_length == 0)
                return strdup("");

        /* If no multibyte characters use ascii_ellipsize_mem for speed */
        if (ascii_is_valid_n(s, old_length))
                return ascii_ellipsize_mem(s, old_length, new_length, percent);

        x = ((new_length - 1) * percent) / 100;
        assert(x <= new_length - 1);

        k = 0;
        for (i = s; i < s + old_length; i = utf8_next_char(i)) {
                char32_t c;
                int w;

                r = utf8_encoded_to_unichar(i, &c);
                if (r < 0)
                        return NULL;

                w = unichar_iswide(c) ? 2 : 1;
                if (k + w <= x)
                        k += w;
                else
                        break;
        }

        for (j = s + old_length; j > i; ) {
                char32_t c;
                int w;
                const char *jj;

                jj = utf8_prev_char(j);
                r = utf8_encoded_to_unichar(jj, &c);
                if (r < 0)
                        return NULL;

                w = unichar_iswide(c) ? 2 : 1;
                if (k + w <= new_length) {
                        k += w;
                        j = jj;
                } else
                        break;
        }
        assert(i <= j);

        /* we don't actually need to ellipsize */
        if (i == j)
                return memdup_suffix0(s, old_length);

        /* make space for ellipsis, if possible */
        if (j < s + old_length)
                j = utf8_next_char(j);
        else if (i > s)
                i = utf8_prev_char(i);

        len = i - s;
        len2 = s + old_length - j;
        e = new(char, len + 3 + len2 + 1);
        if (!e)
                return NULL;

        /*
        printf("old_length=%zu new_length=%zu x=%zu len=%u len2=%u k=%u\n",
               old_length, new_length, x, len, len2, k);
        */

        memcpy(e, s, len);
        write_ellipsis(e + len, true);
        memcpy(e + len + 3, j, len2);
        *(e + len + 3 + len2) = '\0';

        return e;
}

char *cellescape(char *buf, size_t len, const char *s) {
        /* Escape and ellipsize s into buffer buf of size len. Only non-control ASCII
         * characters are copied as they are, everything else is escaped. The result
         * is different then if escaping and ellipsization was performed in two
         * separate steps, because each sequence is either stored in full or skipped.
         *
         * This function should be used for logging about strings which expected to
         * be plain ASCII in a safe way.
         *
         * An ellipsis will be used if s is too long. It was always placed at the
         * very end.
         */

        size_t i = 0, last_char_width[4] = {}, k = 0, j;

        assert(len > 0); /* at least a terminating NUL */

        for (;;) {
                char four[4];
                int w;

                if (*s == 0) /* terminating NUL detected? then we are done! */
                        goto done;

                w = cescape_char(*s, four);
                if (i + w + 1 > len) /* This character doesn't fit into the buffer anymore? In that case let's
                                      * ellipsize at the previous location */
                        break;

                /* OK, there was space, let's add this escaped character to the buffer */
                memcpy(buf + i, four, w);
                i += w;

                /* And remember its width in the ring buffer */
                last_char_width[k] = w;
                k = (k + 1) % 4;

                s++;
        }

        /* Ellipsation is necessary. This means we might need to truncate the string again to make space for 4
         * characters ideally, but the buffer is shorter than that in the first place take what we can get */
        for (j = 0; j < ELEMENTSOF(last_char_width); j++) {

                if (i + 4 <= len) /* nice, we reached our space goal */
                        break;

                k = k == 0 ? 3 : k - 1;
                if (last_char_width[k] == 0) /* bummer, we reached the beginning of the strings */
                        break;

                assert(i >= last_char_width[k]);
                i -= last_char_width[k];
        }

        if (i + 4 <= len) /* yay, enough space */
                i += write_ellipsis(buf + i, false);
        else if (i + 3 <= len) { /* only space for ".." */
                buf[i++] = '.';
                buf[i++] = '.';
        } else if (i + 2 <= len) /* only space for a single "." */
                buf[i++] = '.';
        else
                assert(i + 1 <= len);

 done:
        buf[i] = '\0';
        return buf;
}

char *strextend_with_separator(char **x, const char *separator, ...) {
        bool need_separator;
        size_t f, l, l_separator;
        char *r, *p;
        va_list ap;

        assert(x);

        l = f = strlen_ptr(*x);

        need_separator = !isempty(*x);
        l_separator = strlen_ptr(separator);

        va_start(ap, separator);
        for (;;) {
                const char *t;
                size_t n;

                t = va_arg(ap, const char *);
                if (!t)
                        break;

                n = strlen(t);

                if (need_separator)
                        n += l_separator;

                if (n > ((size_t) -1) - l) {
                        va_end(ap);
                        return NULL;
                }

                l += n;
                need_separator = true;
        }
        va_end(ap);

        need_separator = !isempty(*x);

        r = realloc(*x, l+1);
        if (!r)
                return NULL;

        p = r + f;

        va_start(ap, separator);
        for (;;) {
                const char *t;

                t = va_arg(ap, const char *);
                if (!t)
                        break;

                if (need_separator && separator)
                        p = stpcpy(p, separator);

                p = stpcpy(p, t);

                need_separator = true;
        }
        va_end(ap);

        assert(p == r + l);

        *p = 0;
        *x = r;

        return r + l;
}

char *strrep(const char *s, unsigned n) {
        size_t l;
        char *r, *p;
        unsigned i;

        assert(s);

        l = strlen(s);
        p = r = malloc(l * n + 1);
        if (!r)
                return NULL;

        for (i = 0; i < n; i++)
                p = stpcpy(p, s);

        *p = 0;
        return r;
}

int free_and_strdup(char **p, const char *s) {
        char *t;

        assert(p);

        /* Replaces a string pointer with a strdup()ed new string,
         * possibly freeing the old one. */

        if (streq_ptr(*p, s))
                return 0;

        if (s) {
                t = strdup(s);
                if (!t)
                        return -ENOMEM;
        } else
                t = NULL;

        free(*p);
        *p = t;

        return 1;
}

int free_and_strndup(char **p, const char *s, size_t l) {
        char *t;

        assert(p);
        assert(s || l == 0);

        /* Replaces a string pointer with a strndup()ed new string,
         * freeing the old one. */

        if (!*p && !s)
                return 0;

        if (*p && s && strneq(*p, s, l) && (l > strlen(*p) || (*p)[l] == '\0'))
                return 0;

        if (s) {
                t = strndup(s, l);
                if (!t)
                        return -ENOMEM;
        } else
                t = NULL;

        free_and_replace(*p, t);
        return 1;
}

#if !HAVE_EXPLICIT_BZERO
/*
 * Pointer to memset is volatile so that compiler must de-reference
 * the pointer and can't assume that it points to any function in
 * particular (such as memset, which it then might further "optimize")
 * This approach is inspired by openssl's crypto/mem_clr.c.
 */
typedef void *(*memset_t)(void *,int,size_t);

static volatile memset_t memset_func = memset;

void* explicit_bzero_safe(void *p, size_t l) {
        if (l > 0)
                memset_func(p, '\0', l);

        return p;
}
#endif

char* string_erase(char *x) {
        if (!x)
                return NULL;

        /* A delicious drop of snake-oil! To be called on memory where
         * we stored passphrases or so, after we used them. */
        explicit_bzero_safe(x, strlen(x));
        return x;
}

char *string_free_erase(char *s) {
        return mfree(string_erase(s));
}

bool string_is_safe(const char *p) {
        const char *t;

        if (!p)
                return false;

        for (t = p; *t; t++) {
                if (*t > 0 && *t < ' ') /* no control characters */
                        return false;

                if (strchr(QUOTES "\\\x7f", *t))
                        return false;
        }

        return true;
}
