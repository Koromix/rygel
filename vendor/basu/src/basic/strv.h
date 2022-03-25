/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "alloc-util.h"
#include "macro.h"
#include "string-util.h"
#include "util.h"

char *strv_find(char **l, const char *name) _pure_;

char **strv_free(char **l);
DEFINE_TRIVIAL_CLEANUP_FUNC(char**, strv_free);
#define _cleanup_strv_free_ _cleanup_(strv_freep)

char **strv_free_erase(char **l);
DEFINE_TRIVIAL_CLEANUP_FUNC(char**, strv_free_erase);

void strv_clear(char **l);

char **strv_copy(char * const *l);
size_t strv_length(char * const *l) _pure_;

int strv_extend(char ***l, const char *value);
int strv_push(char ***l, char *value);

int strv_consume(char ***l, char *value);

#define strv_contains(l, s) (!!strv_find((l), (s)))

char **strv_new_internal(const char *x, ...) _sentinel_;
char **strv_new_ap(const char *x, va_list ap);
#define strv_new(...) strv_new_internal(__VA_ARGS__, NULL)

#define STRV_IGNORE ((const char *) -1)

static inline bool strv_isempty(char * const *l) {
        return !l || !*l;
}

char **strv_parse_nulstr(const char *s, size_t l);

#define STRV_FOREACH(s, l)                      \
        for ((s) = (l); (s) && *(s); (s)++)

char **strv_sort(char **l);

#define STRV_MAKE(...) ((char**) ((const char*[]) { __VA_ARGS__, NULL }))

#define strv_from_stdarg_alloca(first)                          \
        ({                                                      \
                char **_l;                                      \
                                                                \
                if (!first)                                     \
                        _l = (char**) &first;                   \
                else {                                          \
                        size_t _n;                              \
                        va_list _ap;                            \
                                                                \
                        _n = 1;                                 \
                        va_start(_ap, first);                   \
                        while (va_arg(_ap, char*))              \
                                _n++;                           \
                        va_end(_ap);                            \
                                                                \
                        _l = newa(char*, _n+1);                 \
                        _l[_n = 0] = (char*) first;             \
                        va_start(_ap, first);                   \
                        for (;;) {                              \
                                _l[++_n] = va_arg(_ap, char*);  \
                                if (!_l[_n])                    \
                                        break;                  \
                        }                                       \
                        va_end(_ap);                            \
                }                                               \
                _l;                                             \
        })

#define STR_IN_SET(x, ...) strv_contains(STRV_MAKE(__VA_ARGS__), x)

bool strv_fnmatch(char* const* patterns, const char *s, int flags);

char ***strv_free_free(char ***l);
DEFINE_TRIVIAL_CLEANUP_FUNC(char***, strv_free_free);
