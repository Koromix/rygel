// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "drdw.hh"

static bool CheckUnitAgainstUser(const User &user, const Unit &unit)
{
    const auto CheckNeedle = [&](const char *needle) {
        return !!strstr(unit.path, needle);
    };

    if (user.allow_default) {
        bool deny = std::any_of(user.deny.begin(), user.deny.end(), CheckNeedle);
        if (deny) {
            bool allow = std::any_of(user.allow.begin(), user.allow.end(), CheckNeedle);
            if (!allow)
                return false;
        }
    } else {
        bool allow = std::any_of(user.allow.begin(), user.allow.end(), CheckNeedle);
        if (!allow)
            return false;
        bool deny = std::any_of(user.deny.begin(), user.deny.end(), CheckNeedle);
        if (deny)
            return false;
    }

    return true;
}

static bool CheckDispenseModeAgainstUser(const User &user, mco_DispenseMode dispense_mode)
{
    return dispense_mode == drdw_structure_set.dispense_mode ||
           (user.dispense_modes & (1 << (int)dispense_mode));
}

Response ProduceCaseMix(const ConnectionInfo *conn, const char *, CompressionType compression_type)
{
    if (!drdw_stay_set.stays.len || !conn->user)
        return CreateErrorPage(404);

    // TODO: Cache in session object (also neeeded in ProduceClassify)?
    HashSet<UnitCode> allowed_units;
    for (const Structure &structure: drdw_structure_set.structures) {
        for (const Unit &unit: structure.units) {
            if (CheckUnitAgainstUser(*conn->user, unit))
                allowed_units.Append(unit.unit);
        }
    }

    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[32];

        writer.StartObject();

        writer.Key("begin_date"); writer.String(Fmt(buf, "%1", drdw_stay_set_dates[0]).ptr);
        writer.Key("end_date"); writer.String(Fmt(buf, "%1", drdw_stay_set_dates[1]).ptr);

        // Algorithms
        {
            const OptionDesc &default_desc = mco_DispenseModeOptions[(int)drdw_structure_set.dispense_mode];

            writer.Key("algorithms"); writer.StartArray();
            for (Size i = 0; i < ARRAY_SIZE(mco_DispenseModeOptions); i++) {
                if (CheckDispenseModeAgainstUser(*conn->user, (mco_DispenseMode)i)) {
                    const OptionDesc &desc = mco_DispenseModeOptions[i];

                    writer.StartObject();
                    writer.Key("name"); writer.String(desc.name);
                    writer.Key("title"); writer.String(desc.help);
                    writer.EndObject();
                }
            }
            writer.EndArray();

            writer.Key("default_algorithm"); writer.String(default_desc.name);
        }

        writer.Key("structures"); writer.StartArray();
        for (const Structure &structure: drdw_structure_set.structures) {
            writer.StartObject();
            writer.Key("name"); writer.String(structure.name);
            writer.Key("units"); writer.StartArray();
            for (const Unit &unit: structure.units) {
                if (allowed_units.Find(unit.unit)) {
                    writer.StartObject();
                    writer.Key("unit"); writer.Int(unit.unit.number);
                    writer.Key("path"); writer.String(unit.path);
                    writer.EndObject();
                }
            }
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndArray();

        writer.EndObject();

        return true;
    });

    return {200, response};
}

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

Response ProduceClassify(const ConnectionInfo *conn, const char *, CompressionType compression_type)
{
    if (!drdw_stay_set.stays.len || !conn->user)
        return CreateErrorPage(404);

    struct CellSummary {
        mco_GhmCode ghm;
        int16_t ghs;
        int16_t duration;
        int count;
        int mono_count;
        int64_t price_cents;
        int deaths;
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

    HashSet<UnitCode> allowed_units;
    for (const Structure &structure: drdw_structure_set.structures) {
        for (const Unit &unit: structure.units) {
            if (CheckUnitAgainstUser(*conn->user, unit))
                allowed_units.Append(unit.unit);
        }
    }

    Date dates[2] = {};
    Date diff_dates[2] = {};
    HashSet<UnitCode> units;
    bool durations = false;
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    {
        if (!ParseDateRange(MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "dates"),
                            &dates[0], &dates[1]))
            return CreateErrorPage(422);
        if (!ParseDateRange(MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "diff"),
                            &diff_dates[0], &diff_dates[1]))
            return CreateErrorPage(422);

        Span<const char> units_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "units");
        while (units_str.len) {
            Span<const char> part = SplitStrAny(units_str, " ,+", &units_str);

            if (part.len) {
                UnitCode unit;
                unit.number = ParseDec<int16_t>(part).first;
                if (!unit.IsValid())
                    return CreateErrorPage(422);

                if (allowed_units.Find(unit)) {
                    units.Append(unit);
                }
            }
        }

        const char *durations_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "durations");
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

        const char *mode_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "mode");
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

    // FIXME: Explicit reject if non-allowed units are present
    if (diff_dates[0].value && !dates[0].value) {
        LogError("Parameter 'diff' specified but 'dates' is missing");
        return CreateErrorPage(422);
    }
    if (dates[0].value && diff_dates[0].value &&
               dates[0] < diff_dates[1] && dates[1] > diff_dates[0]) {
        LogError("Parameters 'dates' and 'diff' must not overlap");
        return CreateErrorPage(422);
    }
    if (!CheckDispenseModeAgainstUser(*conn->user, dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
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

                if (units.Find(mono_result.stays[0].unit)) {
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
                        if (result.stays[result.stays.len - 1].exit.mode == '9') {
                            summary[*ret.first].deaths += multiplier;
                        }
                        counted_rss = true;
                    }
                    summary[*ret.first].mono_count += multiplier;
                    summary[*ret.first].price_cents += multiplier * mono_pricing.price_cents;
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
            writer.Key("count"); writer.Int(cs.count);
            writer.Key("mono_count"); writer.Int(cs.mono_count);
            writer.Key("deaths"); writer.Int64(cs.deaths);
            writer.Key("price_cents"); writer.Int64(cs.price_cents);
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    });

    return {200, response};
}
