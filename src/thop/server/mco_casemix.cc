// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "config.hh"
#include "mco_casemix.hh"
#include "mco.hh"
#include "thop.hh"
#include "user.hh"

namespace RG {

static int GetQueryDateRange(const http_Request &request, const char *key,
                             http_Response *out_response, Date *out_start_date, Date *out_end_date)
{
    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        return http_ProduceErrorPage(422, out_response);
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
    return http_ProduceErrorPage(422, out_response);
}

static int GetQueryDispenseMode(const http_Request &request, const char *key,
                                http_Response *out_response, mco_DispenseMode *out_dispense_mode)
{
    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        return http_ProduceErrorPage(422, out_response);
    }

    const OptionDesc *desc = FindIf(mco_DispenseModeOptions,
                                    [&](const OptionDesc &desc) { return TestStr(desc.name, str); });
    if (!desc) {
        LogError("Invalid '%1' parameter value '%2'", key, str);
        return http_ProduceErrorPage(422, out_response);
    }

    *out_dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
    return 0;
}

static int GetQueryApplyCoefficient(const http_Request &request, const char *key,
                                    http_Response *out_response, bool *out_apply_coefficient)
{
    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        return http_ProduceErrorPage(422, out_response);
    }

    bool apply_coefficient;
    if (TestStr(str, "1")) {
        apply_coefficient = true;
    } else if (TestStr(str, "0")) {
        apply_coefficient = false;
    } else {
        LogError("Invalid '%1' parameter value '%2'", key, str);
        return http_ProduceErrorPage(422, out_response);
    }

    *out_apply_coefficient = apply_coefficient;
    return 0;
}

static int GetQueryGhmRoot(const http_Request &request, const char *key,
                           http_Response *out_response, mco_GhmRootCode *out_ghm_root)
{
    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing 'ghm_root' argument");
        return http_ProduceErrorPage(422, out_response);
    }

    mco_GhmRootCode ghm_root = mco_GhmRootCode::FromString(str);
    if (!ghm_root.IsValid())
        return http_ProduceErrorPage(422, out_response);

    *out_ghm_root = ghm_root;
    return 0;
}

