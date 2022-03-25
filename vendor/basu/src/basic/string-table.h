/* SPDX-License-Identifier: LGPL-2.1+ */

#pragma once

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "macro.h"
#include "parse-util.h"
#include "string-util.h"

#define _DEFINE_STRING_TABLE_LOOKUP_TO_STRING_FALLBACK(name,type,max,scope) \
        scope int name##_to_string_alloc(type i, char **str) {          \
                char *s;                                                \
                if (i < 0 || i > max)                                   \
                        return -ERANGE;                                 \
                if (i < (type) ELEMENTSOF(name##_table)) {              \
                        s = strdup(name##_table[i]);                    \
                        if (!s)                                         \
                                return -ENOMEM;                         \
                } else {                                                \
                        if (asprintf(&s, "%i", i) < 0)                  \
                                return -ENOMEM;                         \
                }                                                       \
                *str = s;                                               \
                return 0;                                               \
        }

#define _DEFINE_STRING_TABLE_LOOKUP_FROM_STRING_FALLBACK(name,type,max,scope) \
        scope type name##_from_string(const char *s) {                  \
                type i;                                                 \
                unsigned u = 0;                                         \
                if (!s)                                                 \
                        return (type) -1;                               \
                for (i = 0; i < (type) ELEMENTSOF(name##_table); i++)   \
                        if (streq_ptr(name##_table[i], s))              \
                                return i;                               \
                if (safe_atou(s, &u) >= 0 && u <= max)                  \
                        return (type) u;                                \
                return (type) -1;                                       \
        }                                                               \

/* For string conversions where numbers are also acceptable */
#define DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(name,type,max)         \
        _DEFINE_STRING_TABLE_LOOKUP_TO_STRING_FALLBACK(name,type,max,)  \
        _DEFINE_STRING_TABLE_LOOKUP_FROM_STRING_FALLBACK(name,type,max,)
