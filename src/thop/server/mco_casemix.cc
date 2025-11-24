// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"
#include "mco_casemix.hh"
#include "mco.hh"
#include "thop.hh"
#include "user.hh"

namespace K {

static bool GetQueryDateRange(http_IO *io, const char *key, LocalDate *out_start_date, LocalDate *out_end_date)
{
    const http_RequestInfo &request = io->Request();

    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        io->SendError(422);
        return false;
    }

    LocalDate start_date = {};
    LocalDate end_date = {};
    {
        Span<const char> remain = str;

        if (!ParseDate(remain, &start_date, (int)ParseFlag::Validate, &remain))
            goto invalid;
        if (remain.len < 2 || remain[0] != '.' || remain[1] != '.')
            goto invalid;
        if (!ParseDate(remain.Take(2, remain.len - 2), &end_date, (int)ParseFlag::Validate, &remain))
            goto invalid;
        if (remain.len)
            goto invalid;

        if (end_date <= start_date)
            goto invalid;
    }

    *out_start_date = start_date;
    *out_end_date = end_date;
    return true;

invalid:
    LogError("Invalid date range '%1'", str);
    io->SendError(422);
    return false;
}

static bool GetQueryDispenseMode(http_IO *io, const char *key, mco_DispenseMode *out_dispense_mode)
{
    const http_RequestInfo &request = io->Request();

    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        io->SendError(422);
        return false;
    }

    if (!OptionToEnumI(mco_DispenseModeOptions, str, out_dispense_mode)) {
        LogError("Invalid '%1' parameter value '%2'", key, str);
        io->SendError(422);
        return false;
    }

    return true;
}

static bool GetQueryApplyCoefficient(http_IO *io, const char *key, bool *out_apply_coefficient)
{
    const http_RequestInfo &request = io->Request();

    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        io->SendError(422);
        return false;
    }

    bool apply_coefficient;
    if (TestStr(str, "1")) {
        apply_coefficient = true;
    } else if (TestStr(str, "0")) {
        apply_coefficient = false;
    } else {
        LogError("Invalid '%1' parameter value '%2'", key, str);
        io->SendError(422);
        return false;
    }

    *out_apply_coefficient = apply_coefficient;
    return true;
}

