/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#if !HAVE_LIBCAP
#error "libcap unavailable"
#endif

unsigned long cap_last_cap(void);

/* Identical to linux/capability.h's CAP_TO_MASK(), but uses an unsigned 1U instead of a signed 1 for shifting left, in
 * order to avoid complaints about shifting a signed int left by 31 bits, which would make it negative. */
#define CAP_TO_MASK_CORRECTED(x) (1U << ((x) & 31U))
