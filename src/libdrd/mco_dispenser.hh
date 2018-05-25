// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_classifier.hh"

struct mco_Summary {
    Size results_count = 0;
    Size stays_count = 0;
    Size failures_count = 0;

    int64_t ghs_cents = 0;
    int64_t price_cents = 0;
    mco_SupplementCounters<int32_t> supplement_days;
    mco_SupplementCounters<int64_t> supplement_cents;
    int64_t total_cents = 0;

    mco_Summary &operator+=(const mco_Summary &other)
    {
        results_count += other.results_count;
        stays_count += other.stays_count;
        failures_count += other.failures_count;

        ghs_cents += other.ghs_cents;
        price_cents += other.price_cents;
        supplement_days += other.supplement_days;
        supplement_cents += other.supplement_cents;
        total_cents += other.total_cents;

        return *this;
    }
    mco_Summary operator+(const mco_Summary &other)
    {
        mco_Summary copy = *this;
        copy += other;
        return copy;
    }
};

enum class mco_DispenseMode {
    E,
    Ex,
    Ex2,
    J,
    ExJ,
    ExJ2
};
static const struct OptionDesc mco_DispenseModeOptions[] = {
    {"e", "E"},
    {"ex", "Ex"},
    {"ex2", "Ex'"},
    {"j", "J"},
    {"exj", "ExJ"},
    {"exj2", "Ex'J"}
};

struct mco_Due {
    UnitCode unit;
    mco_Summary summary;
};

void mco_Summarize(Span<const mco_Result> results, mco_Summary *out_summary);

void mco_Dispense(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Due> *out_dues,
                  HashMap<UnitCode, Size> *out_dues_map);
void mco_Dispense(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Due> *out_dues);
