/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "codes.hh"

struct GhsPricing {
    GhsCode ghs_code;
    Date limit_dates[2];

    struct {
        int32_t price_cents;
        int32_t exh_cents;
        int32_t exb_cents;
    } sectors[2];
};

bool ParseGhsPricings(ArrayRef<const uint8_t> file_data, const char *filename,
                      HeapArray<GhsPricing> *out_prices);
