// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "mco_classifier.hh"

namespace K {

struct mco_Pricing {
    Span<const mco_Stay> stays; // Not valid in totals / summaries

    int32_t results_count;
    int32_t stays_count;
    int32_t failures_count;
    int32_t duration;
    int32_t ghs_duration;

    double ghs_coefficient; // Not valid in totals / summaries
    int64_t ghs_cents;
    int64_t price_cents;
    int32_t exb_exh;
    mco_SupplementCounters<int32_t> supplement_days;
    mco_SupplementCounters<int64_t> supplement_cents;
    int64_t total_cents;

    mco_Pricing &operator+=(const mco_Pricing &other)
    {
        results_count += other.results_count;
        stays_count += other.stays_count;
        failures_count += other.failures_count;
        duration += other.duration;
        ghs_duration += other.ghs_duration;

        ghs_cents += other.ghs_cents;
        price_cents += other.price_cents;
        supplement_days += other.supplement_days;
        supplement_cents += other.supplement_cents;
        total_cents += other.total_cents;

        return *this;
    }
    mco_Pricing operator+(const mco_Pricing &other)
    {
        mco_Pricing copy = *this;
        copy += other;
        return copy;
    }

    void ApplyCoefficient()
    {
        K_ASSERT(!std::isnan(ghs_coefficient));

        ghs_cents = (int64_t)(ghs_coefficient * ghs_cents);
        price_cents = (int64_t)(ghs_coefficient * price_cents);
        for (Size i = 0; i < K_LEN(mco_SupplementTypeNames); i++) {
            supplement_cents.values[i] = (int64_t)(ghs_coefficient * supplement_cents.values[i]);
        }
        total_cents = (int64_t)(ghs_coefficient * total_cents);
    }

    mco_Pricing WithCoefficient() const
    {
        K_ASSERT(!std::isnan(ghs_coefficient));

        mco_Pricing pricing_coeff = *this;
        pricing_coeff.ApplyCoefficient();

        return pricing_coeff;
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
    {"E", "E"},
    {"Ex", "Ex"},
    {"Ex'", "Ex'"},
    {"J", "J"},
    {"ExJ", "ExJ"},
    {"Ex'J", "Ex'J"}
};

int64_t mco_PriceGhs(const mco_GhsPriceInfo &price_info, double ghs_coefficient,
                     int ghs_duration, bool death, bool ucd,
                     int64_t *out_ghs_cents = nullptr, int32_t *out_exb_exh = nullptr);

void mco_Price(const mco_Result &result, bool apply_coefficient, mco_Pricing *out_pricing);
void mco_Price(Span<const mco_Result> results, bool apply_coefficient,
               HeapArray<mco_Pricing> *out_pricings);
void mco_PriceTotal(Span<const mco_Result> results, bool apply_coefficient,
                    mco_Pricing *out_pricing);

static inline void mco_Summarize(Span<const mco_Pricing> pricings, mco_Pricing *out_summary)
{
    for (const mco_Pricing &pricing: pricings) {
        *out_summary += pricing;
    }

    out_summary->stays = {};
    out_summary->ghs_coefficient = NAN;
}

void mco_Dispense(Span<const mco_Pricing> pricings, Span<const mco_Pricing> mono_pricings,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Pricing> *out_pricings);
void mco_Dispense(Span<const mco_Pricing> pricings, Span<const mco_Result> mono_results,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Pricing> *out_pricings);

}
