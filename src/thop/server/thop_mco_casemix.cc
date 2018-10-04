// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "thop.hh"
#include "thop_mco.hh"

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
    return dispense_mode == thop_structure_set.dispense_mode ||
           (user.dispense_modes & (1 << (int)dispense_mode));
}

int ProduceMcoSettings(const ConnectionInfo *conn, const char *, Response *out_response)
{
    if (!thop_stay_set.stays.len || !conn->user)
        return CreateErrorPage(404, out_response);

    // TODO: Cache in session object (also neeeded in ProduceClassify)?
    HashSet<UnitCode> allowed_units;
    for (const Structure &structure: thop_structure_set.structures) {
        for (const Unit &unit: structure.units) {
            if (CheckUnitAgainstUser(*conn->user, unit))
                allowed_units.Append(unit.unit);
        }
    }

    out_response->flags |= (int)Response::Flag::DisableETag;
    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[32];

        writer.StartObject();

        writer.Key("begin_date"); writer.String(Fmt(buf, "%1", thop_stay_set_dates[0]).ptr);
        writer.Key("end_date"); writer.String(Fmt(buf, "%1", thop_stay_set_dates[1]).ptr);

        // Algorithms
        {
            const OptionDesc &default_desc = mco_DispenseModeOptions[(int)thop_structure_set.dispense_mode];

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
        for (const Structure &structure: thop_structure_set.structures) {
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
    }, conn->compression_type, out_response);
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

struct AggregateKey {
    mco_GhmCode ghm;
    mco_GhsCode ghs;
    int duration;
} key;

static inline uint64_t DefaultHash(const AggregateKey &key)
{
    return DefaultHash(key.ghm) ^
           DefaultHash(key.ghs) ^
           DefaultHash(key.duration);
}

static inline bool DefaultCompare(const AggregateKey &key1, const AggregateKey &key2)
{
    return key1.ghm == key2.ghm &&
           key1.ghs == key2.ghs &&
           key1.duration == key2.duration;
}

struct AggregateStatistics {
    AggregateKey key;

    int count;
    int partial_mono_count;
    int mono_count;
    int64_t partial_price_cents;
    int64_t price_cents;
    int deaths;

    HASH_TABLE_HANDLER(AggregateStatistics, key);
};

int ProduceMcoCasemix(const ConnectionInfo *conn, const char *, Response *out_response)
{
    if (!thop_stay_set.stays.len || !conn->user)
        return CreateErrorPage(404, out_response);

    HashSet<UnitCode> allowed_units;
    for (const Structure &structure: thop_structure_set.structures) {
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
            return CreateErrorPage(422, out_response);
        if (!ParseDateRange(MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "diff"),
                            &diff_dates[0], &diff_dates[1]))
            return CreateErrorPage(422, out_response);

        Span<const char> units_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "units");
        while (units_str.len) {
            Span<const char> part = SplitStrAny(units_str, " ,+", &units_str);

            if (part.len) {
                UnitCode unit;
                unit.number = ParseDec<int16_t>(part).first;
                if (!unit.IsValid())
                    return CreateErrorPage(422, out_response);

                units.Append(unit);
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
                return CreateErrorPage(422, out_response);
            }
        }

        const char *mode_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "mode");
        if (mode_str && mode_str[0]) {
            const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                                  std::end(mco_DispenseModeOptions),
                                                  [&](const OptionDesc &desc) { return TestStr(desc.name, mode_str); });
            if (desc == std::end(mco_DispenseModeOptions)) {
                LogError("Invalid 'mode' parameter value '%1'", mode_str);
                return CreateErrorPage(422, out_response);
            }
            dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
        }
    }

    if (!std::all_of(units.table.begin(), units.table.end(),
                     [&](UnitCode unit) { return allowed_units.Find(unit); })) {
        LogError("User is not allowed to view these units");
        return CreateErrorPage(422, out_response);
    }
    if (diff_dates[0].value && !dates[0].value) {
        LogError("Parameter 'diff' specified but 'dates' is missing");
        return CreateErrorPage(422, out_response);
    }
    if (dates[0].value && diff_dates[0].value &&
               dates[0] < diff_dates[1] && dates[1] > diff_dates[0]) {
        LogError("Parameters 'dates' and 'diff' must not overlap");
        return CreateErrorPage(422, out_response);
    }
    if (!CheckDispenseModeAgainstUser(*conn->user, dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
        return CreateErrorPage(422, out_response);
    }

    HeapArray<mco_Result> results;
    HeapArray<mco_Result> mono_results;
    mco_Classify(*thop_table_set, *thop_authorization_set, thop_stay_set.stays,
                 (int)mco_ClassifyFlag::Mono, &results, &mono_results);

    HeapArray<mco_Pricing> pricings;
    HeapArray<mco_Pricing> mono_pricings;
    mco_Price(results, false, &pricings);
    mco_Dispense(pricings, mono_results, dispense_mode, &mono_pricings);

    HeapArray<AggregateStatistics> statistics;
    {
        Size j = 0;
        HashMap<AggregateKey, Size> statistics_map;
        for (Size i = 0; i < results.len; i++) {
            const mco_Result &result = results[i];
            const mco_Pricing &pricing = pricings[i];

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
                    AggregateStatistics *agg;
                    {
                        AggregateKey key;
                        key.ghm = result.ghm;
                        key.ghs = result.ghs;
                        // TODO: Careful with duration overflow
                        key.duration = durations ? (int16_t)result.duration : 0;

                        std::pair<Size *, bool> ret = statistics_map.Append(key, statistics.len);
                        if (ret.first) {
                            agg = statistics.AppendDefault();
                            agg->key = key;
                        } else {
                            agg = &statistics[*ret.first];
                        }
                    }

                    if (!counted_rss) {
                        agg->count += multiplier;
                        agg->mono_count += multiplier * result.stays.len;
                        agg->price_cents += multiplier * pricing.price_cents;
                        if (result.stays[result.stays.len - 1].exit.mode == '9') {
                            agg->deaths += multiplier;
                        }
                        counted_rss = true;
                    }
                    agg->partial_mono_count += multiplier;
                    agg->partial_price_cents += multiplier * mono_pricing.price_cents;
                }
            }
        }
    }

    std::sort(statistics.begin(), statistics.end(),
              [](const AggregateStatistics &agg1, const AggregateStatistics &agg2) {
        return MultiCmp(agg1.key.ghm.value - agg2.key.ghm.value,
                        agg1.key.ghs.number - agg2.key.ghs.number,
                        agg1.key.duration - agg2.key.duration) < 0;
    });

    out_response->flags |= (int)Response::Flag::DisableETag;
    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        writer.StartArray();
        for (const AggregateStatistics &agg: statistics) {
            char buf[32];

            writer.StartObject();
            writer.Key("ghm"); writer.String(Fmt(buf, "%1", agg.key.ghm).ptr);
            writer.Key("ghs"); writer.Int(agg.key.ghs.number);
            if (durations) {
                writer.Key("duration"); writer.Int(agg.key.duration);
            }
            writer.Key("count"); writer.Int(agg.count);
            writer.Key("partial_mono_count"); writer.Int(agg.partial_mono_count);
            writer.Key("mono_count"); writer.Int(agg.mono_count);
            writer.Key("deaths"); writer.Int64(agg.deaths);
            writer.Key("partial_price_cents"); writer.Int64(agg.partial_price_cents);
            writer.Key("price_cents"); writer.Int64(agg.price_cents);
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    }, conn->compression_type, out_response);
}
