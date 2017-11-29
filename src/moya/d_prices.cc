/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "kutil.hh"
#include "d_prices.hh"
#include "d_tables.hh"

bool ParseGhsPricings(Span<const char> file_data, const char *filename,
                      HeapArray<GhsPricing> *out_pricings)
{
    Size start_pricings_len = out_pricings->len;
    DEFER_N(out_guard) { out_pricings->RemoveFrom(start_pricings_len); };

#define FAIL_PARSE_IF(Cond) \
    do { \
        if (Cond) { \
            LogError("Malformed NOEMI (NX) file '%1': %2", \
                     filename ? filename : "?", STRINGIFY(Cond)); \
            return false; \
        } \
    } while (false)

    Span<const char> line = SplitStrLine(file_data, &file_data);
    FAIL_PARSE_IF(line.len != 128);
    FAIL_PARSE_IF(memcmp(line.ptr, "000AM00000001000000TABGHSCT00000001000000GHX000NXGHS", 52) != 0);

    for (; line.len == 128; line = SplitStrLine(file_data, &file_data)) {
        if (!memcmp(line.ptr, "110", 3)) {
            GhsPricing pricing = {};

            unsigned int sector;
            int16_t exh_treshold, exb_treshold;
            int32_t price_cents, exh_cents, exb_cents;
            char type_exb;
#ifdef __GLIBC__
            // Work around glibc trying to scan the whole string for a NUL byte, which makes
            // sscanf() very slow. Eventually I will make my own sscanf() to parse non-C strings.
            char line_buf[129];
            memcpy(line_buf, line.ptr, 128);
            line_buf[128] = 0;
            if (sscanf(line_buf,
#else
            if (sscanf((const char *)line.ptr,
#endif
                       "%*7c%04" SCNd16 "%01u%*3c%03" SCNd16 "%03" SCNd16 "%08" SCNd32 "%*1c%08" SCNd32
                       "%*50c%04" SCNd16 "%02" SCNd8 "%02" SCNd8 "%c%08" SCNd32,
                       &pricing.ghs.number, &sector, &exh_treshold, &exb_treshold, &price_cents, &exh_cents,
                       &pricing.limit_dates[0].st.year, &pricing.limit_dates[0].st.month,
                       &pricing.limit_dates[0].st.day, &type_exb, &exb_cents) != 11) {
                LogError("Malformed NOEMI GHS pricing line (type 110) in '%1'", \
                         filename ? filename : "?"); \
                return false;
            }
            FAIL_PARSE_IF(--sector > 1);
            FAIL_PARSE_IF(!pricing.limit_dates[0].IsValid());

            static const Date default_end_date = ConvertDate1980(UINT16_MAX);
            pricing.limit_dates[1] = default_end_date;

            pricing.sectors[sector].price_cents = price_cents;
            pricing.sectors[sector].exh_treshold = (int16_t)(exh_treshold ? exh_treshold + 1 : 0);
            pricing.sectors[sector].exb_treshold = exb_treshold;
            pricing.sectors[sector].exh_cents = exh_cents;
            pricing.sectors[sector].exb_cents = exb_cents;
            if (type_exb == 'F') {
                pricing.sectors[sector].flags |= (int)GhsPricing::Flag::ExbOnce;
            }

            out_pricings->Append(pricing);
        }
    }
    FAIL_PARSE_IF(line.len != 0);

    {
        Span<GhsPricing> pricings = out_pricings->Take(start_pricings_len,
                                                           out_pricings->len - start_pricings_len);

        std::stable_sort(pricings.begin(), pricings.end(),
                         [](const GhsPricing &pricing1, const GhsPricing &pricing2) {
            return MultiCmp(pricing1.ghs.number - pricing2.ghs.number,
                            pricing1.limit_dates[0] - pricing2.limit_dates[0]) < 0;
        });

        if (pricings.len) {
            Size j = 0;
            for (Size i = 1; i < pricings.len; i++) {
                if (pricings[i].ghs == pricings[j].ghs) {
                    if (pricings[i].limit_dates[0] == pricings[j].limit_dates[0]) {
                        if (pricings[i].sectors[0].price_cents) {
                            pricings[j].sectors[0] = pricings[i].sectors[0];
                        } else if (pricings[i].sectors[1].price_cents) {
                            pricings[j].sectors[1] = pricings[i].sectors[1];
                        }
                    } else {
                        pricings[++j] = pricings[i];

                        pricings[j - 1].limit_dates[1] = pricings[j].limit_dates[0];
                        if (!pricings[j].sectors[0].price_cents) {
                            pricings[j].sectors[0] = pricings[j - 1].sectors[0];
                        }
                        if (!pricings[j].sectors[1].price_cents) {
                            pricings[j].sectors[1] = pricings[j - 1].sectors[1];
                        }
                    }
                } else {
                    pricings[++j] = pricings[i];
                }
            }
            out_pricings->RemoveFrom(start_pricings_len + j + 1);
        }
    }

#undef FAIL_PARSE_IF

    out_guard.disable();
    return true;
}

bool LoadPricingFile(const char *filename, PricingSet *out_set)
{
    Assert(!out_set->ghs_pricings.len);

    Allocator temp_alloc;

    Span<char> file_data;
    if (!ReadFile(&temp_alloc, filename, Megabytes(30), &file_data))
        return false;

    if (!ParseGhsPricings(file_data, filename, &out_set->ghs_pricings))
        return false;
    for (const GhsPricing &pricing: out_set->ghs_pricings) {
        out_set->ghs_pricings_map.Append(&pricing);
    }

    return true;
}

Span<const GhsPricing> PricingSet::FindGhsPricing(GhsCode ghs) const
{
    Span<const GhsPricing> pricings;
    pricings.ptr = ghs_pricings_map.FindValue(ghs, nullptr);
    if (!pricings.ptr)
        return {};

    {
        const GhsPricing *end_pricing = pricings.ptr + 1;
        while (end_pricing < ghs_pricings.end() && end_pricing->ghs == ghs) {
            end_pricing++;
        }
        pricings.len = end_pricing - pricings.ptr;
    }

    return pricings;
}

const GhsPricing *PricingSet::FindGhsPricing(GhsCode ghs, Date date) const
{
    const GhsPricing *pricing = ghs_pricings_map.FindValue(ghs, nullptr);
    if (!pricing)
        return nullptr;

    do {
        if (date >= pricing->limit_dates[0] && date < pricing->limit_dates[1])
            return pricing;
    } while (++pricing < ghs_pricings.end() && pricing->ghs == ghs);

    return nullptr;
}
