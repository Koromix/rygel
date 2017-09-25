/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "kutil.hh"
#include "pricing.hh"
#include "tables.hh"

static ArrayRef<const uint8_t> GetLine(ArrayRef<const uint8_t> data,
                                       ArrayRef<const uint8_t> *out_remainder = nullptr)
{
    size_t line_len = 0, line_end = 0;
    while (line_end < data.len && data[line_end++] != '\n') {
        line_len++;
    }
    if (line_len && data[line_len - 1] == '\r') {
        line_len--;
    }
    if (out_remainder) {
        *out_remainder = data.Take(line_end, data.len - line_end);
    }
    return data.Take(0, line_len);
}

bool ParseGhsPricings(ArrayRef<const uint8_t> file_data, const char *filename,
                      HeapArray<GhsPricing> *out_pricings)
{
    size_t start_pricings_len = out_pricings->len;
    DEFER_N(out_guard) { out_pricings->RemoveFrom(start_pricings_len); };

#define FAIL_PARSE_IF(Cond) \
    do { \
        if (Cond) { \
            LogError("Malformed NOEMI (NX) file '%1': %2", \
                     filename ? filename : "?", STRINGIFY(Cond)); \
            return false; \
        } \
    } while (false)

    ArrayRef<const uint8_t> line = GetLine(file_data, &file_data);
    FAIL_PARSE_IF(line.len != 128);
    FAIL_PARSE_IF(memcmp(line.ptr, "000AM00000001000000TABGHSCT00000001000000GHX000NXGHS", 52) != 0);

    for (; line.len == 128; line = GetLine(file_data, &file_data)) {
        if (!memcmp(line.ptr, "110", 3)) {
            GhsPricing pricing = {};

            unsigned int sector;
            int32_t price, exh, exb;
            if (sscanf((const char *)line.ptr,
                       "%*7c%04" SCNd16 "%01u%*9c%08" SCNd32 "%*1c%08" SCNd32 "%*50c%04" SCNd16 "%02" SCNd8 "%02" SCNd8 "%*1c%08" SCNd32,
                       &pricing.ghs_code.number, &sector, &price, &exh, &pricing.limit_dates[0].st.year,
                       &pricing.limit_dates[0].st.month, &pricing.limit_dates[0].st.day, &exb) != 8) {
                LogError("Malformed NOEMI GHS pricing line (type 110) in '%1': %2", \
                         filename ? filename : "?", STRINGIFY(Cond)); \
                return false;
            }
            FAIL_PARSE_IF(--sector > 1);
            FAIL_PARSE_IF(!pricing.limit_dates[0].IsValid());

            static const Date default_end_date = ConvertDate1980(UINT16_MAX);
            pricing.limit_dates[1] = default_end_date;

            pricing.sectors[sector].price_cents = price;
            pricing.sectors[sector].exh_cents = exh;
            pricing.sectors[sector].exb_cents = exb;

            out_pricings->Append(pricing);
        }
    }
    FAIL_PARSE_IF(line.len != 0);

    {
        ArrayRef<GhsPricing> pricings = out_pricings->Take(start_pricings_len,
                                                           out_pricings->len - start_pricings_len);

        std::stable_sort(pricings.begin(), pricings.end(),
                         [](const GhsPricing &pricing1, const GhsPricing &pricing2) {
            return MultiCmp(pricing1.ghs_code.number - pricing2.ghs_code.number,
                            pricing1.limit_dates[0] - pricing2.limit_dates[0]) < 0;
        });

        size_t j = 0;
        for (size_t i = 1; i < pricings.len; i++) {
            if (pricings[i].ghs_code == pricings[j].ghs_code) {
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
        out_pricings->RemoveFrom(start_pricings_len + j);
    }

#undef FAIL_PARSE_IF

    out_guard.disable();
    return true;
}
