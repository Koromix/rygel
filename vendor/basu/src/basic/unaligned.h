/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#ifdef __FreeBSD__
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#include <stdint.h>

static inline uint64_t unaligned_read_le64(const void *_u) {
        const struct __attribute__((packed, may_alias)) { uint64_t x; } *u = _u;

        return le64toh(u->x);
}
