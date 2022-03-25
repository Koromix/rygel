/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>

#include "macro.h"

#define snprintf_ok(buf, len, fmt, ...) \
        ((size_t) snprintf(buf, len, fmt, __VA_ARGS__) < (len))

#define xsprintf(buf, fmt, ...) \
        assert_message_se(snprintf_ok(buf, ELEMENTSOF(buf), fmt, __VA_ARGS__), "xsprintf: " #buf "[] must be big enough")