static bool GetQueryGhmRoot(http_IO *io, const char *key, mco_GhmRootCode *out_ghm_root)
{
    const http_RequestInfo &request = io->Request();

    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        io->SendError(422);
        return false;
    }

    mco_GhmRootCode ghm_root = mco_GhmRootCode::Parse(str);
    if (!ghm_root.IsValid()) {
        io->SendError(422);
        return false;
    }

    *out_ghm_root = ghm_root;
    return true;
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

    K_HASHTABLE_HANDLER(Aggregate, key);
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
    K_DELETE_COPY(AggregateSetBuilder)

    const User *user;
    unsigned int flags;

    AggregateSet set;
    HeapArray<mco_GhmRootCode> ghm_roots;

    HashMap<Aggregate::Key, Size> aggregates_map;
    HashSet<mco_GhmRootCode> ghm_roots_set;
    BlockAllocator units_alloc { Kibibytes(4) };
    BlockAllocator parts_alloc { Kibibytes(16) };

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
    int16_t exh_threshold;
    int16_t exb_threshold;
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
            K_ASSERT(mono_result.stays[0].bill_id == result.stays[0].bill_id);

            if (user->mco_allowed_units.Find(unit)) {
                bool inserted;
                auto bucket = agg_parts_map.InsertOrGetDefault(mono_result.stays[0].unit, &inserted);

                bucket->value.mono_count += multiplier;
                bucket->value.price_cents += multiplier * mono_pricing.price_cents;

                if ((flags & (int)AggregationFlag::KeyOnUnits) && inserted) {
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
                bool inserted;
                Size *ptr = aggregates_map.InsertOrGet(key, set.aggregates.len, &inserted);

                if (inserted) {
                    agg = set.aggregates.AppendDefault();
                    agg->key = key;
                } else {
                    agg = &set.aggregates[*ptr];
                }
            }

            agg->count += multiplier;
            agg->deaths += multiplier * (result.stays[result.stays.len - 1].exit.mode == '9');
            agg->mono_count += multiplier * (int32_t)result.stays.len;
            agg->price_cents += multiplier * pricing.price_cents;
            if (agg->parts.ptr) {
                K_ASSERT(agg->parts.len == agg_parts.len);
                for (Size k = 0; k < agg->parts.len; k++) {
                    agg->parts[k].mono_count += agg_parts[k].mono_count;
                    agg->parts[k].price_cents += agg_parts[k].price_cents;
                }
            } else {
                agg->parts = agg_parts.TrimAndLeak();
            }

            bool inserted = ghm_roots_set.InsertOrFail(result.ghm.Root());

            if (inserted) {
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

    units_alloc.GiveTo(&set.array_alloc);
    parts_alloc.GiveTo(&set.array_alloc);

    std::swap(*out_set, set);
    if (out_ghm_roots) {
        std::swap(ghm_roots, *out_ghm_roots);
    }
}

static void GatherGhmGhsInfo(Span<const mco_GhmRootCode> ghm_roots, LocalDate min_date, LocalDate max_date,
                             drd_Sector sector, HeapArray<GhmGhsInfo> *out_ghm_ghs)
{
    HashMap<GhmGhsInfo::Key, Size> ghm_ghs_map;

    for (const mco_TableIndex &index: mco_table_set.indexes) {
        const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
            *mco_cache_set.index_to_constraints.FindValue(&index, nullptr);

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

                        bool inserted;
                        Size *ptr = ghm_ghs_map.InsertOrGet(key, out_ghm_ghs->len, &inserted);

                        if (inserted) {
                            agg_ghm_ghs = out_ghm_ghs->AppendDefault();
                            agg_ghm_ghs->key = key;
                            agg_ghm_ghs->ghm_to_ghs_info = &ghm_to_ghs_info;
                        } else {
                            agg_ghm_ghs = &(*out_ghm_ghs)[*ptr];
                        }
                    }

                    if (constraint) {
                        agg_ghm_ghs->durations |= constraint->durations &
                                                  ~((1u << ghm_to_ghs_info.minimum_duration) - 1);
                    }

                    if (ghs_price_info) {
                        if (!agg_ghm_ghs->exh_threshold ||
                                ghs_price_info->exh_threshold < agg_ghm_ghs->exh_threshold) {
                            agg_ghm_ghs->exh_threshold = ghs_price_info->exh_threshold;
                        }
                        if (!agg_ghm_ghs->exb_threshold ||
                                ghs_price_info->exb_threshold > agg_ghm_ghs->exb_threshold) {
                            agg_ghm_ghs->exb_threshold = ghs_price_info->exb_threshold;
                        }
                    }
                }
            }
        }
    }
}

