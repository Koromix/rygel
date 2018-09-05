// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "drdw.hh"

static bool ParseDateRange(Span<const char> date_str, Date *out_start_date, Date *out_end_date)
{
    Date start_date = {};
    Date end_date = {};

    if (date_str.len) {
        Span<const char> str = date_str;
        start_date = Date::FromString(str, 0, &str);
        if (str.len < 2 || str[0] != '.' || str[1] != '.')
            goto invalid;
        end_date = Date::FromString(str.Take(2, str.len - 2), 0, &str);
        if (str.len)
            goto invalid;
        if (!start_date.IsValid() || !end_date.IsValid() || end_date <= start_date)
            goto invalid;
    }

    *out_start_date = start_date;
    *out_end_date = end_date;
    return true;

invalid:
    LogError("Invalid date range '%1'", date_str);
    return false;
}

Response ProduceCaseMix(MHD_Connection *conn, const char *, CompressionType compression_type)
{
    struct CellSummary {
        mco_GhmCode ghm;
        int16_t ghs;
        int16_t duration;
        int count;
        int64_t ghs_price_cents;
    };

    union SummaryMapKey {
        struct {
            mco_GhmCode ghm;
            mco_GhsCode ghs;
            int16_t duration;
        } st;
        int64_t value;
        StaticAssert(SIZE(SummaryMapKey::value) == SIZE(SummaryMapKey::st));
    };

    Date dates[2] = {};
    Date diff_dates[2] = {};
    HashSet<UnitCode> units;
    bool durations = false;
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    {
        if (!ParseDateRange(MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "dates"),
                            &dates[0], &dates[1]))
            return CreateErrorPage(422);
        if (!ParseDateRange(MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "diff"),
                            &diff_dates[0], &diff_dates[1]))
            return CreateErrorPage(422);

        Span<const char> units_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "units");
        while (units_str.len) {
            Span<const char> unit_str = SplitStrAny(units_str, " ,+", &units_str);

            UnitCode unit;
            unit.number = ParseDec<int16_t>(unit_str).first;
            if (!unit.IsValid())
                return CreateErrorPage(422);

            units.Append(unit);
        }

        const char *durations_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "durations");
        if (durations_str && durations_str[0]) {
            if (TestStr(durations_str, "1")) {
                durations = true;
            } else if (TestStr(durations_str, "0")) {
                durations = false;
            } else {
                LogError("Invalid 'durations' parameter value '%1'", durations_str);
                return CreateErrorPage(422);
            }
        }

        const char *mode_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "mode");
        if (mode_str && mode_str[0]) {
            const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                                  std::end(mco_DispenseModeOptions),
                                                  [&](const OptionDesc &desc) { return TestStr(desc.name, mode_str); });
            if (desc == std::end(mco_DispenseModeOptions)) {
                LogError("Invalid 'mode' parameter value '%1'", mode_str);
                return CreateErrorPage(422);
            }
            dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
        }
    }

    if (diff_dates[0].value && !dates[0].value) {
        LogError("Parameter 'diff' specified but 'dates' is missing");
        return CreateErrorPage(422);
    }
    if (dates[0].value && diff_dates[0].value &&
               dates[0] < diff_dates[1] && dates[1] > diff_dates[0]) {
        LogError("Parameters 'dates' and 'diff' must not overlap");
        return CreateErrorPage(422);
    }

    HeapArray<mco_Result> results;
    HeapArray<mco_Result> mono_results;
    mco_Classify(*drdw_table_set, *drdw_authorization_set, drdw_stay_set.stays,
                 (int)mco_ClassifyFlag::Mono, &results, &mono_results);

    HeapArray<mco_Pricing> pricings;
    HeapArray<mco_Pricing> mono_pricings;
    mco_Price(results, false, &pricings);
    mco_Dispense(pricings, mono_results, dispense_mode, &mono_pricings);

    HeapArray<CellSummary> summary;
    {
        Size j = 0;
        HashMap<int64_t, Size> summary_map;
        for (const mco_Result &result: results) {
            Span<const mco_Result> sub_mono_results = mono_results.Take(j, result.stays.len);
            Span<const mco_Pricing> sub_mono_pricings = mono_pricings.Take(j, result.stays.len);
            j += result.stays.len;

            int multiplier;
            if (!dates[0].value ||
                    (result.stays[result.stays.len - 1].exit.date >= dates[0] &&
                     result.stays[result.stays.len - 1].exit.date < dates[1])) {
                multiplier = 1;
            } else if (diff_dates[0].value &&
                       result.stays[result.stays.len - 1].exit.date >= diff_dates[0] &&
                       result.stays[result.stays.len - 1].exit.date < diff_dates[1]) {
                multiplier = -1;
            } else {
                continue;
            }

            bool counted_rss = false;
            for (Size k = 0; k < sub_mono_results.len; k++) {
                const mco_Result &mono_result = sub_mono_results[k];
                const mco_Pricing &mono_pricing = sub_mono_pricings[k];
                DebugAssert(mono_result.stays[0].bill_id == result.stays[0].bill_id);

                if (!units.table.count || units.Find(mono_result.stays[0].unit)) {
                    // TODO: Careful with duration overflow
                    SummaryMapKey key = {};
                    key.st.ghm = result.ghm;
                    key.st.ghs = result.ghs;
                    if (durations) {
                        key.st.duration = (int16_t)result.duration;
                    }

                    std::pair<Size *, bool> ret = summary_map.Append(key.value, summary.len);
                    if (ret.second) {
                        CellSummary cell_summary = {};
                        cell_summary.ghm = result.ghm;
                        cell_summary.ghs = result.ghs.number;
                        cell_summary.duration = key.st.duration;
                        summary.Append(cell_summary);
                    }

                    if (!counted_rss) {
                        summary[*ret.first].count += multiplier;
                        counted_rss = true;
                    }
                    summary[*ret.first].ghs_price_cents += multiplier * mono_pricing.price_cents;
                }
            }
        }
    }

    std::sort(summary.begin(), summary.end(), [](const CellSummary &cs1, const CellSummary &cs2) {
        return MultiCmp(cs1.ghm.value - cs2.ghm.value,
                        cs1.ghs - cs2.ghs,
                        cs1.duration - cs2.duration) < 0;
    });

    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        writer.StartArray();
        for (const CellSummary &cs: summary) {
            char buf[32];

            writer.StartObject();
            writer.Key("ghm"); writer.String(Fmt(buf, "%1", cs.ghm).ptr);
            writer.Key("ghs"); writer.Int(cs.ghs);
            if (durations) {
                writer.Key("duration"); writer.Int(cs.duration);
            }
            writer.Key("stays_count"); writer.Int(cs.count);
            writer.Key("ghs_price_cents"); writer.Int64(cs.ghs_price_cents);
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    });

    return {200, response};
}
