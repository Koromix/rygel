/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "format-util.h"
#include "macro.h"
#include "missing.h"
#include "time-util.h"

#ifndef __COMPAR_FN_T
#define __COMPAR_FN_T
typedef int (*__compar_fn_t)(const void *, const void *);
#endif

size_t page_size(void) _pure_;
#define PAGE_ALIGN(l) ALIGN_TO((l), page_size())

static inline const char* yes_no(bool b) {
        return b ? "yes" : "no";
}

static inline const char* true_false(bool b) {
        return b ? "true" : "false";
}

extern int saved_argc;
extern char **saved_argv;

/**
 * Normal qsort requires base to be nonnull. Here were require
 * that only if nmemb > 0.
 */
static inline void qsort_safe(void *base, size_t nmemb, size_t size, __compar_fn_t compar) {
        if (nmemb <= 1)
                return;

        assert(base);
        qsort(base, nmemb, size, compar);
}

/* A wrapper around the above, but that adds typesafety: the element size is automatically derived from the type and so
 * is the prototype for the comparison function */
#define typesafe_qsort(p, n, func)                                      \
        ({                                                              \
                int (*_func_)(const typeof(p[0])*, const typeof(p[0])*) = func; \
                qsort_safe((p), (n), sizeof((p)[0]), (__compar_fn_t) _func_); \
        })

/* Normal memcpy requires src to be nonnull. We do nothing if n is 0. */
static inline void memcpy_safe(void *dst, const void *src, size_t n) {
        if (n == 0)
                return;
        assert(src);
        memcpy(dst, src, n);
}


#define memzero(x,l)                                            \
        ({                                                      \
                size_t _l_ = (l);                               \
                void *_x_ = (x);                                \
                _l_ == 0 ? _x_ : memset(_x_, 0, _l_);           \
        })

#define zero(x) (memzero(&(x), sizeof(x)))

static inline void *mempset(void *s, int c, size_t n) {
        memset(s, c, n);
        return (uint8_t*)s + n;
}

static inline void _reset_errno_(int *saved_errno) {
        errno = *saved_errno;
}

#define PROTECT_ERRNO _cleanup_(_reset_errno_) __attribute__((unused)) int _saved_errno_ = errno

static inline unsigned log2u(unsigned x) {
        assert(x > 0);

        return sizeof(unsigned) * 8 - __builtin_clz(x) - 1;
}

static inline unsigned log2u_round_up(unsigned x) {
        assert(x > 0);

        if (x == 1)
                return 0;

        return log2u(x - 1) + 1;
}

int version(void);
