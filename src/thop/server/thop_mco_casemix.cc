// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "thop.hh"
#include "thop_mco.hh"

static bool CheckUnitAgainstUser(const User &user, const StructureEntity &ent)
{
    const auto CheckNeedle = [&](const char *needle) {
        return !!strstr(ent.path, needle);
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
        for (const StructureEntity &ent: structure.entities) {
            if (CheckUnitAgainstUser(*conn->user, ent))
                allowed_units.Append(ent.unit);
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
            writer.Key("entities"); writer.StartArray();
            for (const StructureEntity &ent: structure.entities) {
                if (allowed_units.Find(ent.unit)) {
                    writer.StartObject();
                    writer.Key("unit"); writer.Int(ent.unit.number);
                    writer.Key("path"); writer.String(ent.path);
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

struct AggregateStatistics {
    struct Key {
        mco_GhmCode ghm;
        mco_GhsCode ghs;
        int16_t duration;
        Span<UnitCode> units;

        bool operator==(const Key &other) const
        {
            return ghm == other.ghm &&
                   ghs == other.ghs &&
                   duration == other.duration &&
                   units == other.units;
        }
        bool operator !=(const Key &other) const { return !(*this == other); }

        uint64_t Hash() const
        {
            uint64_t hash = HashTraits<mco_GhmCode>::Hash(ghm) ^
                            HashTraits<mco_GhsCode>::Hash(ghs) ^
                            HashTraits<int16_t>::Hash(duration);
            for (UnitCode unit: units) {
                hash ^= HashTraits<UnitCode>::Hash(unit);
            }

            return hash;
        }
    };

    struct Part {
        int32_t mono_count;
        int64_t price_cents;
    };

    Key key;
    int32_t count;
    int32_t deaths;
    int32_t mono_count;
    int64_t price_cents;
    Span<Part> parts;

    HASH_TABLE_HANDLER(AggregateStatistics, key);
};

enum class AggregationFlag {
    KeyOnDuration = 1 << 0,
    KeyOnUnits = 1 << 1
};

template <typename T>
int ProduceMcoCasemix(const ConnectionInfo *conn, unsigned int flags,
                      T func, Response *out_response)
{
    if (!thop_stay_set.stays.len || !conn->user)
        return CreateErrorPage(404, out_response);

    HashSet<UnitCode> allowed_units;
    for (const Structure &structure: thop_structure_set.structures) {
        for (const StructureEntity &ent: structure.entities) {
            if (CheckUnitAgainstUser(*conn->user, ent))
                allowed_units.Append(ent.unit);
        }
    }

    Date dates[2] = {};
    Date diff_dates[2] = {};
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    {
        if (!ParseDateRange(MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "dates"),
                            &dates[0], &dates[1]))
            return CreateErrorPage(422, out_response);
        if (!ParseDateRange(MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "diff"),
                            &diff_dates[0], &diff_dates[1]))
            return CreateErrorPage(422, out_response);

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

    // TODO: Parallelize and optimize aggregation
    HeapArray<AggregateStatistics> statistics;
    BlockAllocator statistics_units_alloc(Kibibytes(4));
    BlockAllocator statistics_parts_alloc(Kibibytes(16));
    {
        HashMap<AggregateStatistics::Key, Size> statistics_map;

        // Reuse for performance
        HeapArray<mco_Pricing> pricings;
        HeapArray<mco_Pricing> mono_pricings;
        HashMap<UnitCode, AggregateStatistics::Part> agg_parts_map;

        Size i = 0, j = 0;
        for (;;) {
            Span<const mco_Result> results;
            Span<const mco_Result> mono_results;
            if (!func(i, j, &results, &mono_results))
                break;

            pricings.RemoveFrom(0);
            mono_pricings.RemoveFrom(0);
            mco_Price(results, true, &pricings);
            mco_Dispense(pricings, mono_results, dispense_mode, &mono_pricings);

            for (i = 0, j = 0; i < results.len; i++) {
                agg_parts_map.RemoveAll();

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

                bool match = false;
                HeapArray<UnitCode> agg_units(&statistics_units_alloc);
                for (Size k = 0; k < sub_mono_results.len; k++) {
                    const mco_Result &mono_result = sub_mono_results[k];
                    const mco_Pricing &mono_pricing = sub_mono_pricings[k];
                    UnitCode unit = mono_result.stays[0].unit;
                    DebugAssert(mono_result.stays[0].bill_id == result.stays[0].bill_id);

                    if (allowed_units.Find(unit)) {
                        std::pair<AggregateStatistics::Part *, bool> ret =
                            agg_parts_map.AppendDefault(mono_result.stays[0].unit);

                        ret.first->mono_count += multiplier;
                        ret.first->price_cents += multiplier * mono_pricing.price_cents;

                        if ((flags & (int)AggregationFlag::KeyOnUnits) && ret.second) {
                            agg_units.Append(unit);
                        }

                        match = true;
                    }
                }

                if (match) {
                    std::sort(agg_units.begin(), agg_units.end());

                    HeapArray<AggregateStatistics::Part> agg_parts(&statistics_parts_alloc);
                    for (UnitCode unit: agg_units) {
                        AggregateStatistics::Part *part = agg_parts_map.Find(unit);
                        if (part) {
                            agg_parts.Append(*part);
                        }
                    }

                    AggregateStatistics::Key key = {};
                    key.ghm = result.ghm;
                    key.ghs = result.ghs;
                    if (flags & (int)AggregationFlag::KeyOnDuration) {
                        key.duration = result.duration;
                    }
                    if (flags & (int)AggregationFlag::KeyOnUnits) {
                        key.units = agg_units.TrimAndLeak();
                    }

                    AggregateStatistics *agg;
                    {
                        std::pair<Size *, bool> ret = statistics_map.Append(key, statistics.len);
                        if (ret.second) {
                            agg = statistics.AppendDefault();
                            agg->key = key;
                        } else {
                            agg = &statistics[*ret.first];
                        }
                    }

                    agg->count += multiplier;
                    agg->deaths += multiplier * (result.stays[result.stays.len - 1].exit.mode == '9');
                    agg->mono_count += multiplier * (int32_t)result.stays.len;
                    agg->price_cents += multiplier * pricing.price_cents;
                    if (agg->parts.ptr) {
                        DebugAssert(agg->parts.len == agg_parts.len);
                        for (Size k = 0; k < agg->parts.len; k++) {
                            agg->parts[k].mono_count += agg_parts[k].mono_count;
                            agg->parts[k].price_cents += agg_parts[k].price_cents;
                        }
                    } else {
                        agg->parts = agg_parts.TrimAndLeak();
                    }
                }
            }
        }
    }

    std::sort(statistics.begin(), statistics.end(),
              [](const AggregateStatistics &agg1, const AggregateStatistics &agg2) {
        return MultiCmp(agg1.key.ghm.value - agg2.key.ghm.value,
                        agg1.key.ghs.number - agg2.key.ghs.number) < 0;
    });

    out_response->flags |= (int)Response::Flag::DisableETag;
    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        writer.StartArray();
        for (const AggregateStatistics &agg: statistics) {
            char buf[32];

            writer.StartObject();
            writer.Key("ghm"); writer.String(Fmt(buf, "%1", agg.key.ghm).ptr);
            writer.Key("ghs"); writer.Int(agg.key.ghs.number);
            if (flags & (int)AggregationFlag::KeyOnDuration) {
                writer.Key("duration"); writer.Int(agg.key.duration);
            }
            if (flags & (int)AggregationFlag::KeyOnUnits) {
                writer.Key("units"); writer.StartArray();
                for (UnitCode unit: agg.key.units) {
                    writer.Int(unit.number);
                }
                writer.EndArray();
            }
            writer.Key("count"); writer.Int(agg.count);
            writer.Key("deaths"); writer.Int64(agg.deaths);
            writer.Key("mono_count_total"); writer.Int(agg.mono_count);
            writer.Key("price_cents_total"); writer.Int64(agg.price_cents);
            writer.Key("mono_count"); writer.StartArray();
            for (const AggregateStatistics::Part &part: agg.parts) {
                writer.Int(part.mono_count);
            }
            writer.EndArray();
            writer.Key("price_cents"); writer.StartArray();
            for (const AggregateStatistics::Part &part: agg.parts) {
                writer.Int64(part.price_cents);
            }
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    }, conn->compression_type, out_response);
}

int ProduceMcoCasemixUnits(const ConnectionInfo *conn, const char *, Response *out_response)
{
    Size i = 0, j = 0;
    return ProduceMcoCasemix(conn, (int)AggregationFlag::KeyOnUnits,
                             [&](Size consumed, Size mono_consumed,
                                 Span<const mco_Result> *out_results,
                                 Span<const mco_Result> *out_mono_results) {
        i += consumed;
        j += mono_consumed;
        if (i >= thop_results.len)
            return false;

        Size sub_len = std::min((Size)262144, thop_results.len - i);
        *out_results = thop_results.Take(i, sub_len);
        *out_mono_results = thop_mono_results.Take(j, thop_mono_results.len - j);

        return true;
    }, out_response);
}

int ProduceMcoCasemixDuration(const ConnectionInfo *conn, const char *, Response *out_response)
{
    mco_GhmRootCode ghm_root;
    {
        const char *ghm_root_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "ghm_root");
        if (!ghm_root_str)
            return CreateErrorPage(422, out_response);

        ghm_root = mco_GhmRootCode::FromString(ghm_root_str);
        if (!ghm_root.IsValid())
            return CreateErrorPage(422, out_response);
    }

    Span<const mco_ResultPointers> results_index = thop_results_index_ghm_map.FindValue(ghm_root, {});

    // Reuse for performance
    HeapArray<mco_Result> results;
    HeapArray<mco_Result> mono_results;

    Size i = 0;
    return ProduceMcoCasemix(conn, (int)AggregationFlag::KeyOnDuration |
                                   (int)AggregationFlag::KeyOnUnits,
                             [&](Size, Size,
                                 Span<const mco_Result> *out_results,
                                 Span<const mco_Result> *out_mono_results) {
        results.RemoveFrom(0);
        mono_results.RemoveFrom(0);

        for (; i < results_index.len && results.len < 8192; i++) {
            const mco_ResultPointers &p = results_index[i];

            results.Append(*p.result);
            mono_results.Append(p.mono_results);
        }
        if (!results.len)
            return false;

        *out_results = results;
        *out_mono_results = mono_results;

        return true;
    }, out_response);
}
