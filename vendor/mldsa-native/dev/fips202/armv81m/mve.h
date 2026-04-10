/*
 * Copyright (c) The mlkem-native project authors
 * Copyright (c) The mldsa-native project authors
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
 */

#ifndef MLD_DEV_FIPS202_ARMV81M_MVE_H
#define MLD_DEV_FIPS202_ARMV81M_MVE_H

#define MLD_FIPS202_NATIVE_ARMV81M

/* Part of backend API */
#define MLD_USE_FIPS202_X4_NATIVE
/* Guard for assembly file */
#define MLD_FIPS202_ARMV81M_NEED_X4

#if !defined(__ASSEMBLER__)
#include "../api.h"

#define mld_keccak_f1600_x4_native_impl \
  MLD_NAMESPACE(keccak_f1600_x4_native_impl)
int mld_keccak_f1600_x4_native_impl(uint64_t *state);

MLD_MUST_CHECK_RETURN_VALUE
static MLD_INLINE int mld_keccak_f1600_x4_native(uint64_t *state)
{
  return mld_keccak_f1600_x4_native_impl(state);
}

#endif /* !__ASSEMBLER__ */

#endif /* !MLD_DEV_FIPS202_ARMV81M_MVE_H */
