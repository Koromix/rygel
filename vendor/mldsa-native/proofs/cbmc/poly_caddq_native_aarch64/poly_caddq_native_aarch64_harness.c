// Copyright (c) The mldsa-native project authors
// SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT

#include <stdint.h>
#include "cbmc.h"
#include "params.h"

int mld_poly_caddq_native(int32_t a[MLDSA_N]);

void harness(void)
{
  int32_t *a;
  int t;
  t = mld_poly_caddq_native(a);
}
