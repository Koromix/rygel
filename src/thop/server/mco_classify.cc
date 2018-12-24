// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "thop.hh"
#include "mco_classify.hh"
#include "user.hh"

static int GetQueryDateRange(MHD_Connection *conn, const char *key, Response *out_response,
                             Date *out_start_date, Date *out_end_date)
{
    const char *str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        return CreateErrorPage(422, out_response);
    }

    Date start_date;
    Date end_date = {};
    {
        Span<const char> remain = str;

        start_date = Date::FromString(remain, 0, &remain);
        if (remain.len < 2 || remain[0] != '.' || remain[1] != '.')
            goto invalid;
        end_date = Date::FromString(remain.Take(2, remain.len - 2), 0, &remain);
        if (remain.len)
            goto invalid;

        if (!start_date.IsValid() || !end_date.IsValid() || end_date <= start_date)
            goto invalid;
    }

    *out_start_date = start_date;
    *out_end_date = end_date;
    return 0;

invalid:
    LogError("Invalid date range '%1'", str);
    return false;
}

static int GetQueryDispenseMode(MHD_Connection *conn, const char *key, Response *out_response,
                                mco_DispenseMode *out_dispense_mode)
{
    const char *str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        return CreateErrorPage(422, out_response);
    }

    const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                          std::end(mco_DispenseModeOptions),
                                          [&](const OptionDesc &desc) { return TestStr(desc.name, str); });
    if (desc == std::end(mco_DispenseModeOptions)) {
        LogError("Invalid '%1' parameter value '%2'", key, str);
        return CreateErrorPage(422, out_response);
    }

    *out_dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
    return 0;
}

static int GetQueryApplyCoefficient(MHD_Connection *conn, const char *key, Response *out_response,
                                    bool *out_apply_coefficient)
{
    const char *str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        return CreateErrorPage(422, out_response);
    }

    bool apply_coefficient;
    if (TestStr(str, "1")) {
        apply_coefficient = true;
    } else if (TestStr(str, "0")) {
        apply_coefficient = false;
    } else {
        LogError("Invalid '%1' parameter value '%2'", key, str);
        return CreateErrorPage(422, out_response);
    }

    *out_apply_coefficient = apply_coefficient;
    return 0;
}

static int GetQueryGhmRoot(MHD_Connection *conn, const char *key, Response *out_response,
                           mco_GhmRootCode *out_ghm_root)
{
    const char *str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, key);
    if (!str) {
        LogError("Missing 'ghm_root' argument");
        return CreateErrorPage(422, out_response);
    }

    mco_GhmRootCode ghm_root = mco_GhmRootCode::FromString(str);
    if (!ghm_root.IsValid())
        return CreateErrorPage(422, out_response);

    *out_ghm_root = ghm_root;
    return 0;
}

struct Aggregate {
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

    HASH_TABLE_HANDLER(Aggregate, key);
};

enum class AggregationFlag {
    KeyOnDuration = 1 << 0,
    KeyOnUnits = 1 << 1
};

struct AggregateSet {
    HeapArray<Aggregate> aggregates;

    LinkedAllocator array_alloc;
};

class AggregateSetBuilder {
    mco_DispenseMode dispense_mode;
    bool apply_coefficient;
    const HashSet<UnitCode> *allowed_units;
    unsigned int flags;

    AggregateSet set;
    HeapArray<mco_GhmRootCode> ghm_roots;

    HashMap<Aggregate::Key, Size> aggregates_map;
    HashSet<mco_GhmRootCode> ghm_roots_set;
    IndirectBlockAllocator units_alloc {&set.array_alloc, Kibibytes(4)};
    IndirectBlockAllocator parts_alloc {&set.array_alloc, Kibibytes(16)};

    // Reuse for performance
    HashMap<UnitCode, Aggregate::Part> agg_parts_map;

public:
    AggregateSetBuilder(mco_DispenseMode dispense_mode, bool apply_coefficient,
                        const HashSet<UnitCode> &allowed_units, unsigned int flags)
        : dispense_mode(dispense_mode), apply_coefficient(apply_coefficient),
          allowed_units(&allowed_units), flags(flags) {}