void ProduceMcoAggregate(http_IO *io, const User *user)
{
    const http_RequestInfo &request = io->Request();

    if (!user || !user->CheckPermission(UserPermission::McoCasemix)) {
        LogError("Not allowed to query MCO aggregations");
        io->SendError(403);
        return;
    }

    // Get query parameters
    LocalDate period[2] = {};
    LocalDate diff[2] = {};
    const char *filter = nullptr;
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    bool apply_coefficient = false;
    mco_GhmRootCode ghm_root = {};
    if (!GetQueryDateRange(io, "period", &period[0], &period[1]))
        return;
    if (request.GetQueryValue("diff")) {
        if (!GetQueryDateRange(io, "diff", &diff[0], &diff[1]))
            return;
    }
    filter = request.GetQueryValue("filter");
    if (!GetQueryDispenseMode(io, "dispense_mode", &dispense_mode))
        return;
    if (!GetQueryApplyCoefficient(io, "apply_coefficient", &apply_coefficient))
        return;
    if (request.GetQueryValue("ghm_root")) {
        if (!GetQueryGhmRoot(io, "ghm_root", &ghm_root))
            return;
    }

    // Check errors and permissions
    if (diff[0].value && period[0] < diff[1] && period[1] > diff[0]) {
        LogError("Parameters 'period' and 'diff' must not overlap");
        io->SendError(422);
        return;
    }
    if (!user->CheckMcoDispenseMode(dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
        io->SendError(403);
        return;
    }
    if (filter && !user->CheckPermission(UserPermission::McoFilter)) {
        LogError("User is not allowed to use filters");
        io->SendError(403);
        return;
    }

    // Prepare query
    McoResultProvider provider;
    int flags;
    provider.SetFilter(filter, user->CheckPermission(UserPermission::McoMutate));
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

        const auto aggregate_period = [&](LocalDate min_date, LocalDate max_date, int multiplier) {
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

        if (!aggregate_period(period[0], period[1], 1)) {
            io->SendError(422);
            return;
        }
        if (diff[0].value && !aggregate_period(diff[0], diff[1], -1)) {
            io->SendError(422);
            return;
        }

        aggregate_set_builder.Finish(&aggregate_set, ghm_root.IsValid() ? &ghm_roots : nullptr);
    }

    // GHM and GHS info
    HeapArray<GhmGhsInfo> ghm_ghs;
    {
        LocalDate min_date = diff[0].value ? std::min(diff[0], period[0]) : period[0];
        LocalDate max_date = diff[0].value ? std::min(diff[1], period[1]) : period[1];
        GatherGhmGhsInfo(ghm_roots, min_date, max_date, thop_config.sector, &ghm_ghs);
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        char buf[32];

        json->StartObject();

        json->Key("ghs"); json->StartArray();
        for (const GhmGhsInfo &ghm_ghs_info: ghm_ghs) {
            json->StartObject();
            json->Key("ghm"); json->String(Fmt(buf, "%1", ghm_ghs_info.key.ghm).ptr);
            json->Key("ghs"); json->Int(ghm_ghs_info.key.ghs.number);
            json->Key("conditions"); json->Bool(ghm_ghs_info.ghm_to_ghs_info->conditions_count);
            json->Key("durations"); json->Uint(ghm_ghs_info.durations);

            if (ghm_ghs_info.exh_threshold) {
                json->Key("exh_threshold"); json->Int(ghm_ghs_info.exh_threshold);
            }
            if (ghm_ghs_info.exb_threshold) {
                json->Key("exb_threshold"); json->Int(ghm_ghs_info.exb_threshold);
            }
            json->EndObject();
        }
        json->EndArray();

        json->Key("rows"); json->StartArray();
        for (const Aggregate &agg: aggregate_set.aggregates) {
            json->StartObject();
            json->Key("ghm"); json->String(Fmt(buf, "%1", agg.key.ghm).ptr);
            json->Key("ghs"); json->Int(agg.key.ghs.number);
            if (flags & (int)AggregationFlag::KeyOnDuration) {
                json->Key("duration"); json->Int(agg.key.duration);
            }
            if (flags & (int)AggregationFlag::KeyOnUnits) {
                json->Key("unit"); json->StartArray();
                for (drd_UnitCode unit: agg.key.units) {
                    json->Int(unit.number);
                }
                json->EndArray();
            }
            json->Key("count"); json->Int(agg.count);
            json->Key("deaths"); json->Int64(agg.deaths);
            json->Key("mono_count_total"); json->Int(agg.mono_count);
            json->Key("price_cents_total"); json->Int64(agg.price_cents);
            json->Key("mono_count"); json->StartArray();
            for (const Aggregate::Part &part: agg.parts) {
                json->Int(part.mono_count);
            }
            json->EndArray();
            json->Key("price_cents"); json->StartArray();
            for (const Aggregate::Part &part: agg.parts) {
                json->Int64(part.price_cents);
            }
            json->EndArray();
            json->EndObject();
        }
        json->EndArray();

        json->EndObject();
    });
}

