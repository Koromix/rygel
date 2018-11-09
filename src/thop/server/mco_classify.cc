// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "thop.hh"
#include "mco_classify.hh"
#include "structure.hh"
#include "user.hh"

static bool CheckDispenseModeAgainstUser(const User &user, mco_DispenseMode dispense_mode)
{
    return dispense_mode == thop_structure_set.dispense_mode ||
           (user.dispense_modes & (1 << (int)dispense_mode));
}

int ProduceMcoSettings(const ConnectionInfo *conn, const char *, Response *out_response)
{
    if (!thop_stay_set.stays.len || !conn->user)
        return CreateErrorPage(403, out_response);

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
                if (conn->user->allowed_units.Find(ent.unit)) {
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

struct AggregationGhmGhs {
    struct Key {
        mco_GhmCode ghm;
        mco_GhsCode ghs;

        bool operator==(const Key &other) const
        {
            return ghm == other.ghm &&
                   ghs == other.ghs;
        }
        bool operator !=(const Key &other) const { return !(*this == other); }

        uint64_t Hash() const
        {
            uint64_t hash = HashTraits<mco_GhmCode>::Hash(ghm) ^
                            HashTraits<mco_GhsCode>::Hash(ghs);

            return hash;
        }
    };

    Key key;
    const mco_GhmToGhsInfo *ghm_to_ghs_info;
    int16_t exh_treshold;
    int16_t exb_treshold;
    uint32_t durations;
};

enum class AggregationFlag {
    KeyOnDuration = 1 << 0,
    KeyOnUnits = 1 << 1,
    ExportGhsInfo = 1 << 2
};

template <typename T>
int ProduceMcoCasemix(const ConnectionInfo *conn, unsigned int flags,
                      T func, Response *out_response)
{
    if (!thop_stay_set.stays.len || !conn->user)
        return CreateErrorPage(403, out_response);

    Date dates[2] = {thop_stay_set_dates[0], thop_stay_set_dates[1]};
    Date diff_dates[2] = {};
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    bool apply_coefficent = false;
    {
        if (!ParseDateRange(MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "dates"),
                            &dates[0], &dates[1]))
            return CreateErrorPage(422, out_response);
        if (!ParseDateRange(MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "diff"),
                            &diff_dates[0], &diff_dates[1]))
            return CreateErrorPage(422, out_response);
        if (dates[0].value && diff_dates[0].value &&
                dates[0] < diff_dates[1] && dates[1] > diff_dates[0]) {
            LogError("Parameters 'dates' and 'diff' must not overlap");
            return CreateErrorPage(422, out_response);
        }

        const char *mode_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "dispense_mode");
        if (mode_str) {
            const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                                  std::end(mco_DispenseModeOptions),
                                                  [&](const OptionDesc &desc) { return TestStr(desc.name, mode_str); });
            if (desc == std::end(mco_DispenseModeOptions)) {
                LogError("Invalid 'dispense_mode' parameter value '%1'", mode_str);
                return CreateErrorPage(422, out_response);
            }
            dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
        } else {
            LogError("Missing 'dispense_mode' argument");
            return CreateErrorPage(422, out_response);
        }

        const char *apply_coefficent_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "apply_coefficient");
        if (apply_coefficent_str) {
            if (TestStr(apply_coefficent_str, "1")) {
                apply_coefficent = true;
            } else if (TestStr(apply_coefficent_str, "0")) {
                apply_coefficent = false;
            } else {
                LogError("Invalid 'apply_coefficent' parameter value '%1'", apply_coefficent_str);
                return CreateErrorPage(422, out_response);
            }
        } else {
            LogError("Missing 'apply_coefficent' argument");
            return CreateErrorPage(422, out_response);
        }
    }

    // Permissions
    if (!CheckDispenseModeAgainstUser(*conn->user, dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
        return CreateErrorPage(403, out_response);
    }

    // TODO: Parallelize and optimize aggregation
    HeapArray<AggregateStatistics> statistics;
    HeapArray<mco_GhmRootCode> ghm_roots;
    BlockAllocator statistics_units_alloc(Kibibytes(4));
    BlockAllocator statistics_parts_alloc(Kibibytes(16));
    {
        HashMap<AggregateStatistics::Key, Size> statistics_map;
        HashSet<mco_GhmRootCode> ghm_roots_set;

        // Reuse for performance
        HeapArray<mco_Pricing> pricings;
        HeapArray<mco_Pricing> mono_pricings;
        HashMap<UnitCode, AggregateStatistics::Part> agg_parts_map;

        for (;;) {
            Span<const mco_Result> results;
            Span<const mco_Result> mono_results;
            if (!func(&results, &mono_results))
                break;

            pricings.RemoveFrom(0);
            mono_pricings.RemoveFrom(0);
            mco_Price(results, apply_coefficent, &pricings);
            mco_Dispense(pricings, mono_results, dispense_mode, &mono_pricings);

            for (Size i = 0, j = 0; i < results.len; i++) {
                agg_parts_map.RemoveAll();

                const mco_Result &result = results[i];
                const mco_Pricing &pricing = pricings[i];
                Span<const mco_Result> sub_mono_results = mono_results.Take(j, result.stays.len);
                Span<const mco_Pricing> sub_mono_pricings = mono_pricings.Take(j, result.stays.len);
                j += result.stays.len;

                int multiplier;
                if (result.stays[result.stays.len - 1].exit.date >= dates[0] &&
                         result.stays[result.stays.len - 1].exit.date < dates[1]) {
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

                    if (conn->user->allowed_units.Find(unit)) {
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

                    if ((flags & (int)AggregationFlag::ExportGhsInfo) &&
                            ghm_roots_set.Append(result.ghm.Root()).second) {
                        ghm_roots.Append(result.ghm.Root());
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

    HeapArray<AggregationGhmGhs> ghm_ghs;
    if (flags & (int)AggregationFlag::ExportGhsInfo) {
        HashMap<AggregationGhmGhs::Key, Size> ghm_ghs_map;

        for (const mco_TableIndex &index: thop_table_set->indexes) {
            const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
                *thop_index_to_constraints[&index - thop_table_set->indexes.ptr];

            if ((dates[0] < index.limit_dates[1] && index.limit_dates[0] < dates[1]) ||
                    (diff_dates[0].value && diff_dates[0] < index.limit_dates[1] &&
                     index.limit_dates[0] < diff_dates[1])) {
                for (mco_GhmRootCode ghm_root: ghm_roots) {
                    Span<const mco_GhmToGhsInfo> compatible_ghs = index.FindCompatibleGhs(ghm_root);

                    for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
                        const mco_GhsPriceInfo *ghs_price_info = index.FindGhsPrice(ghm_to_ghs_info.Ghs(Sector::Public), Sector::Public);
                        const mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);

                        AggregationGhmGhs *agg_ghm_ghs;
                        {
                            AggregationGhmGhs::Key key;
                            key.ghm = ghm_to_ghs_info.ghm;
                            key.ghs = ghm_to_ghs_info.Ghs(Sector::Public);

                            std::pair<Size *, bool> ret = ghm_ghs_map.Append(key, ghm_ghs.len);
                            if (ret.second) {
                                agg_ghm_ghs = ghm_ghs.AppendDefault();
                                agg_ghm_ghs->key = key;
                                agg_ghm_ghs->ghm_to_ghs_info = &ghm_to_ghs_info;
                            } else {
                                agg_ghm_ghs = &ghm_ghs[*ret.first];
                            }
                        }

                        if (constraint) {
                            agg_ghm_ghs->durations |= constraint->durations &
                                                      ~((1u << ghm_to_ghs_info.minimal_duration) - 1);
                        }

                        if (ghs_price_info) {
                            if (!agg_ghm_ghs->exh_treshold ||
                                    ghs_price_info->exh_treshold < agg_ghm_ghs->exh_treshold) {
                                agg_ghm_ghs->exh_treshold = ghs_price_info->exh_treshold;
                            }
                            if (!agg_ghm_ghs->exb_treshold ||
                                    ghs_price_info->exb_treshold > agg_ghm_ghs->exb_treshold) {
                                agg_ghm_ghs->exb_treshold = ghs_price_info->exb_treshold;
                            }
                        }
                    }
                }
            }
        }
    }

    out_response->flags |= (int)Response::Flag::DisableCache;
    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[32];

        writer.StartObject();
        if (flags & (int)AggregationFlag::ExportGhsInfo) {
            writer.Key("ghs"); writer.StartArray();
            for (const AggregationGhmGhs &agg_ghm_ghs: ghm_ghs) {
                writer.StartObject();
                writer.Key("ghm"); writer.String(Fmt(buf, "%1", agg_ghm_ghs.key.ghm).ptr);
                writer.Key("ghs"); writer.Int(agg_ghm_ghs.key.ghs.number);
                writer.Key("conditions"); writer.Bool(agg_ghm_ghs.ghm_to_ghs_info->conditions_count);
                writer.Key("durations"); writer.Uint(agg_ghm_ghs.durations);

                if (agg_ghm_ghs.exh_treshold) {
                    writer.Key("exh_treshold"); writer.Int(agg_ghm_ghs.exh_treshold);
                }
                if (agg_ghm_ghs.exb_treshold) {
                    writer.Key("exb_treshold"); writer.Int(agg_ghm_ghs.exb_treshold);
                }
                writer.EndObject();
            }
            writer.EndArray();
        }

        writer.Key("rows"); writer.StartArray();
        for (const AggregateStatistics &agg: statistics) {
            writer.StartObject();
            writer.Key("ghm"); writer.String(Fmt(buf, "%1", agg.key.ghm).ptr);
            writer.Key("ghs"); writer.Int(agg.key.ghs.number);
            if (flags & (int)AggregationFlag::KeyOnDuration) {
                writer.Key("duration"); writer.Int(agg.key.duration);
            }
            if (flags & (int)AggregationFlag::KeyOnUnits) {
                writer.Key("unit"); writer.StartArray();
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
        writer.EndObject();

        return true;
    }, conn->compression_type, out_response);
}

int ProduceMcoCasemixUnits(const ConnectionInfo *conn, const char *, Response *out_response)
{
    const int split_size = 65536;

    Size i = 0, j = 0;
    return ProduceMcoCasemix(conn, (int)AggregationFlag::KeyOnUnits,
                             [&](Span<const mco_Result> *out_results,
                                 Span<const mco_Result> *out_mono_results) {
        if (i >= thop_results.len)
            return false;

        Size len = std::min((Size)split_size, thop_results.len - i);
        Size mono_len = 0;
        for (Size k = i, end = i + len; k < end; k++) {
            mono_len += thop_results[k].stays.len;
        }

        *out_results = thop_results.Take(i, len);
        *out_mono_results = thop_mono_results.Take(j, mono_len);
        i += len;
        j += mono_len;

        return true;
    }, out_response);
}

int ProduceMcoCasemixDuration(const ConnectionInfo *conn, const char *, Response *out_response)
{
    const int split_size = 8192;

    mco_GhmRootCode ghm_root;
    {
        const char *ghm_root_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "ghm_root");
        if (!ghm_root_str) {
            LogError("Missing 'ghm_root' argument");
            return CreateErrorPage(422, out_response);
        }
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
                                   (int)AggregationFlag::KeyOnUnits |
                                   (int)AggregationFlag::ExportGhsInfo,
                             [&](Span<const mco_Result> *out_results,
                                 Span<const mco_Result> *out_mono_results) {
        results.RemoveFrom(0);
        mono_results.RemoveFrom(0);

        for (; i < results_index.len && results.len < split_size; i++) {
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

// TODO: Limit maximum number of results (~ 500?)
// TODO: Split loop
// TODO: Deal with invalid values (duration, age)
int ProduceMcoResults(const ConnectionInfo *conn, const char *, Response *out_response)
{
    if (!thop_stay_set.stays.len || !conn->user ||
            !(conn->user->permissions & (int)UserPermission::Classify))
        return CreateErrorPage(403, out_response);

    Date dates[2] = {thop_stay_set_dates[0], thop_stay_set_dates[1]};
    mco_GhmRootCode ghm_root;
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    bool apply_coefficent = false;
    {
        if (!ParseDateRange(MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "dates"),
                            &dates[0], &dates[1]))
            return CreateErrorPage(422, out_response);

        const char *ghm_root_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "ghm_root");
        if (!ghm_root_str) {
            LogError("Missing 'ghm_root' argument");
            return CreateErrorPage(422, out_response);
        }
        ghm_root = mco_GhmRootCode::FromString(ghm_root_str);
        if (!ghm_root.IsValid())
            return CreateErrorPage(422, out_response);

        const char *mode_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "dispense_mode");
        if (mode_str) {
            const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                                  std::end(mco_DispenseModeOptions),
                                                  [&](const OptionDesc &desc) { return TestStr(desc.name, mode_str); });
            if (desc == std::end(mco_DispenseModeOptions)) {
                LogError("Invalid 'dispense_mode' parameter value '%1'", mode_str);
                return CreateErrorPage(422, out_response);
            }
            dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
        } else {
            LogError("Missing 'dispense_mode' argument");
            return CreateErrorPage(422, out_response);
        }

        const char *apply_coefficent_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "apply_coefficient");
        if (apply_coefficent_str) {
            if (TestStr(apply_coefficent_str, "1")) {
                apply_coefficent = true;
            } else if (TestStr(apply_coefficent_str, "0")) {
                apply_coefficent = false;
            } else {
                LogError("Invalid 'apply_coefficent' parameter value '%1'", apply_coefficent_str);
                return CreateErrorPage(422, out_response);
            }
        } else {
            LogError("Missing 'apply_coefficent' argument");
            return CreateErrorPage(422, out_response);
        }
    }

    if (!CheckDispenseModeAgainstUser(*conn->user, dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
        return CreateErrorPage(403, out_response);
    }

    HeapArray<mco_Result> results;
    HeapArray<mco_Result> mono_results;
    {

        Span<const mco_ResultPointers> results_index = thop_results_index_ghm_map.FindValue(ghm_root, {});

        for (const mco_ResultPointers &p: results_index) {
            bool allow = std::any_of(p.result->stays.begin(), p.result->stays.end(),
                                 [&](const mco_Stay &stay) {
                return !!conn->user->allowed_units.Find(stay.unit);
            });

            if (allow) {
                results.Append(*p.result);
                mono_results.Append(p.mono_results);
            }
        }
    }

    HeapArray<mco_Pricing> pricings;
    HeapArray<mco_Pricing> mono_pricings;
    mco_Price(results, apply_coefficent, &pricings);
    mco_Dispense(pricings, mono_results, dispense_mode, &mono_pricings);

    out_response->flags |= (int)Response::Flag::DisableCache;
    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        writer.StartArray();
        for (Size i = 0, j = 0; i < results.len; i++) {
            char buf[32];

            const mco_Result &result = results[i];
            const mco_Pricing &pricing = pricings[i];
            Span<const mco_Result> sub_mono_results = mono_results.Take(j, result.stays.len);
            Span<const mco_Pricing> sub_mono_pricings = mono_pricings.Take(j, result.stays.len);
            j += result.stays.len;

            if (result.stays[result.stays.len - 1].exit.date < dates[0] ||
                    result.stays[result.stays.len - 1].exit.date >= dates[1])
                continue;

            const mco_GhmRootInfo *ghm_root_info = result.index->FindGhmRoot(result.ghm.Root());
            const mco_DiagnosisInfo *main_diag_info =
                result.index->FindDiagnosis(result.stays[result.main_stay_idx].main_diagnosis);
            const mco_DiagnosisInfo *linked_diag_info =
                result.index->FindDiagnosis(result.stays[result.main_stay_idx].linked_diagnosis);

            writer.StartObject();

            writer.Key("admin_id"); writer.Int(result.stays[0].admin_id);
            writer.Key("bill_id"); writer.Int(result.stays[0].bill_id);
            writer.Key("index_date"); writer.String(Fmt(buf, "%1", result.index->limit_dates[0]).ptr);
            writer.Key("duration"); writer.Int(result.duration);
            writer.Key("sex"); writer.Int(result.stays[0].sex);
            writer.Key("age"); writer.Int(result.age);
            writer.Key("main_stay"); writer.Int(result.main_stay_idx);
            writer.Key("ghm"); writer.String(result.ghm.ToString(buf).ptr);
            writer.Key("main_error"); writer.Int(result.main_error);
            writer.Key("ghs"); writer.Int(result.ghs.number);
            writer.Key("ghs_duration"); writer.Int(result.ghs_duration);
            writer.Key("exb_exh"); writer.Int(pricing.exb_exh);
            writer.Key("price_cents"); writer.Int((int)pricing.price_cents);
            writer.Key("total_cents"); writer.Int((int)pricing.total_cents);

            writer.Key("stays"); writer.StartArray();
            for (Size k = 0; k < result.stays.len; k++) {
                const mco_Stay &stay = result.stays[k];
                const mco_Result &mono_result = sub_mono_results[k];
                const mco_Pricing &mono_pricing = sub_mono_pricings[k];

                writer.StartObject();

                writer.Key("duration"); writer.Int(mono_result.duration);
                writer.Key("unit"); writer.Int(stay.unit.number);
                if (conn->user->allowed_units.Find(stay.unit)) {
                    writer.Key("sex"); writer.Int(stay.sex);
                    writer.Key("birthdate"); writer.String(Fmt(buf, "%1", stay.birthdate).ptr);
                    writer.Key("entry_date"); writer.String(Fmt(buf, "%1", stay.entry.date).ptr);
                    writer.Key("entry_mode"); writer.String(&stay.entry.mode, 1);
                    if (stay.entry.origin) {
                        writer.Key("entry_origin"); writer.String(&stay.entry.origin, 1);
                    }
                    writer.Key("exit_date"); writer.String(Fmt(buf, "%1", stay.exit.date).ptr);
                    writer.Key("exit_mode"); writer.String(&stay.exit.mode, 1);
                    if (stay.exit.destination) {
                        writer.Key("exit_destination"); writer.String(&stay.exit.destination, 1);
                    }
                    if (stay.bed_authorization) {
                        writer.Key("bed_authorization"); writer.Int(stay.bed_authorization);
                    }
                    if (stay.session_count) {
                        writer.Key("session_count"); writer.Int(stay.session_count);
                    }
                    if (stay.igs2) {
                        writer.Key("igs2"); writer.Int(stay.igs2);
                    }
                    if (stay.last_menstrual_period.value) {
                        writer.Key("last_menstrual_period"); writer.String(Fmt(buf, "%1", stay.last_menstrual_period).ptr);
                    }
                    if (stay.gestational_age) {
                        writer.Key("gestational_age"); writer.Int(stay.gestational_age);
                    }
                    if (stay.newborn_weight) {
                        writer.Key("newborn_weight"); writer.Int(stay.newborn_weight);
                    }
                    if (stay.flags & (int)mco_Stay::Flag::Confirmed) {
                        writer.Key("confirm"); writer.Bool(true);
                    }
                    if (stay.dip_count) {
                        writer.Key("dip_count"); writer.Int(stay.dip_count);
                    }
                    if (stay.flags & (int)mco_Stay::Flag::Ucd) {
                        writer.Key("ucd"); writer.Bool(stay.flags & (int)mco_Stay::Flag::Ucd);
                    }

                    if (LIKELY(stay.main_diagnosis.IsValid())) {
                        writer.Key("main_diagnosis"); writer.String(stay.main_diagnosis.str);
                    }
                    if (stay.linked_diagnosis.IsValid()) {
                        writer.Key("linked_diagnosis"); writer.String(stay.linked_diagnosis.str);
                    }

                    writer.Key("other_diagnoses"); writer.StartArray();
                    for (DiagnosisCode diag: stay.diagnoses) {
                        if (diag == stay.main_diagnosis || diag == stay.linked_diagnosis)
                            continue;

                        const mco_DiagnosisInfo *diag_info = result.index->FindDiagnosis(diag);

                        writer.StartObject();
                        writer.Key("diag"); writer.String(diag.str);
                        if (diag_info) {
                            writer.Key("severity"); writer.Int(diag_info->Attributes(stay.sex).severity);
                        }
                        if (!ghm_root_info || !main_diag_info || !diag_info ||
                                mco_TestExclusion(*result.index, stay.sex, result.age,
                                                  *diag_info, *ghm_root_info, *main_diag_info,
                                                  linked_diag_info)) {
                            writer.Key("exclude"); writer.Bool(true);
                        }
                        writer.EndObject();
                    }
                    writer.EndArray();

                    writer.Key("procedures"); writer.StartArray();
                    for (const mco_ProcedureRealisation &proc: stay.procedures) {
                        writer.StartObject();
                        writer.Key("proc"); writer.String(proc.proc.str);
                        if (proc.phase) {
                            writer.Key("phase"); writer.Int(proc.phase);
                        }
                        writer.Key("activity"); writer.Int(proc.activity);
                        if (proc.extension) {
                            writer.Key("extension"); writer.Int(proc.extension);
                        }
                        writer.String("date"); writer.String(Fmt(buf, "%1", proc.date).ptr);
                        writer.String("count"); writer.Int(proc.count);
                        if (proc.doc) {
                            writer.String("doc"); writer.String(&proc.doc, 1);
                        }
                        writer.EndObject();
                    }
                    writer.EndArray();
                }

                writer.Key("price_cents"); writer.Int(mono_pricing.price_cents);
                writer.Key("total_cents"); writer.Int(mono_pricing.total_cents);

                writer.EndObject();
            }
            writer.EndArray();

            writer.EndObject();
        }
        writer.EndArray();

        return true;
    }, conn->compression_type, out_response);
}
