/*
 * Copyright (c) The mlkem-native project authors
 * Copyright (c) The mldsa-native project authors
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
 */
#ifndef MLD_DEV_FIPS202_ARMV81M_SRC_FIPS202_NATIVE_ARMV81M_H
#define MLD_DEV_FIPS202_ARMV81M_SRC_FIPS202_NATIVE_ARMV81M_H

#include "../../../../common.h"

/* Keccak round constants in bit-interleaved form */
#define mld_keccakf1600_round_constants \
  MLD_NAMESPACE(keccakf1600_round_constants)
extern const uint32_t mld_keccakf1600_round_constants[48];

#define mld_keccak_f1600_x4_mve_asm MLD_NAMESPACE(keccak_f1600_x4_mve_asm)
void mld_keccak_f1600_x4_mve_asm(uint64_t state[100], uint64_t tmpstate[100],
                                 const uint32_t rc[48]);

#endif /* !MLD_DEV_FIPS202_ARMV81M_SRC_FIPS202_NATIVE_ARMV81M_H */