void ProduceMcoResults(http_IO *io, const User *user)
{
    const http_RequestInfo &request = io->Request();

    if (!user || !user->CheckPermission(UserPermission::McoCasemix) ||
                 !user->CheckPermission(UserPermission::McoResults)) {
        LogError("Not allowed to query MCO results");
        io->SendError(403);
        return;
    }

    // Get query parameters
    LocalDate period[2] = {};
    mco_GhmRootCode ghm_root = {};
    const char *filter;
    mco_DispenseMode dispense_mode = mco_DispenseMode::J;
    bool apply_coefficent = false;
    if (!GetQueryDateRange(io, "period", &period[0], &period[1]))
        return;
    if (!GetQueryGhmRoot(io, "ghm_root", &ghm_root))
        return;
    filter = request.GetQueryValue("filter");
    if (!GetQueryDispenseMode(io, "dispense_mode", &dispense_mode))
        return;
    if (!GetQueryApplyCoefficient(io, "apply_coefficient", &apply_coefficent))
        return;

    // Check permissions
    if (!user->CheckMcoDispenseMode(dispense_mode)) {
        LogError("User is not allowed to use this dispensation mode");
        io->SendError(403);
        return;
    }
    if (filter && !user->CheckPermission(UserPermission::McoFilter)) {
        LogError("User is not allowed to use filters");
        io->SendError(403);
        return;
    }

    // Prepare query
    McoResultProvider provider;
    provider.SetDateRange(period[0], period[1]);
    provider.SetFilter(filter, user->CheckPermission(UserPermission::McoMutate));
    provider.SetGhmRoot(ghm_root);

    // Reuse for performance
    HeapArray<mco_Pricing> pricings;
    HeapArray<mco_Pricing> mono_pricings;

    HeapArray<uint8_t> buf;
    {
        // Buffer JSON before sending because we may fail
        buf.allocator = io->Allocator();
        buf.Grow(Mebibytes(1));

        StreamWriter st;
        CompressionType encoding;
        if (!io->NegociateEncoding(CompressionType::Brotli, CompressionType::Gzip, &encoding))
            return;
        if (!st.Open(&buf, "<json>", 0, encoding))
            return;
        json_Writer json(&st);

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
                if (result.index) [[likely]] {
                    const mco_Stay &main_stay = result.stays[result.main_stay_idx];

                    ghm_root_info = result.index->FindGhmRoot(result.ghm.Root());
                    main_diag_info = result.index->FindDiagnosis(main_stay.main_diagnosis, main_stay.sex);
                    linked_diag_info = result.index->FindDiagnosis(main_stay.linked_diagnosis, main_stay.sex);
                }

                json.StartObject();

                json.Key("admin_id"); json.Int(result.stays[0].admin_id);
                json.Key("bill_id"); json.Int(result.stays[0].bill_id);
                if (result.index) [[likely]] {
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
                        if (stay.flags & (int)mco_Stay::Flag::RAAC) {
                            json.Key("raac"); json.Bool(true);
                        }
                        if (stay.flags & (int)mco_Stay::Flag::UCD) {
                            json.Key("ucd"); json.Bool(stay.flags & (int)mco_Stay::Flag::UCD);
                        }
                        if (stay.dip_count) {
                            json.Key("dip_count"); json.Int(stay.dip_count);
                        }

                        if (stay.main_diagnosis.IsValid()) [[likely]] {
                            json.Key("main_diagnosis"); json.String(stay.main_diagnosis.str);
                        }
                        if (stay.linked_diagnosis.IsValid()) {
                            json.Key("linked_diagnosis"); json.String(stay.linked_diagnosis.str);
                        }

                        json.Key("other_diagnoses"); json.StartArray();
                        for (drd_DiagnosisCode diag: stay.other_diagnoses) {
                            const mco_DiagnosisInfo *diag_info =
                                result.index ? result.index->FindDiagnosis(diag, stay.sex) : nullptr;

                            json.StartObject();
                            json.Key("diag"); json.String(diag.str);
                            if (!result.ghm.IsError() && ghm_root_info && main_diag_info && diag_info) {
                                json.Key("severity"); json.Int(diag_info->severity);

                                if (mco_TestExclusion(*result.index, result.age, *diag_info,
                                                      *ghm_root_info, *main_diag_info, linked_diag_info)) {
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
        if (!success) {
            io->SendError(422);
            return;
        }

        json.EndArray();

        if (st.Close()) {
            Span<const uint8_t> json = buf.Leak();

            io->AddEncodingHeader(encoding);
            io->SendBinary(200, json, "application/json");
        }
    }
}

}
