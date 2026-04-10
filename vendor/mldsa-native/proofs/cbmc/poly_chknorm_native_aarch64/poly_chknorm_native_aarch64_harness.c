// Copyright (c) The mldsa-native project authors
// SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT

#include <stdint.h>
#include "cbmc.h"
#include "params.h"

int mld_poly_chknorm_native(const int32_t a[MLDSA_N], int32_t B);

void harness(void)
{
  const int32_t *a;
  int32_t B;
  int t;
  t = mld_poly_chknorm_native(a, B);
}
