/*
 * Copyright (c) The mlkem-native project authors
 * Copyright (c) The mldsa-native project authors
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
 */

#ifndef MLD_FIPS202_NATIVE_AUTO_H
#define MLD_FIPS202_NATIVE_AUTO_H

/*
 * Default FIPS202 backend
 */
#include "../../sys.h"

#if defined(MLD_SYS_AARCH64)
#include "aarch64/auto.h"
#endif

#if defined(MLD_SYS_X86_64) && defined(MLD_SYS_X86_64_AVX2)
#include "x86_64/xkcp.h"
#endif

/* We do not yet include the FIPS202 backend for Armv8.1-M+MVE by default
 * as it is still experimental and undergoing review. */
/* #if defined(MLD_SYS_ARMV81M_MVE) */
/* #include "armv81m/mve.h" */
/* #endif */

#endif /* !MLD_FIPS202_NATIVE_AUTO_H */