    void Process(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                 Span<const mco_Pricing> pricings, Span<const mco_Pricing> mono_pricings,
                 int multiplier = 1)
    {
        for (Size i = 0, j = 0; i < results.len; i++) {
            agg_parts_map.RemoveAll();

            const mco_Result &result = results[i];
            const mco_Pricing &pricing = pricings[i];
            Span<const mco_Result> sub_mono_results = mono_results.Take(j, result.stays.len);
            Span<const mco_Pricing> sub_mono_pricings = mono_pricings.Take(j, result.stays.len);
            j += result.stays.len;

            bool match = false;
            HeapArray<UnitCode> agg_units(&units_alloc);
            for (Size k = 0; k < sub_mono_results.len; k++) {
                const mco_Result &mono_result = sub_mono_results[k];
                const mco_Pricing &mono_pricing = sub_mono_pricings[k];
                UnitCode unit = mono_result.stays[0].unit;
                DebugAssert(mono_result.stays[0].bill_id == result.stays[0].bill_id);

                if (allowed_units->Find(unit)) {
                    std::pair<Aggregate::Part *, bool> ret =
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

                HeapArray<Aggregate::Part> agg_parts(&parts_alloc);
                for (UnitCode unit: agg_units) {
                    Aggregate::Part *part = agg_parts_map.Find(unit);
                    if (part) {
                        agg_parts.Append(*part);
                    }
                }

                Aggregate::Key key = {};
                key.ghm = result.ghm;
                key.ghs = result.ghs;
                if (flags & (int)AggregationFlag::KeyOnDuration) {
                    key.duration = result.duration;
                }
                if (flags & (int)AggregationFlag::KeyOnUnits) {
                    key.units = agg_units.TrimAndLeak();
                }

                Aggregate *agg;
                {
                    std::pair<Size *, bool> ret = aggregates_map.Append(key, set.aggregates.len);
                    if (ret.second) {
                        agg = set.aggregates.AppendDefault();
                        agg->key = key;
                    } else {
                        agg = &set.aggregates[*ret.first];
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

                if (ghm_roots_set.Append(result.ghm.Root()).second) {
                    ghm_roots.Append(result.ghm.Root());
                }
            }
        }
    }

    void ProcessIndexedResults(Span<const mco_ResultPointers> index, int multiplier = 1)
    {
        const int split_size = 8192;

        // Reuse for performance
        HeapArray<mco_Result> results;
        HeapArray<mco_Result> mono_results;

        Size i = 0;
        RunAggregationLoop([&](Span<const mco_Result> *out_results,
                             Span<const mco_Result> *out_mono_results) {
            results.RemoveFrom(0);
            mono_results.RemoveFrom(0);

            for (; i < index.len && results.len < split_size; i++) {
                const mco_ResultPointers &p = index[i];

                results.Append(*p.result);
                mono_results.Append(MakeSpan(p.mono_result, p.result->stays.len));
            }
            if (!results.len)
                return false;

            *out_results = results;
            *out_mono_results = mono_results;

            return true;
        }, multiplier);
    }

    void ProcessResults(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                        int multiplier = 1)
    {
        const int split_size = 65536;

        Size i = 0, j = 0;
        RunAggregationLoop([&](Span<const mco_Result> *out_results,
                             Span<const mco_Result> *out_mono_results) {
            if (i >= results.len)
                return false;

            Size len = std::min((Size)split_size, results.len - i);
            Size mono_len = 0;
            for (Size k = i, end = i + len; k < end; k++) {
                mono_len += results[k].stays.len;
            }

            *out_results = results.Take(i, len);
            *out_mono_results = mono_results.Take(j, mono_len);
            i += len;
            j += mono_len;

            return true;
        }, multiplier);
    }

    void Finish(AggregateSet *out_set, HeapArray<mco_GhmRootCode> *out_ghm_roots = nullptr)
    {
        std::sort(set.aggregates.begin(), set.aggregates.end(),
                  [](const Aggregate &agg1, const Aggregate &agg2) {
            return MultiCmp(agg1.key.ghm.value - agg2.key.ghm.value,
                            agg1.key.ghs.number - agg2.key.ghs.number) < 0;
        });

        SwapMemory(out_set, &set, SIZE(set));
        if (out_ghm_roots) {
            std::swap(ghm_roots, *out_ghm_roots);
        }
    }

private:
    template <typename T>
    void RunAggregationLoop(T func, int multiplier)
    {
        // Reuse for performance
        HeapArray<mco_Pricing> pricings;
        HeapArray<mco_Pricing> mono_pricings;

        // TODO: Parallelize and optimize aggregation
        for (;;) {
            Span<const mco_Result> results;
            Span<const mco_Result> mono_results;
            if (!func(&results, &mono_results))
                break;

            pricings.RemoveFrom(0);
            mono_pricings.RemoveFrom(0);
            mco_Price(results, apply_coefficient, &pricings);
            mco_Dispense(pricings, mono_results, dispense_mode, &mono_pricings);

            Process(results, mono_results, pricings, mono_pricings, multiplier);
        }
    }
};

struct GhmGhsInfo {
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

static void GatherGhmGhsInfo(Span<const mco_GhmRootCode> ghm_roots, Date min_date, Date max_date,
                             HeapArray<GhmGhsInfo> *out_ghm_ghs)
{
    HashMap<GhmGhsInfo::Key, Size> ghm_ghs_map;

    for (const mco_TableIndex &index: thop_table_set.indexes) {
        const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
            *thop_index_to_constraints[&index - thop_table_set.indexes.ptr];

        if (min_date < index.limit_dates[1] && index.limit_dates[0] < max_date) {
            for (mco_GhmRootCode ghm_root: ghm_roots) {
                Span<const mco_GhmToGhsInfo> compatible_ghs = index.FindCompatibleGhs(ghm_root);

                for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
                    const mco_GhsPriceInfo *ghs_price_info = index.FindGhsPrice(ghm_to_ghs_info.Ghs(Sector::Public), Sector::Public);
                    const mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);

                    GhmGhsInfo *agg_ghm_ghs;
                    {
                        GhmGhsInfo::Key key;
                        key.ghm = ghm_to_ghs_info.ghm;
                        key.ghs = ghm_to_ghs_info.Ghs(Sector::Public);

                        std::pair<Size *, bool> ret = ghm_ghs_map.Append(key, out_ghm_ghs->len);
                        if (ret.second) {
                            agg_ghm_ghs = out_ghm_ghs->AppendDefault();
                            agg_ghm_ghs->key = key;
                            agg_ghm_ghs->ghm_to_ghs_info = &ghm_to_ghs_info;
                        } else {
                            agg_ghm_ghs = &(*out_ghm_ghs)[*ret.first];
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

static Span<const mco_Result> GetResultsRange(Span<const mco_Result> results,
                                              Date min_date, Date max_date)
{
    const mco_Result *start =
        std::lower_bound(results.begin(), results.end(), min_date,
                         [](const mco_Result &result, Date date) {
        return result.stays[result.stays.len - 1].exit.date < date;
    });
    const mco_Result *end =
        std::upper_bound(start, (const mco_Result *)results.end(), max_date,
                         [](Date date, const mco_Result &result) {
        return date < result.stays[result.stays.len - 1].exit.date;
    });

    return MakeSpan(start, end);
}

static Span<const mco_ResultPointers> GetIndexedResultsRange(Span<const mco_ResultPointers> index,
                                                             Date min_date, Date max_date)
{
    const mco_ResultPointers *start =
        std::lower_bound(index.begin(), index.end(), min_date,
                         [](const mco_ResultPointers &p, Date date) {
        return p.result->stays[p.result->stays.len - 1].exit.date < date;
    });
    const mco_ResultPointers *end =
        std::upper_bound(start, index.end(), max_date,
                         [](Date date, const mco_ResultPointers &p) {
        return date < p.result->stays[p.result->stays.len - 1].exit.date;
    });

    return MakeSpan(start, end);
}

int ProduceMcoCasemix(const ConnectionInfo *conn, const char *, Response *out_response)
{
    if (!conn->user)
        return CreateErrorPage(403, out_response);

    // Get query parameters
    Date period[2] = {};
    Date diff[2] = {};
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    bool apply_coefficient = false;
    if (int code = GetQueryDateRange(conn->conn, "period", out_response, &period[0], &period[1]); code)
        return code;
    if (MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "diff")) {
        if (int code = GetQueryDateRange(conn->conn, "diff", out_response, &diff[0], &diff[1]); code)
            return code;
    }
    if (int code = GetQueryDispenseMode(conn->conn, "dispense_mode", out_response, &dispense_mode); code)
        return code;
    if (int code = GetQueryApplyCoefficient(conn->conn, "apply_coefficient", out_response, &apply_coefficient); code)
        return code;

    // Check errors and permissions
    if (diff[0].value && period[0] < diff[1] && period[1] > diff[0]) {
        LogError("Parameters 'period' and 'diff' must not overlap");
        return CreateErrorPage(422, out_response);
    }
    if (!conn->user->CheckDispenseMode(dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
        return CreateErrorPage(403, out_response);
    }

    // Aggregate casemix
    int flags;
    AggregateSet aggregate_set;
    HeapArray<mco_GhmRootCode> ghm_roots;
    if (MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "ghm_root")) {
        mco_GhmRootCode ghm_root = {};
        if (int code = GetQueryGhmRoot(conn->conn, "ghm_root", out_response, &ghm_root); code)
            return code;

        flags = (int)AggregationFlag::KeyOnUnits | (int)AggregationFlag::KeyOnDuration;
        AggregateSetBuilder aggregate_set_builder(dispense_mode, apply_coefficient,
                                                  conn->user->allowed_units, flags);

        Span<const mco_ResultPointers> index = thop_results_index_ghm_map.FindValue(ghm_root, {});

        // Run aggregation
        aggregate_set_builder.ProcessIndexedResults(GetIndexedResultsRange(index, period[0], period[1]), 1);
        if (diff[0].value) {
            aggregate_set_builder.ProcessIndexedResults(GetIndexedResultsRange(index, diff[0], diff[1]), -1);
        }

        aggregate_set_builder.Finish(&aggregate_set, &ghm_roots);
    } else {
        flags = (int)AggregationFlag::KeyOnUnits;
        AggregateSetBuilder aggregate_set_builder(dispense_mode, apply_coefficient,
                                                  conn->user->allowed_units, flags);

        // Main aggregation
        {
            Span<const mco_Result> results = GetResultsRange(thop_results, period[0], period[1]);
            Span<const mco_Result> mono_results = MakeSpan(thop_results_to_mono_results.FindValue(results.begin(), nullptr),
                                                           thop_results_to_mono_results.FindValue(results.end(), nullptr));
            aggregate_set_builder.ProcessResults(results, mono_results, 1);
        }

        // Diff aggregation
        if (diff[0].value) {
            Span<const mco_Result> results = GetResultsRange(thop_results, diff[0], diff[1]);
            Span<const mco_Result> mono_results = MakeSpan(thop_results_to_mono_results.FindValue(results.begin(), nullptr),
                                                           thop_results_to_mono_results.FindValue(results.end(), nullptr));
            aggregate_set_builder.ProcessResults(results, mono_results, -1);
        }

        aggregate_set_builder.Finish(&aggregate_set);
    }

    // GHM and GHS info
    HeapArray<GhmGhsInfo> ghm_ghs;
    {
        Date min_date = diff[0].value ? std::min(diff[0], period[0]) : period[0];
        Date max_date = diff[0].value ? std::min(diff[1], period[1]) : period[1];
        GatherGhmGhsInfo(ghm_roots, min_date, max_date, &ghm_ghs);
    }

    // Export data
    out_response->flags |= (int)Response::Flag::DisableCache;
    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[32];

        writer.StartObject();

        writer.Key("ghs"); writer.StartArray();
        for (const GhmGhsInfo &ghm_ghs_info: ghm_ghs) {
            writer.StartObject();
            writer.Key("ghm"); writer.String(Fmt(buf, "%1", ghm_ghs_info.key.ghm).ptr);
            writer.Key("ghs"); writer.Int(ghm_ghs_info.key.ghs.number);
            writer.Key("conditions"); writer.Bool(ghm_ghs_info.ghm_to_ghs_info->conditions_count);
            writer.Key("durations"); writer.Uint(ghm_ghs_info.durations);

            if (ghm_ghs_info.exh_treshold) {
                writer.Key("exh_treshold"); writer.Int(ghm_ghs_info.exh_treshold);
            }
            if (ghm_ghs_info.exb_treshold) {
                writer.Key("exb_treshold"); writer.Int(ghm_ghs_info.exb_treshold);
            }
            writer.EndObject();
        }
        writer.EndArray();

        writer.Key("rows"); writer.StartArray();
        for (const Aggregate &agg: aggregate_set.aggregates) {
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
            for (const Aggregate::Part &part: agg.parts) {
                writer.Int(part.mono_count);
            }
            writer.EndArray();
            writer.Key("price_cents"); writer.StartArray();
            for (const Aggregate::Part &part: agg.parts) {
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

int ProduceMcoResults(const ConnectionInfo *conn, const char *, Response *out_response)
{
    if (!conn->user || !(conn->user->permissions & (int)UserPermission::FullResults))
        return CreateErrorPage(403, out_response);

    // Get query parameters
    Date period[2] = {};
    mco_GhmRootCode ghm_root = {};
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    bool apply_coefficent = false;
    if (int code = GetQueryDateRange(conn->conn, "period", out_response, &period[0], &period[1]); code)
        return code;
    if (int code = GetQueryDispenseMode(conn->conn, "dispense_mode", out_response, &dispense_mode); code)
        return code;
    if (int code = GetQueryApplyCoefficient(conn->conn, "apply_coefficient", out_response, &apply_coefficent); code)
        return code;
    if (int code = GetQueryGhmRoot(conn->conn, "ghm_root", out_response, &ghm_root); code)
        return code;

    // Check permissions
    if (!conn->user->CheckDispenseMode(dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
        return CreateErrorPage(403, out_response);
    }

    // Gather results
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
                mono_results.Append(MakeSpan(p.mono_result, p.result->stays.len));
            }
        }
    }

    // Compute prices
    HeapArray<mco_Pricing> pricings;
    HeapArray<mco_Pricing> mono_pricings;
    mco_Price(results, apply_coefficent, &pricings);
    mco_Dispense(pricings, mono_results, dispense_mode, &mono_pricings);

    // Export data
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

            if (result.stays[result.stays.len - 1].exit.date < period[0] ||
                    result.stays[result.stays.len - 1].exit.date >= period[1])
                continue;

            const mco_GhmRootInfo *ghm_root_info = nullptr;
            const mco_DiagnosisInfo *main_diag_info = nullptr;
            const mco_DiagnosisInfo *linked_diag_info = nullptr;
            if (LIKELY(result.index)) {
                ghm_root_info = result.index->FindGhmRoot(result.ghm.Root());
                main_diag_info = result.index->FindDiagnosis(result.stays[result.main_stay_idx].main_diagnosis);
                linked_diag_info = result.index->FindDiagnosis(result.stays[result.main_stay_idx].linked_diagnosis);
            }

            writer.StartObject();

            writer.Key("admin_id"); writer.Int(result.stays[0].admin_id);
            writer.Key("bill_id"); writer.Int(result.stays[0].bill_id);
            if (LIKELY(result.index)) {
                writer.Key("index_date"); writer.String(Fmt(buf, "%1", result.index->limit_dates[0]).ptr);
            }
            if (result.duration >= 0) {
                writer.Key("duration"); writer.Int(result.duration);
            }
            writer.Key("sex"); writer.Int(result.stays[0].sex);
            if (result.age >= 0) {
                writer.Key("age"); writer.Int(result.age);
            }
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

                if (mono_result.duration >= 0) {
                    writer.Key("duration"); writer.Int(mono_result.duration);
                }
                writer.Key("unit"); writer.Int(stay.unit.number);
                if (conn->user->allowed_units.Find(stay.unit)) {
                    writer.Key("sex"); writer.Int(stay.sex);
                    writer.Key("age"); writer.Int(mono_result.age);
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
                    for (DiagnosisCode diag: stay.other_diagnoses) {
                        const mco_DiagnosisInfo *diag_info =
                            LIKELY(result.index) ? result.index->FindDiagnosis(diag) : nullptr;

                        writer.StartObject();
                        writer.Key("diag"); writer.String(diag.str);
                        if (!result.ghm.IsError() && ghm_root_info && main_diag_info && diag_info) {
                            writer.Key("severity"); writer.Int(diag_info->Attributes(stay.sex).severity);

                            if (mco_TestExclusion(*result.index, stay.sex, result.age,
                                                  *diag_info, *ghm_root_info, *main_diag_info,
                                                  linked_diag_info)) {
                                writer.Key("exclude"); writer.Bool(true);
                            }
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

                writer.Key("price_cents"); writer.Int64(mono_pricing.price_cents);
                writer.Key("total_cents"); writer.Int64(mono_pricing.total_cents);

                writer.EndObject();
            }
            writer.EndArray();

            writer.EndObject();
        }
        writer.EndArray();

        return true;
    }, conn->compression_type, out_response);
}