struct Aggregate {
    struct Key {
        mco_GhmCode ghm;
        mco_GhsCode ghs;
        int16_t duration;
        Span<drd_UnitCode> units;

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
            for (drd_UnitCode unit: units) {
                hash ^= HashTraits<drd_UnitCode>::Hash(unit);
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

    RG_HASH_TABLE_HANDLER(Aggregate, key);
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
    const User *user;
    unsigned int flags;

    AggregateSet set;
    HeapArray<mco_GhmRootCode> ghm_roots;

    HashMap<Aggregate::Key, Size> aggregates_map;
    HashSet<mco_GhmRootCode> ghm_roots_set;
    IndirectBlockAllocator units_alloc {&set.array_alloc, Kibibytes(4)};
    IndirectBlockAllocator parts_alloc {&set.array_alloc, Kibibytes(16)};

    // Reuse for performance
    HashMap<drd_UnitCode, Aggregate::Part> agg_parts_map;

public:
    AggregateSetBuilder(const User *user, unsigned int flags)
        : user(user), flags(flags) {}

    void Process(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                 Span<const mco_Pricing> pricings, Span<const mco_Pricing> mono_pricings,
                 int multiplier = 1);

    void Finish(AggregateSet *out_set, HeapArray<mco_GhmRootCode> *out_ghm_roots = nullptr);
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

void AggregateSetBuilder::Process(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                                  Span<const mco_Pricing> pricings, Span<const mco_Pricing> mono_pricings,
                                  int multiplier)
{
    for (Size i = 0, j = 0; i < results.len; i++) {
        agg_parts_map.RemoveAll();

        const mco_Result &result = results[i];
        const mco_Pricing &pricing = pricings[i];
        Span<const mco_Result> sub_mono_results = mono_results.Take(j, result.stays.len);
        Span<const mco_Pricing> sub_mono_pricings = mono_pricings.Take(j, result.stays.len);
        j += result.stays.len;

        bool match = false;
        HeapArray<drd_UnitCode> agg_units(&units_alloc);
        for (Size k = 0; k < sub_mono_results.len; k++) {
            const mco_Result &mono_result = sub_mono_results[k];
            const mco_Pricing &mono_pricing = sub_mono_pricings[k];
            drd_UnitCode unit = mono_result.stays[0].unit;
            RG_ASSERT_DEBUG(mono_result.stays[0].bill_id == result.stays[0].bill_id);

            if (user->mco_allowed_units.Find(unit)) {
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
            for (drd_UnitCode unit: agg_units) {
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
                RG_ASSERT_DEBUG(agg->parts.len == agg_parts.len);
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

void AggregateSetBuilder::Finish(AggregateSet *out_set, HeapArray<mco_GhmRootCode> *out_ghm_roots)
{
    std::sort(set.aggregates.begin(), set.aggregates.end(),
              [](const Aggregate &agg1, const Aggregate &agg2) {
        return MultiCmp(agg1.key.ghm.value - agg2.key.ghm.value,
                        agg1.key.ghs.number - agg2.key.ghs.number) < 0;
    });

    SwapMemory(out_set, &set, RG_SIZE(set));
    if (out_ghm_roots) {
        std::swap(ghm_roots, *out_ghm_roots);
    }
}

static void GatherGhmGhsInfo(Span<const mco_GhmRootCode> ghm_roots, Date min_date, Date max_date,
                             drd_Sector sector, HeapArray<GhmGhsInfo> *out_ghm_ghs)
{
    HashMap<GhmGhsInfo::Key, Size> ghm_ghs_map;

    for (const mco_TableIndex &index: mco_table_set.indexes) {
        const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
            *mco_index_to_constraints[&index - mco_table_set.indexes.ptr];

        if (min_date < index.limit_dates[1] && index.limit_dates[0] < max_date) {
            for (mco_GhmRootCode ghm_root: ghm_roots) {
                Span<const mco_GhmToGhsInfo> compatible_ghs = index.FindCompatibleGhs(ghm_root);

                for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
                    const mco_GhsPriceInfo *ghs_price_info = index.FindGhsPrice(ghm_to_ghs_info.Ghs(sector), sector);
                    const mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);

                    GhmGhsInfo *agg_ghm_ghs;
                    {
                        GhmGhsInfo::Key key;
                        key.ghm = ghm_to_ghs_info.ghm;
                        key.ghs = ghm_to_ghs_info.Ghs(sector);

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
                                                  ~((1u << ghm_to_ghs_info.minimum_duration) - 1);
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

int ProduceMcoAggregate(const http_Request &request, const User *user, http_Response *out_response)
{
    if (!user) {
        LogError("Not allowed to query MCO aggregations");
        return http_ProduceErrorPage(403, out_response);
    }
    out_response->flags |= (int)http_Response::Flag::DisableCache;

    // Get query parameters
    Date period[2] = {};
    Date diff[2] = {};
    const char *filter = nullptr;
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    bool apply_coefficient = false;
    mco_GhmRootCode ghm_root = {};
    if (int code = GetQueryDateRange(request, "period", out_response, &period[0], &period[1]); code)
        return code;
    if (request.GetQueryValue("diff")) {
        if (int code = GetQueryDateRange(request, "diff", out_response, &diff[0], &diff[1]); code)
            return code;
    }
    filter = request.GetQueryValue("filter");
    if (int code = GetQueryDispenseMode(request, "dispense_mode", out_response, &dispense_mode); code)
        return code;
    if (int code = GetQueryApplyCoefficient(request, "apply_coefficient", out_response, &apply_coefficient); code)
        return code;
    if (request.GetQueryValue("ghm_root")) {
        if (int code = GetQueryGhmRoot(request, "ghm_root", out_response, &ghm_root); code)
            return code;
    }

    // Check errors and permissions
    if (diff[0].value && period[0] < diff[1] && period[1] > diff[0]) {
        LogError("Parameters 'period' and 'diff' must not overlap");
        return http_ProduceErrorPage(422, out_response);
    }
    if (!user->CheckMcoDispenseMode(dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
        return http_ProduceErrorPage(403, out_response);
    }
    if (filter && !user->CheckPermission(UserPermission::UseFilter)) {
        LogError("User is not allowed to use filters");
        return http_ProduceErrorPage(403, out_response);
    }

    // Prepare query
    McoResultProvider provider;
    int flags;
    provider.SetFilter(filter, user->CheckPermission(UserPermission::MutateFilter));
    if (ghm_root.IsValid()) {
        provider.SetGhmRoot(ghm_root);
        flags = (int)AggregationFlag::KeyOnUnits | (int)AggregationFlag::KeyOnDuration;
    } else {
        flags = (int)AggregationFlag::KeyOnUnits;
    }

    // Aggregate
    AggregateSet aggregate_set;
    HeapArray<mco_GhmRootCode> ghm_roots;
    {
        AggregateSetBuilder aggregate_set_builder(user, flags);

        // Reuse for performance
        HeapArray<mco_Pricing> pricings;
        HeapArray<mco_Pricing> mono_pricings;

        const auto aggregate_period = [&](Date min_date, Date max_date, int multiplier) {
            provider.SetDateRange(min_date, max_date);

            return provider.Run([&](Span<const mco_Result> results,
                                    Span<const mco_Result> mono_results) {
                pricings.RemoveFrom(0);
                mono_pricings.RemoveFrom(0);
                mco_Price(results, apply_coefficient, &pricings);
                mco_Dispense(pricings, mono_results, dispense_mode, &mono_pricings);

                aggregate_set_builder.Process(results, mono_results,
                                              pricings, mono_pricings, multiplier);
            });
        };

        if (!aggregate_period(period[0], period[1], 1))
            return http_ProduceErrorPage(500, out_response);
        if (diff[0].value && !aggregate_period(diff[0], diff[1], -1))
            return http_ProduceErrorPage(500, out_response);

        aggregate_set_builder.Finish(&aggregate_set, ghm_root.IsValid() ? &ghm_roots : nullptr);
    }

    // GHM and GHS info
    HeapArray<GhmGhsInfo> ghm_ghs;
    {
        Date min_date = diff[0].value ? std::min(diff[0], period[0]) : period[0];
        Date max_date = diff[0].value ? std::min(diff[1], period[1]) : period[1];
        GatherGhmGhsInfo(ghm_roots, min_date, max_date, thop_config.sector, &ghm_ghs);
    }

    // Export data
    http_JsonPageBuilder json(request.compression_type);
    char buf[32];

    json.StartObject();

    json.Key("ghs"); json.StartArray();
    for (const GhmGhsInfo &ghm_ghs_info: ghm_ghs) {
        json.StartObject();
        json.Key("ghm"); json.String(Fmt(buf, "%1", ghm_ghs_info.key.ghm).ptr);
        json.Key("ghs"); json.Int(ghm_ghs_info.key.ghs.number);
        json.Key("conditions"); json.Bool(ghm_ghs_info.ghm_to_ghs_info->conditions_count);
        json.Key("durations"); json.Uint(ghm_ghs_info.durations);

        if (ghm_ghs_info.exh_treshold) {
            json.Key("exh_treshold"); json.Int(ghm_ghs_info.exh_treshold);
        }
        if (ghm_ghs_info.exb_treshold) {
            json.Key("exb_treshold"); json.Int(ghm_ghs_info.exb_treshold);
        }
        json.EndObject();
    }
    json.EndArray();

    json.Key("rows"); json.StartArray();
    for (const Aggregate &agg: aggregate_set.aggregates) {
        json.StartObject();
        json.Key("ghm"); json.String(Fmt(buf, "%1", agg.key.ghm).ptr);
        json.Key("ghs"); json.Int(agg.key.ghs.number);
        if (flags & (int)AggregationFlag::KeyOnDuration) {
            json.Key("duration"); json.Int(agg.key.duration);
        }
        if (flags & (int)AggregationFlag::KeyOnUnits) {
            json.Key("unit"); json.StartArray();
            for (drd_UnitCode unit: agg.key.units) {
                json.Int(unit.number);
            }
            json.EndArray();
        }
        json.Key("count"); json.Int(agg.count);
        json.Key("deaths"); json.Int64(agg.deaths);
        json.Key("mono_count_total"); json.Int(agg.mono_count);
        json.Key("price_cents_total"); json.Int64(agg.price_cents);
        json.Key("mono_count"); json.StartArray();
        for (const Aggregate::Part &part: agg.parts) {
            json.Int(part.mono_count);
        }
        json.EndArray();
        json.Key("price_cents"); json.StartArray();
        for (const Aggregate::Part &part: agg.parts) {
            json.Int64(part.price_cents);
        }
        json.EndArray();
        json.EndObject();
    }
    json.EndArray();

    json.EndObject();

    return json.Finish(out_response);
}

int ProduceMcoResults(const http_Request &request, const User *user, http_Response *out_response)
{
    if (!user || !user->CheckPermission(UserPermission::FullResults)) {
        LogError("Not allowed to query MCO results");
        return http_ProduceErrorPage(403, out_response);
    }
    out_response->flags |= (int)http_Response::Flag::DisableCache;

    // Get query parameters
    Date period[2] = {};
    mco_GhmRootCode ghm_root = {};
    const char *filter;
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    bool apply_coefficent = false;
    if (int code = GetQueryDateRange(request, "period", out_response, &period[0], &period[1]); code)
        return code;
    if (int code = GetQueryGhmRoot(request, "ghm_root", out_response, &ghm_root); code)
        return code;
    filter = request.GetQueryValue("filter");
    if (int code = GetQueryDispenseMode(request, "dispense_mode", out_response, &dispense_mode); code)
        return code;
    if (int code = GetQueryApplyCoefficient(request, "apply_coefficient", out_response, &apply_coefficent); code)
        return code;

    // Check permissions
    if (!user->CheckMcoDispenseMode(dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
        return http_ProduceErrorPage(403, out_response);
    }
    if (filter && !user->CheckPermission(UserPermission::UseFilter)) {
        LogError("User is not allowed to use filters");
        return http_ProduceErrorPage(403, out_response);
    }

    // Prepare query
    McoResultProvider provider;
    provider.SetDateRange(period[0], period[1]);
    provider.SetFilter(filter, user->CheckPermission(UserPermission::MutateFilter));
    provider.SetGhmRoot(ghm_root);

    // Reuse for performance
    HeapArray<mco_Pricing> pricings;
    HeapArray<mco_Pricing> mono_pricings;

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    bool success = provider.Run([&](Span<const mco_Result> results,
                                    Span<const mco_Result> mono_results) {
        // Compute prices
        pricings.RemoveFrom(0);
        mono_pricings.RemoveFrom(0);
        mco_Price(results, apply_coefficent, &pricings);
        mco_Dispense(pricings, mono_results, dispense_mode, &mono_pricings);

        for (Size i = 0, j = 0; i < results.len; i++) {
            char buf[32];

            const mco_Result &result = results[i];
            const mco_Pricing &pricing = pricings[i];
            Span<const mco_Result> sub_mono_results = mono_results.Take(j, result.stays.len);
            Span<const mco_Pricing> sub_mono_pricings = mono_pricings.Take(j, result.stays.len);
            j += result.stays.len;

            const mco_GhmRootInfo *ghm_root_info = nullptr;
            const mco_DiagnosisInfo *main_diag_info = nullptr;
            const mco_DiagnosisInfo *linked_diag_info = nullptr;
            if (RG_LIKELY(result.index)) {
                ghm_root_info = result.index->FindGhmRoot(result.ghm.Root());
                main_diag_info = result.index->FindDiagnosis(result.stays[result.main_stay_idx].main_diagnosis);
                linked_diag_info = result.index->FindDiagnosis(result.stays[result.main_stay_idx].linked_diagnosis);
            }

            json.StartObject();

            json.Key("admin_id"); json.Int(result.stays[0].admin_id);
            json.Key("bill_id"); json.Int(result.stays[0].bill_id);
            if (RG_LIKELY(result.index)) {
                json.Key("index_date"); json.String(Fmt(buf, "%1", result.index->limit_dates[0]).ptr);
            }
            if (result.duration >= 0) {
                json.Key("duration"); json.Int(result.duration);
            }
            json.Key("sex"); json.Int(result.stays[0].sex);
            if (result.age >= 0) {
                json.Key("age"); json.Int(result.age);
            }
            json.Key("main_stay"); json.Int(result.main_stay_idx);
            json.Key("ghm"); json.String(result.ghm.ToString(buf).ptr);
            json.Key("main_error"); json.Int(result.main_error);
            json.Key("ghs"); json.Int(result.ghs.number);
            json.Key("ghs_duration"); json.Int(result.ghs_duration);
            json.Key("exb_exh"); json.Int(pricing.exb_exh);
            json.Key("price_cents"); json.Int((int)pricing.price_cents);
            json.Key("total_cents"); json.Int((int)pricing.total_cents);

            json.Key("stays"); json.StartArray();
            for (Size k = 0; k < result.stays.len; k++) {
                const mco_Stay &stay = result.stays[k];
                const mco_Result &mono_result = sub_mono_results[k];
                const mco_Pricing &mono_pricing = sub_mono_pricings[k];

                json.StartObject();

                if (mono_result.duration >= 0) {
                    json.Key("duration"); json.Int(mono_result.duration);
                }
                json.Key("unit"); json.Int(stay.unit.number);
                if (user->mco_allowed_units.Find(stay.unit)) {
                    json.Key("sex"); json.Int(stay.sex);
                    json.Key("age"); json.Int(mono_result.age);
                    json.Key("birthdate"); json.String(Fmt(buf, "%1", stay.birthdate).ptr);
                    json.Key("entry_date"); json.String(Fmt(buf, "%1", stay.entry.date).ptr);
                    json.Key("entry_mode"); json.String(&stay.entry.mode, 1);
                    if (stay.entry.origin) {
                        json.Key("entry_origin"); json.String(&stay.entry.origin, 1);
                    }
                    json.Key("exit_date"); json.String(Fmt(buf, "%1", stay.exit.date).ptr);
                    json.Key("exit_mode"); json.String(&stay.exit.mode, 1);
                    if (stay.exit.destination) {
                        json.Key("exit_destination"); json.String(&stay.exit.destination, 1);
                    }
                    if (stay.bed_authorization) {
                        json.Key("bed_authorization"); json.Int(stay.bed_authorization);
                    }
                    if (stay.session_count) {
                        json.Key("session_count"); json.Int(stay.session_count);
                    }
                    if (stay.igs2) {
                        json.Key("igs2"); json.Int(stay.igs2);
                    }
                    if (stay.last_menstrual_period.value) {
                        json.Key("last_menstrual_period"); json.String(Fmt(buf, "%1", stay.last_menstrual_period).ptr);
                    }
                    if (stay.gestational_age) {
                        json.Key("gestational_age"); json.Int(stay.gestational_age);
                    }
                    if (stay.newborn_weight) {
                        json.Key("newborn_weight"); json.Int(stay.newborn_weight);
                    }
                    if (stay.flags & (int)mco_Stay::Flag::Confirmed) {
                        json.Key("confirm"); json.Bool(true);
                    }
                    if (stay.dip_count) {
                        json.Key("dip_count"); json.Int(stay.dip_count);
                    }
                    if (stay.flags & (int)mco_Stay::Flag::UCD) {
                        json.Key("ucd"); json.Bool(stay.flags & (int)mco_Stay::Flag::UCD);
                    }

                    if (RG_LIKELY(stay.main_diagnosis.IsValid())) {
                        json.Key("main_diagnosis"); json.String(stay.main_diagnosis.str);
                    }
                    if (stay.linked_diagnosis.IsValid()) {
                        json.Key("linked_diagnosis"); json.String(stay.linked_diagnosis.str);
                    }

                    json.Key("other_diagnoses"); json.StartArray();
                    for (drd_DiagnosisCode diag: stay.other_diagnoses) {
                        const mco_DiagnosisInfo *diag_info =
                            RG_LIKELY(result.index) ? result.index->FindDiagnosis(diag) : nullptr;

                        json.StartObject();
                        json.Key("diag"); json.String(diag.str);
                        if (!result.ghm.IsError() && ghm_root_info && main_diag_info && diag_info) {
                            json.Key("severity"); json.Int(diag_info->Attributes(stay.sex).severity);

                            if (mco_TestExclusion(*result.index, stay.sex, result.age,
                                                  *diag_info, *ghm_root_info, *main_diag_info,
                                                  linked_diag_info)) {
                                json.Key("exclude"); json.Bool(true);
                            }
                        }
                        json.EndObject();
                    }
                    json.EndArray();

                    json.Key("procedures"); json.StartArray();
                    for (const mco_ProcedureRealisation &proc: stay.procedures) {
                        json.StartObject();
                        json.Key("proc"); json.String(proc.proc.str);
                        if (proc.phase) {
                            json.Key("phase"); json.Int(proc.phase);
                        }
                        json.Key("activity"); json.Int(proc.activity);
                        if (proc.extension) {
                            json.Key("extension"); json.Int(proc.extension);
                        }
                        json.String("date"); json.String(Fmt(buf, "%1", proc.date).ptr);
                        json.String("count"); json.Int(proc.count);
                        if (proc.doc) {
                            json.String("doc"); json.String(&proc.doc, 1);
                        }
                        json.EndObject();
                    }
                    json.EndArray();
                }

                json.Key("price_cents"); json.Int64(mono_pricing.price_cents);
                json.Key("total_cents"); json.Int64(mono_pricing.total_cents);

                json.EndObject();
            }
            json.EndArray();

            json.EndObject();
        }
    });
    if (!success)
        return http_ProduceErrorPage(500, out_response);
    json.EndArray();

    return json.Finish(out_response);
}

}
