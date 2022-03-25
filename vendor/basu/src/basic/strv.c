/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc-util.h"
#include "escape.h"
#include "strv.h"

char *strv_find(char **l, const char *name) {
        char **i;

        assert(name);

        STRV_FOREACH(i, l)
                if (streq(*i, name))
                        return *i;

        return NULL;
}

void strv_clear(char **l) {
        char **k;

        if (!l)
                return;

        for (k = l; *k; k++)
                free(*k);

        *l = NULL;
}

char **strv_free(char **l) {
        strv_clear(l);
        return mfree(l);
}

char **strv_free_erase(char **l) {
        char **i;

        STRV_FOREACH(i, l)
                string_erase(*i);

        return strv_free(l);
}

char **strv_copy(char * const *l) {
        char **r, **k;

        k = r = new(char*, strv_length(l) + 1);
        if (!r)
                return NULL;

        if (l)
                for (; *l; k++, l++) {
                        *k = strdup(*l);
                        if (!*k) {
                                strv_free(r);
                                return NULL;
                        }
                }

        *k = NULL;
        return r;
}

size_t strv_length(char * const *l) {
        size_t n = 0;

        if (!l)
                return 0;

        for (; *l; l++)
                n++;

        return n;
}

char **strv_new_ap(const char *x, va_list ap) {
        const char *s;
        _cleanup_strv_free_ char **a = NULL;
        size_t n = 0, i = 0;
        va_list aq;

        /* As a special trick we ignore all listed strings that equal
         * STRV_IGNORE. This is supposed to be used with the
         * STRV_IFNOTNULL() macro to include possibly NULL strings in
         * the string list. */

        if (x) {
                n = x == STRV_IGNORE ? 0 : 1;

                va_copy(aq, ap);
                while ((s = va_arg(aq, const char*))) {
                        if (s == STRV_IGNORE)
                                continue;

                        n++;
                }

                va_end(aq);
        }

        a = new(char*, n+1);
        if (!a)
                return NULL;

        if (x) {
                if (x != STRV_IGNORE) {
                        a[i] = strdup(x);
                        if (!a[i])
                                return NULL;
                        i++;
                }

                while ((s = va_arg(ap, const char*))) {

                        if (s == STRV_IGNORE)
                                continue;

                        a[i] = strdup(s);
                        if (!a[i])
                                return NULL;

                        i++;
                }
        }

        a[i] = NULL;

        return TAKE_PTR(a);
}

char **strv_new_internal(const char *x, ...) {
        char **r;
        va_list ap;

        va_start(ap, x);
        r = strv_new_ap(x, ap);
        va_end(ap);

        return r;
}

int strv_push(char ***l, char *value) {
        char **c;
        size_t n, m;

        if (!value)
                return 0;

        n = strv_length(*l);

        /* Increase and check for overflow */
        m = n + 2;
        if (m < n)
                return -ENOMEM;

        c = reallocarray(*l, m, sizeof(char*));
        if (!c)
                return -ENOMEM;

        c[n] = value;
        c[n+1] = NULL;

        *l = c;
        return 0;
}

int strv_consume(char ***l, char *value) {
        int r;

        r = strv_push(l, value);
        if (r < 0)
                free(value);

        return r;
}

int strv_extend(char ***l, const char *value) {
        char *v;

        if (!value)
                return 0;

        v = strdup(value);
        if (!v)
                return -ENOMEM;

        return strv_consume(l, v);
}

char **strv_parse_nulstr(const char *s, size_t l) {
        /* l is the length of the input data, which will be split at NULs into
         * elements of the resulting strv. Hence, the number of items in the resulting strv
         * will be equal to one plus the number of NUL bytes in the l bytes starting at s,
         * unless s[l-1] is NUL, in which case the final empty string is not stored in
         * the resulting strv, and length is equal to the number of NUL bytes.
         *
         * Note that contrary to a normal nulstr which cannot contain empty strings, because
         * the input data is terminated by any two consequent NUL bytes, this parser accepts
         * empty strings in s.
         */

        const char *p;
        size_t c = 0, i = 0;
        char **v;

        assert(s || l <= 0);

        if (l <= 0)
                return new0(char*, 1);

        for (p = s; p < s + l; p++)
                if (*p == 0)
                        c++;

        if (s[l-1] != 0)
                c++;

        v = new0(char*, c+1);
        if (!v)
                return NULL;

        p = s;
        while (p < s + l) {
                const char *e;

                e = memchr(p, 0, s + l - p);

                v[i] = strndup(p, e ? e - p : s + l - p);
                if (!v[i]) {
                        strv_free(v);
                        return NULL;
                }

                i++;

                if (!e)
                        break;

                p = e + 1;
        }

        assert(i == c);

        return v;
}

static int str_compare(char * const *a, char * const *b) {
        return strcmp(*a, *b);
}

char **strv_sort(char **l) {
        typesafe_qsort(l, strv_length(l), str_compare);
        return l;
}

bool strv_fnmatch(char* const* patterns, const char *s, int flags) {
        char* const* p;

        STRV_FOREACH(p, patterns)
                if (fnmatch(*p, s, flags) == 0)
                        return true;

        return false;
}

char ***strv_free_free(char ***l) {
        char ***i;

        if (!l)
                return NULL;

        for (i = l; *i; i++)
                strv_free(*i);

        return mfree(l);
}
