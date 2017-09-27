/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "codes.hh"

struct GhsPricing {
    enum class Flag {
        ExbOnce = 1
    };

    GhsCode code;
    Date limit_dates[2];

    struct {
        int32_t price_cents;
        int16_t exh_treshold;
        int16_t exb_treshold;
        int32_t exh_cents;
        int32_t exb_cents;
        uint16_t flags;
    } sectors[2];

    HASH_SET_HANDLER(GhsPricing, code);
};

struct PricingSet {
    HeapArray<GhsPricing> ghs_pricings;
    HashSet<GhsCode, const GhsPricing *> ghs_pricings_map;

    ArrayRef<const GhsPricing> FindGhsPricing(GhsCode ghs_code) const;
    const GhsPricing *FindGhsPricing(GhsCode ghs_code, Date date) const;
};

bool ParseGhsPricings(ArrayRef<const char> file_data, const char *filename,
                      HeapArray<GhsPricing> *out_prices);

bool LoadPricingSet(const char *filename, PricingSet *out_set);
