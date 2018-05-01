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
    enum class DurationMode {
        Disabled,
        Partial,
        Full
    };

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
    DurationMode duration_mode = DurationMode::Disabled;
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

        const char *durations_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "duration_mode");
        if (durations_str && durations_str[0]) {
            if (TestStr(durations_str, "partial")) {
                duration_mode = DurationMode::Partial;
            } else if (TestStr(durations_str, "full")) {
                duration_mode = DurationMode::Full;
            } else if (!TestStr(durations_str, "0")) {
                LogError("Invalid 'duration_mode' parameter value '%1'", durations_str);
                return CreateErrorPage(422);
            }
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
    mco_ClassifyParallel(*drdw_table_set, *drdw_authorization_set, drdw_stay_set.stays, 0, &results);

    HeapArray<CellSummary> summary;
    {
        HashMap<int64_t, Size> summary_map;
        for (const mco_Result &result: results) {
            if (result.ghm.IsError())
                continue;

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

            int full_duration = 0;
            for (const mco_Stay &stay: result.stays) {
                if (stay.exit.date != stay.entry.date) {
                    full_duration += stay.exit.date - stay.entry.date;
                } else {
                    full_duration++;
                }
            }

            int result_total = 0;
            for (Size i = 0; i < result.stays.len; i++) {
                const mco_Stay &stay = result.stays[i];

                if (units.table.count && !units.Find(stay.unit))
                    continue;

                int stay_duration = stay.exit.date - stay.entry.date;
                int stay_ghs_cents = result.ghs_price_cents * (stay_duration ? stay_duration : 1) / full_duration;
                result_total += stay_ghs_cents;
                // FIXME: Hack for rounding errors
                if (i == result.stays.len - 1) {
                    stay_ghs_cents += result.ghs_price_cents - result_total;
                }

                // TODO: Careful with duration overflow
                SummaryMapKey key;
                key.st.ghm = result.ghm;
                key.st.ghs = result.ghs;
                switch (duration_mode) {
                    case DurationMode::Disabled: { key.st.duration = 0; } break;
                    case DurationMode::Partial: { key.st.duration = (int16_t)stay_duration; } break;
                    case DurationMode::Full: { key.st.duration = (int16_t)result.duration; } break;
                }

                std::pair<int64_t *, bool> ret = summary_map.Append(key.value, summary.len);
                if (ret.second) {
                    CellSummary cell_summary = {};
                    cell_summary.ghm = result.ghm;
                    cell_summary.ghs = result.ghs.number;
                    cell_summary.duration = key.st.duration;
                    summary.Append(cell_summary);
                }
                summary[*ret.first].count += multiplier;
                summary[*ret.first].ghs_price_cents += multiplier * stay_ghs_cents;
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
            if (duration_mode != DurationMode::Disabled) {
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
