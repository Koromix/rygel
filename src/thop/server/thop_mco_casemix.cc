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
        return CreateErrorPage(404, out_response);

    HashSet<UnitCode> allowed_units;
    for (const Structure &structure: thop_structure_set.structures) {
        for (const StructureEntity &ent: structure.entities) {
            if (CheckUnitAgainstUser(*conn->user, ent))
                allowed_units.Append(ent.unit);
        }
    }

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

        const char *apply_coefficent_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "apply_coefficient");
        if (apply_coefficent_str && apply_coefficent_str[0]) {
            if (TestStr(apply_coefficent_str, "1")) {
                apply_coefficent = true;
            } else if (TestStr(apply_coefficent_str, "0")) {
                apply_coefficent = false;
            } else {
                LogError("Invalid 'apply_coefficent' parameter value '%1'", apply_coefficent_str);
                return CreateErrorPage(422, out_response);
            }
        }
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
                        DebugAssert(constraint);

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

                        agg_ghm_ghs->durations |= constraint->durations &
                                                  ~((1u << ghm_to_ghs_info.minimal_duration) - 1);

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

    out_response->flags |= (int)Response::Flag::DisableCacheControl |
                           (int)Response::Flag::DisableETag;
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
