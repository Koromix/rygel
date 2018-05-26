// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "mco_dispenser.hh"

struct DispenseCoefficient {
    UnitCode unit;
    double value;
};

void mco_Summarize(Span<const mco_Result> results, mco_Summary *out_summary)
{
    out_summary->results_count += results.len;
    for (const mco_Result &result: results) {
        out_summary->stays_count += result.stays.len;
        out_summary->failures_count += result.ghm.IsError();
        out_summary->ghs_cents += result.ghs_pricing.ghs_cents;
        out_summary->price_cents += result.ghs_pricing.price_cents;
        out_summary->supplement_days += result.supplement_days;
        out_summary->supplement_cents += result.supplement_cents;
        out_summary->total_cents += result.total_cents;
    }
}

static double ComputeCoefficients(mco_DispenseMode dispense_mode,
                                  const mco_Result &result, Span<const mco_Result> mono_results,
                                  HeapArray<DispenseCoefficient> *out_coefficients)
{
    double coefficients_total = 0.0;
    for (const mco_Result &mono_result: mono_results) {
        DebugAssert(mono_result.stays[0].bill_id == result.stays[0].bill_id);

        DispenseCoefficient coefficient = {};

        coefficient.unit = mono_result.stays[0].unit;

        switch (dispense_mode) {
            case mco_DispenseMode::E: {
                coefficient.value = (double)mono_result.ghs_pricing.ghs_cents;
            } break;

            case mco_DispenseMode::Ex: {
                coefficient.value = (double)mono_result.ghs_pricing.price_cents;
            } break;

            case mco_DispenseMode::Ex2: {
                if (result.ghs_pricing.exb_exh < 0) {
                    coefficient.value = (double)mono_result.ghs_pricing.price_cents;
                } else {
                    coefficient.value = (double)mono_result.ghs_pricing.ghs_cents;
                }
            } break;

            case mco_DispenseMode::J: {
                coefficient.value = (double)std::max(mono_result.duration, 1);
            } break;

            case mco_DispenseMode::ExJ: {
                coefficient.value = (double)std::max(mono_result.duration, 1) *
                                            mono_result.ghs_pricing.price_cents;
            } break;

            case mco_DispenseMode::ExJ2: {
                if (result.ghs_pricing.exb_exh < 0) {
                    coefficient.value = (double)std::max(mono_result.duration, 1) *
                                                mono_result.ghs_pricing.price_cents;
                } else {
                    coefficient.value = (double)std::max(mono_result.duration, 1) *
                                                mono_result.ghs_pricing.ghs_cents;
                }
            } break;
        }

        out_coefficients->Append(coefficient);
        coefficients_total += coefficient.value;
    }

    return coefficients_total;
}

void mco_Dispense(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Due> *out_dues,
                  HashMap<UnitCode, Size> *out_dues_map)
{
    DebugAssert(mono_results.len >= results.len);

    // Reuse for performance
    HeapArray<DispenseCoefficient> coefficients;

    Size j = 0;
    for (const mco_Result &result: results) {
        Span<const mco_Result> sub_mono_results = mono_results.Take(j, result.stays.len);
        j += result.stays.len;

        coefficients.Clear(64);
        double coefficients_total = ComputeCoefficients(dispense_mode, result,
                                                        sub_mono_results, &coefficients);

        if (UNLIKELY(coefficients_total == 0.0)) {
            coefficients.RemoveFrom(0);
            coefficients_total = ComputeCoefficients(mco_DispenseMode::J, result,
                                                     sub_mono_results, &coefficients);
        }

        mco_Due *due = nullptr;
        int64_t total_ghs_cents = 0;
        int64_t total_price_cents = 0;
        for (Size k = 0; k < coefficients.len; k++) {
            const DispenseCoefficient &unit_coefficient = coefficients[k];
            const mco_Result &mono_result = sub_mono_results[k];

            double coefficient = unit_coefficient.value / coefficients_total;
            int64_t ghs_cents = (int64_t)((double)result.ghs_pricing.ghs_cents * coefficient);
            int64_t price_cents = (int64_t)((double)result.ghs_pricing.price_cents * coefficient);

            {
                std::pair<Size *, bool> ret =
                    out_dues_map->AppendUninitialized(unit_coefficient.unit);
                if (ret.second) {
                    *ret.first = out_dues->len;
                    due = out_dues->AppendDefault();
                    due->unit = unit_coefficient.unit;
                } else {
                    due = &(*out_dues)[*ret.first];
                }
            }

            due->summary.ghs_cents += ghs_cents;
            due->summary.price_cents += price_cents;
            due->summary.supplement_cents += mono_result.supplement_cents;
            due->summary.supplement_days += mono_result.supplement_days;
            due->summary.total_cents += price_cents + (mono_result.total_cents -
                                                       mono_result.ghs_pricing.price_cents);

            total_ghs_cents += ghs_cents;
            total_price_cents += price_cents;
        }

        // Attribute missing cents to last stay (rounding errors)
        due->summary.ghs_cents += result.ghs_pricing.ghs_cents - total_ghs_cents;
        due->summary.price_cents += result.ghs_pricing.price_cents - total_price_cents;
        due->summary.total_cents += result.ghs_pricing.price_cents - total_price_cents;
    }
}

void mco_Dispense(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Due> *out_dues)
{
    HashMap<UnitCode, Size> dues_map;
    for (Size i = 0; i < out_dues->len; i++) {
        dues_map.Append((*out_dues)[i].unit, i);
    }

    mco_Dispense(results, mono_results, dispense_mode, out_dues, &dues_map);
}
