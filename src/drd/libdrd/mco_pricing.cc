// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "mco_pricing.hh"

namespace RG {

int64_t mco_PriceGhs(const mco_GhsPriceInfo &price_info, double ghs_coefficient,
                     int ghs_duration, bool death, bool ucd,
                     int64_t *out_ghs_cents, int32_t *out_exb_exh)
{
    int ghs_cents = price_info.ghs_cents -
                    4000 * ((price_info.flags & (int)mco_GhsPriceInfo::Flag::Minoration) && ucd);

    int price_cents = ghs_cents;
    int exb_exh;
    if (ghs_duration < price_info.exb_threshold && !death) {
        exb_exh = -(price_info.exb_threshold - ghs_duration);
        if (price_info.flags & (int)mco_GhsPriceInfo::Flag::ExbOnce) {
            price_cents -= price_info.exb_cents;
        } else {
            price_cents += price_info.exb_cents * exb_exh;
        }
    } else if (price_info.exh_threshold && ghs_duration + death >= price_info.exh_threshold) {
        exb_exh = ghs_duration + death + 1 - price_info.exh_threshold;
        price_cents += price_info.exh_cents * exb_exh;
    } else {
        exb_exh = 0;
    }

    if (out_ghs_cents) {
        *out_ghs_cents += (int64_t)(ghs_coefficient * ghs_cents);
    }
    if (out_exb_exh) {
        *out_exb_exh += exb_exh;
    }
    return (int64_t)(ghs_coefficient * price_cents);
}

void mco_Price(const mco_Result &result, bool apply_coefficient, mco_Pricing *out_pricing)
{
    out_pricing->stays = result.stays;

    out_pricing->results_count++;
    out_pricing->stays_count += (int32_t)result.stays.len;
    out_pricing->failures_count += result.ghm.IsError();
    out_pricing->duration += result.duration;
    out_pricing->ghs_duration += result.ghs_duration;

    if (!result.index || result.ghs == mco_GhsCode(9999))
        return;

    const mco_GhsPriceInfo *price_info = result.index->FindGhsPrice(result.ghs, result.sector);
    const mco_SupplementCounters<int32_t> &prices = result.index->SupplementPrices(result.sector);
    double ghs_coefficient = result.index->GhsCoefficient(result.sector);

    out_pricing->ghs_coefficient = ghs_coefficient;
    if (!apply_coefficient)
        ghs_coefficient = 1.0;

    if (price_info) [[likely]] {
        int64_t price_cents = mco_PriceGhs(*price_info, ghs_coefficient, result.ghs_duration,
                                           result.stays[result.stays.len - 1].exit.mode == '9',
                                           result.stays[0].flags & (int)mco_Stay::Flag::UCD,
                                           &out_pricing->ghs_cents, &out_pricing->exb_exh);
        out_pricing->price_cents += price_cents;
        out_pricing->total_cents += price_cents;
    } else {
        LogError("Cannot find price for GHS %1 (%2 -- %3)", result.ghs,
                 result.index->limit_dates[0], result.index->limit_dates[1]);
    }

    out_pricing->supplement_days += result.supplement_days;
    for (Size i = 0; i < RG_LEN(mco_SupplementTypeNames); i++) {
        int32_t supplement_cents = (int32_t)(ghs_coefficient *
                                             (result.supplement_days.values[i] * prices.values[i]));

        out_pricing->supplement_cents.values[i] += supplement_cents;
        out_pricing->total_cents += supplement_cents;
    }
}

void mco_Price(Span<const mco_Result> results, bool apply_coefficient,
               HeapArray<mco_Pricing> *out_pricings)
{
    static const int task_size = 2048;

    const Size start_pricings_len = out_pricings->len;
    out_pricings->Grow(results.len);

    Async async;
    for (Size i = 0; i < results.len; i += task_size) {
        Size task_offset = i;

        async.Run([&, task_offset]() {
            Size end = std::min(results.len, task_offset + task_size);
            MemSet(out_pricings->ptr + start_pricings_len + task_offset, 0,
                       (end - task_offset) * RG_SIZE(*out_pricings->ptr));
            for (Size j = task_offset; j < end; j++) {
                mco_Price(results[j], apply_coefficient, &out_pricings->ptr[start_pricings_len + j]);
            }
            return true;
        });
    }
    async.Sync();

    out_pricings->len += results.len;
}

void mco_PriceTotal(Span<const mco_Result> results, bool apply_coefficient,
                    mco_Pricing *out_pricing)
{
    static const int task_size = 2048;

    HeapArray<mco_Pricing> task_pricings;
    task_pricings.AppendDefault((results.len - 1) / task_size + 1);

    Async async;
    for (Size i = 0; i < task_pricings.len; i++) {
        mco_Pricing *task_pricing = &task_pricings[i];
        Size task_offset = i * task_size;

        async.Run([&, task_offset, task_pricing]() {
            Size end = std::min(results.len, task_offset + task_size);
            for (Size j = task_offset; j < end; j++) {
                mco_Price(results[j], apply_coefficient, task_pricing);
            }
            return true;
        });
    }
    async.Sync();

    mco_Summarize(task_pricings, out_pricing);
}

static double ComputeCoefficients(const mco_Pricing &pricing, Span<const mco_Pricing> mono_pricings,
                                  mco_DispenseMode mode, HeapArray<double> *out_coefficients)
{
    double total = 0;
    for (Size i = 0; i < mono_pricings.len; i++) {
        const mco_Pricing &mono_pricing = mono_pricings[i];
        RG_ASSERT(mono_pricing.stays[0].bill_id == pricing.stays[0].bill_id);

        double coefficient = -1;
        switch (mode) {
            case mco_DispenseMode::E: {
                coefficient = (double)mono_pricing.ghs_cents;
            } break;

            case mco_DispenseMode::Ex: {
                coefficient = (double)mono_pricing.price_cents;
            } break;

            case mco_DispenseMode::Ex2: {
                if (pricing.exb_exh < 0) {
                    coefficient = (double)mono_pricing.price_cents;
                } else {
                    coefficient = (double)mono_pricing.ghs_cents;
                }
            } break;

            case mco_DispenseMode::J: {
                coefficient = std::max((int)mono_pricing.duration, 1);
            } break;

            case mco_DispenseMode::ExJ: {
                coefficient = std::max((int)mono_pricing.duration, 1) * (double)mono_pricing.price_cents;
            } break;

            case mco_DispenseMode::ExJ2: {
                if (pricing.exb_exh < 0) {
                    coefficient = std::max((int)mono_pricing.duration, 1) * (double)mono_pricing.price_cents;
                } else {
                    coefficient = std::max((int)mono_pricing.duration, 1) * (double)mono_pricing.ghs_cents;
                }
            } break;
        }

        out_coefficients->Append(coefficient);
        total += coefficient;
    }

    return total;
}

void mco_Dispense(Span<const mco_Pricing> pricings, Span<const mco_Pricing> mono_pricings,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Pricing> *out_mono_pricings)
{
    RG_ASSERT(mono_pricings.len >= pricings.len);

    static const int task_size = 2048;

    // Cannot append (parallel algorithm)
    out_mono_pricings->Grow(mono_pricings.len);

    Async async;
    Size i = 0, j = 0;
    while (i < pricings.len) {
        Size task_offset = i;
        Size task_mono_offset = j;

        for (Size k = 0; k < task_size && i < pricings.len; i++, k++) {
            j += pricings[i].stays_count;
        }

        async.Run([&, task_offset, task_mono_offset]() {
            // Reuse for performance
            HeapArray<double> coefficients;

            Size end = std::min(pricings.len, task_offset + task_size);
            Size j = task_mono_offset;
            for (Size i = task_offset; i < end;) {
                const mco_Pricing &pricing = pricings[i];
                Span<const mco_Pricing> sub_mono_pricings = mono_pricings.Take(j, pricing.stays_count);

                coefficients.RemoveFrom(0);
                double coefficients_total = ComputeCoefficients(pricing, sub_mono_pricings,
                                                                dispense_mode, &coefficients);

                if (!coefficients_total) [[unlikely]] {
                    coefficients.RemoveFrom(0);
                    coefficients_total = ComputeCoefficients(pricing, sub_mono_pricings,
                                                             mco_DispenseMode::J, &coefficients);
                }

                int64_t total_ghs_cents = 0;
                int64_t total_price_cents = 0;
                for (Size k = 0; k < coefficients.len; k++) {
                    mco_Pricing mono_pricing = sub_mono_pricings[k];
                    double fraction = coefficients[k] / coefficients_total;

                    // DIP rules are special
                    if (pricing.supplement_cents.st.dip) {
                        double dip_fraction = (double)(mono_pricing.duration + 1) /
                                                      (pricing.duration + coefficients.len);
                        int64_t mono_dip_cents = (int64_t)round(pricing.supplement_cents.st.dip * dip_fraction);
                        mono_pricing.total_cents += mono_dip_cents - mono_pricing.supplement_cents.st.dip;
                        mono_pricing.supplement_cents.st.dip = mono_dip_cents;
                    }

                    {
                        int64_t ghs_cents = (int64_t)round(pricing.ghs_cents * fraction);
                        int64_t price_cents = (int64_t)round(pricing.price_cents * fraction);
                        int64_t supplement_cents = mono_pricing.total_cents - mono_pricing.price_cents;

                        mono_pricing.ghs_cents = ghs_cents;
                        mono_pricing.price_cents = price_cents;
                        mono_pricing.total_cents = price_cents + supplement_cents;
                    }

                    total_ghs_cents += mono_pricing.ghs_cents;
                    total_price_cents += mono_pricing.price_cents;

                    out_mono_pricings->ptr[j + k] = mono_pricing;
                }

                // Attribute missing cents to last stay (rounding errors)
                mco_Pricing *last_mono_pricing = &out_mono_pricings->ptr[j + coefficients.len - 1];
                last_mono_pricing->ghs_cents += pricing.ghs_cents - total_ghs_cents;
                last_mono_pricing->price_cents += pricing.price_cents - total_price_cents;
                last_mono_pricing->total_cents += pricing.price_cents - total_price_cents;

                i++;
                j += pricing.stays_count;
            }

            return true;
        });
    }
    async.Sync();

    out_mono_pricings->len += mono_pricings.len;
}

void mco_Dispense(Span<const mco_Pricing> pricings, Span<const mco_Result> mono_results,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Pricing> *out_mono_pricings)
{
    // First, calculate naive mono-stay prices, which we will use as coefficients (for
    // some modes at least) below.
    Size mono_pricings_start_len = out_mono_pricings->len;
    mco_Price(mono_results, false, out_mono_pricings);

    Span<const mco_Pricing> mono_pricings =
        out_mono_pricings->Take(mono_pricings_start_len, out_mono_pricings->len - mono_pricings_start_len);

    out_mono_pricings->len = mono_pricings_start_len;
    mco_Dispense(pricings, mono_pricings, dispense_mode, out_mono_pricings);
}

}
