/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#ifdef __linux__
#include <alloca.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "macro.h"

#define new(t, n) ((t*) malloc_multiply(sizeof(t), (n)))

#define new0(t, n) ((t*) calloc((n), sizeof(t)))

#define newa(t, n)                                              \
        ({                                                      \
                assert(!size_multiply_overflow(sizeof(t), n));  \
                (t*) alloca(sizeof(t)*(n));                     \
        })

#define newdup(t, p, n) ((t*) memdup_multiply(p, sizeof(t), (n)))

#define malloc0(n) (calloc(1, (n)))

static inline void *mfree(void *memory) {
        free(memory);
        return NULL;
}

#define free_and_replace(a, b)                  \
        ({                                      \
                free(a);                        \
                (a) = (b);                      \
                (b) = NULL;                     \
                0;                              \
        })

#ifdef __FreeBSD__
#define mempcpy __builtin_mempcpy
#endif

void* memdup(const void *p, size_t l) _alloc_(2);
void* memdup_suffix0(const void *p, size_t l) _alloc_(2);

static inline void freep(void *p) {
        free(*(void**) p);
}

#define _cleanup_free_ _cleanup_(freep)

static inline bool size_multiply_overflow(size_t size, size_t need) {
        return _unlikely_(need != 0 && size > (SIZE_MAX / need));
}

_malloc_  _alloc_(1, 2) static inline void *malloc_multiply(size_t size, size_t need) {
        if (size_multiply_overflow(size, need))
                return NULL;

        return malloc(size * need);
}

#if !HAVE_REALLOCARRAY
_alloc_(2, 3) static inline void *reallocarray(void *p, size_t need, size_t size) {
        if (size_multiply_overflow(size, need))
                return NULL;

        return realloc(p, size * need);
}
#endif

_alloc_(2, 3) static inline void *memdup_multiply(const void *p, size_t size, size_t need) {
        if (size_multiply_overflow(size, need))
                return NULL;

        return memdup(p, size * need);
}

void* greedy_realloc(void **p, size_t *allocated, size_t need, size_t size);

#define GREEDY_REALLOC(array, allocated, need)                          \
        greedy_realloc((void**) &(array), &(allocated), (need), sizeof((array)[0]))

/* Takes inspiration from Rusts's Option::take() method: reads and returns a pointer, but at the same time resets it to
 * NULL. See: https://doc.rust-lang.org/std/option/enum.Option.html#method.take */
#define TAKE_PTR(ptr)                           \
        ({                                      \
                typeof(ptr) _ptr_ = (ptr);      \
                (ptr) = NULL;                   \
                _ptr_;                          \
        })

#ifndef strndupa
#define strndupa(src, len)                                    \
        ({                                                    \
                const char *__src = (src);                    \
                size_t __strlen = strnlen(__src, (len));      \
                char *__dst = alloca(__strlen + 1);           \
                __dst[__strlen] = '\0';                       \
                memcpy(__dst, __src, __strlen);               \
         })
#endif
