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

int ProduceMcoCaseMix(const ConnectionInfo *conn, const char *, Response *out_response)
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

int ProduceMcoClassify(const ConnectionInfo *conn, const char *, Response *out_response)
{
    if (!thop_stay_set.stays.len || !conn->user)
        return CreateErrorPage(404, out_response);

    struct CellSummary {
        mco_GhmCode ghm;
        int16_t ghs;
        int16_t duration;
        int count;
        int partial_mono_count;
        int mono_count;
        int64_t partial_price_cents;
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

    HeapArray<CellSummary> summary;
    {
        Size j = 0;
        HashMap<int64_t, Size> summary_map;
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
                    // TODO: Careful with duration overflow
                    SummaryMapKey key = {};
                    key.st.ghm = result.ghm;
                    key.st.ghs = result.ghs;
                    if (durations) {
                        key.st.duration = (int16_t)result.duration;
                    }

                    CellSummary *cell;
                    {
                        std::pair<Size *, bool> ret = summary_map.Append(key.value, summary.len);
                        if (ret.second) {
                            CellSummary cell_summary = {};
                            cell_summary.ghm = result.ghm;
                            cell_summary.ghs = result.ghs.number;
                            cell_summary.duration = key.st.duration;
                            summary.Append(cell_summary);
                        }

                        cell = &summary[*ret.first];
                    }

                    if (!counted_rss) {
                        cell->count += multiplier;
                        cell->mono_count += multiplier * result.stays.len;
                        cell->price_cents += multiplier * pricing.price_cents;
                        if (result.stays[result.stays.len - 1].exit.mode == '9') {
                            cell->deaths += multiplier;
                        }
                        counted_rss = true;
                    }
                    cell->partial_mono_count += multiplier;
                    cell->partial_price_cents += multiplier * mono_pricing.price_cents;
                }
            }
        }
    }

    std::sort(summary.begin(), summary.end(), [](const CellSummary &cs1, const CellSummary &cs2) {
        return MultiCmp(cs1.ghm.value - cs2.ghm.value,
                        cs1.ghs - cs2.ghs,
                        cs1.duration - cs2.duration) < 0;
    });

    out_response->flags |= (int)Response::Flag::DisableETag;
    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
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
            writer.Key("partial_mono_count"); writer.Int(cs.partial_mono_count);
            writer.Key("mono_count"); writer.Int(cs.mono_count);
            writer.Key("deaths"); writer.Int64(cs.deaths);
            writer.Key("partial_price_cents"); writer.Int64(cs.partial_price_cents);
            writer.Key("price_cents"); writer.Int64(cs.price_cents);
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    }, conn->compression_type, out_response);
}

static int GetIndexFromRequest(const ConnectionInfo *conn, const char *redirect_url,
                               Response *out_response, const mco_TableIndex **out_index)
{
    Date date = {};
    {
        const char *date_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "date");
        if (date_str) {
            date = Date::FromString(date_str);
        } else {
            LogError("Missing 'date' parameter");
        }
        if (!date.value)
            return CreateErrorPage(422, out_response);
    }

    const mco_TableIndex *index = thop_table_set->FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        return CreateErrorPage(404, out_response);
    }

    // Redirect to the canonical URL for this version, to improve client-side caching
    if (redirect_url && date != index->limit_dates[0]) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        out_response->response.reset(response);

        {
            char url_buf[64];
            Fmt(url_buf, "%1?date=%2", redirect_url, index->limit_dates[0]);
            MHD_add_response_header(response, "Location", url_buf);
        }

        return 303;
    }

    *out_index = index;
    return 0;
}

int ProduceMcoIndexes(const ConnectionInfo *conn, const char *, Response *out_response)
{
    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        writer.StartArray();
        for (const mco_TableIndex &index: thop_table_set->indexes) {
            if (!index.valid)
                continue;

            char buf[32];

            writer.StartObject();
            writer.Key("begin_date"); writer.String(Fmt(buf, "%1", index.limit_dates[0]).ptr);
            writer.Key("end_date"); writer.String(Fmt(buf, "%1", index.limit_dates[1]).ptr);
            if (index.changed_tables & ~MaskEnum(mco_TableType::PriceTablePublic)) {
                writer.Key("changed_tables"); writer.Bool(true);
            }
            if (index.changed_tables & MaskEnum(mco_TableType::PriceTablePublic)) {
                writer.Key("changed_prices"); writer.Bool(true);
            }
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    }, conn->compression_type, out_response);
}

int ProduceMcoDiagnoses(const ConnectionInfo *conn, const char *url, Response *out_response)
{
    const mco_TableIndex *index;
    {
        int code = GetIndexFromRequest(conn, url, out_response, &index);
        if (code)
            return code;
    }

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Diagnoses);
    {
        const char *spec_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "spec");
        if (spec_str) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Diagnoses) {
                LogError("Invalid diagnosis list specifier '%1'", spec_str);
                return CreateErrorPage(422, out_response);
            }
        }
    }

    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[512];

        const auto WriteSexSpecificInfo = [&](const mco_DiagnosisInfo &diag_info,
                                              int sex) {
            if (diag_info.Attributes(sex).cmd) {
                writer.Key("cmd");
                writer.String(Fmt(buf, "D-%1",
                                  FmtArg(diag_info.Attributes(sex).cmd).Pad0(-2)).ptr);
            }
            if (diag_info.Attributes(sex).cmd && diag_info.Attributes(sex).jump) {
                writer.Key("main_list");
                writer.String(Fmt(buf, "D-%1%2",
                                  FmtArg(diag_info.Attributes(sex).cmd).Pad0(-2),
                                  FmtArg(diag_info.Attributes(sex).jump).Pad0(-2)).ptr);
            }
            if (diag_info.Attributes(sex).severity) {
                writer.Key("severity"); writer.Int(diag_info.Attributes(sex).severity);
            }
        };

        writer.StartArray();
        for (const mco_DiagnosisInfo &diag_info: index->diagnoses) {
            if (diag_info.flags & (int)mco_DiagnosisInfo::Flag::SexDifference) {
                if (spec.Match(diag_info.Attributes(1).raw)) {
                    writer.StartObject();
                    writer.Key("diag"); writer.String(diag_info.diag.str);
                    writer.Key("sex"); writer.String("Homme");
                    WriteSexSpecificInfo(diag_info, 1);
                    writer.EndObject();
                }

                if (spec.Match(diag_info.Attributes(2).raw)) {
                    writer.StartObject();
                    writer.Key("diag"); writer.String(diag_info.diag.str);
                    writer.Key("sex"); writer.String("Femme");
                    WriteSexSpecificInfo(diag_info, 2);
                    writer.EndObject();
                }
            } else if (spec.Match(diag_info.Attributes(1).raw)) {
                writer.StartObject();
                writer.Key("diag"); writer.String(diag_info.diag.str);
                WriteSexSpecificInfo(diag_info, 1);
                writer.EndObject();
            }
        }
        writer.EndArray();
        return true;
    }, conn->compression_type, out_response);
}

int ProduceMcoProcedures(const ConnectionInfo *conn, const char *url, Response *out_response)
{
    const mco_TableIndex *index;
    {
        int code = GetIndexFromRequest(conn, url, out_response, &index);
        if (code)
            return code;
    }

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Procedures);
    {
        const char *spec_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "spec");
        if (spec_str) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Procedures) {
                LogError("Invalid procedure list specifier '%1'", spec_str);
                return CreateErrorPage(422, out_response);
            }
        }
    }

    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[512];

        writer.StartArray();
        for (const mco_ProcedureInfo &proc_info: index->procedures) {
            if (spec.Match(proc_info.bytes)) {
                writer.StartObject();
                writer.Key("proc"); writer.String(proc_info.proc.str);
                writer.Key("begin_date"); writer.String(Fmt(buf, "%1", proc_info.limit_dates[0]).ptr);
                writer.Key("end_date"); writer.String(Fmt(buf, "%1", proc_info.limit_dates[1]).ptr);
                writer.Key("phase"); writer.Int(proc_info.phase);
                writer.Key("activities"); writer.Int(proc_info.ActivitiesToDec());
                if (proc_info.extensions > 1) {
                    writer.Key("extensions"); writer.Int(proc_info.ExtensionsToDec());
                }
                writer.EndObject();
            }
        }
        writer.EndArray();
        return true;
    }, conn->compression_type, out_response);
}

int ProduceMcoGhmGhs(const ConnectionInfo *conn, const char *url, Response *out_response)
{
    const mco_TableIndex *index;
    {
        int code = GetIndexFromRequest(conn, url, out_response, &index);
        if (code)
            return code;
    }

    const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
        *thop_index_to_constraints[index - thop_table_set->indexes.ptr];

    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[512];

        writer.StartArray();
        for (const mco_GhmRootInfo &ghm_root_info: index->ghm_roots) {
            Span<const mco_GhmToGhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
            for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
                mco_GhsCode ghs = ghm_to_ghs_info.Ghs(Sector::Public);

                const mco_GhsPriceInfo *ghs_price_info = index->FindGhsPrice(ghs, Sector::Public);
                const mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);
                if (!constraint)
                    continue;

                uint32_t combined_durations = constraint->durations &
                                              ~((1u << ghm_to_ghs_info.minimal_duration) - 1);

                writer.StartObject();

                writer.Key("ghm"); writer.String(ghm_to_ghs_info.ghm.ToString(buf).ptr);
                writer.Key("ghm_root"); writer.String(ghm_to_ghs_info.ghm.Root().ToString(buf).ptr);
                if (ghm_root_info.young_severity_limit) {
                    writer.Key("young_age_treshold"); writer.Int(ghm_root_info.young_age_treshold);
                    writer.Key("young_severity_limit"); writer.Int(ghm_root_info.young_severity_limit);
                }
                if (ghm_root_info.old_severity_limit) {
                    writer.Key("old_age_treshold"); writer.Int(ghm_root_info.old_age_treshold);
                    writer.Key("old_severity_limit"); writer.Int(ghm_root_info.old_severity_limit);
                }
                writer.Key("durations"); writer.Uint(combined_durations);

                writer.Key("ghs"); writer.Int(ghm_to_ghs_info.Ghs(Sector::Public).number);
                if ((combined_durations & 1) &&
                        (constraint->warnings & (int)mco_GhmConstraint::Warning::PreferCmd28)) {
                    writer.Key("warn_cmd28"); writer.Bool(true);
                }
                if (ghm_root_info.confirm_duration_treshold) {
                    writer.Key("confirm_treshold"); writer.Int(ghm_root_info.confirm_duration_treshold);
                }
                if (ghm_to_ghs_info.unit_authorization) {
                    writer.Key("unit_authorization"); writer.Int(ghm_to_ghs_info.unit_authorization);
                }
                if (ghm_to_ghs_info.bed_authorization) {
                    writer.Key("bed_authorization"); writer.Int(ghm_to_ghs_info.bed_authorization);
                }
                if (ghm_to_ghs_info.minimal_duration) {
                    writer.Key("minimum_duration"); writer.Int(ghm_to_ghs_info.minimal_duration);
                }
                if (ghm_to_ghs_info.minimal_age) {
                    writer.Key("minimum_age"); writer.Int(ghm_to_ghs_info.minimal_age);
                }
                if (ghm_to_ghs_info.main_diagnosis_mask.value) {
                    writer.Key("main_diagnosis");
                    writer.String(Fmt(buf, "D$%1.%2",
                                      ghm_to_ghs_info.main_diagnosis_mask.offset,
                                      ghm_to_ghs_info.main_diagnosis_mask.value).ptr);
                }
                if (ghm_to_ghs_info.diagnosis_mask.value) {
                    writer.Key("diagnoses");
                    writer.String(Fmt(buf, "D$%1.%2",
                                      ghm_to_ghs_info.diagnosis_mask.offset,
                                      ghm_to_ghs_info.diagnosis_mask.value).ptr);
                }
                if (ghm_to_ghs_info.procedure_masks.len) {
                    writer.Key("procedures"); writer.StartArray();
                    for (const ListMask &mask: ghm_to_ghs_info.procedure_masks) {
                        writer.String(Fmt(buf, "A$%1.%2", mask.offset, mask.value).ptr);
                    }
                    writer.EndArray();
                }

                if (ghs_price_info) {
                    writer.Key("ghs_cents"); writer.Int(ghs_price_info->ghs_cents);
                    writer.Key("ghs_coefficient"); writer.Double(index->GhsCoefficient(Sector::Public));
                    if (ghs_price_info->exh_treshold) {
                        writer.Key("exh_treshold"); writer.Int(ghs_price_info->exh_treshold);
                        writer.Key("exh_cents"); writer.Int(ghs_price_info->exh_cents);
                    }
                    if (ghs_price_info->exb_treshold) {
                        writer.Key("exb_treshold"); writer.Int(ghs_price_info->exb_treshold);
                        writer.Key("exb_cents"); writer.Int(ghs_price_info->exb_cents);
                        if (ghs_price_info->flags & (int)mco_GhsPriceInfo::Flag::ExbOnce) {
                            writer.Key("exb_once"); writer.Bool(true);
                        }
                    }
                }

                writer.EndObject();
            }
        }
        writer.EndArray();
        return true;
    }, conn->compression_type, out_response);
}

struct ReadableGhmDecisionNode {
    const char *key;
    const char *header;
    const char *text;
    const char *reverse;

    uint8_t function;
    Size children_idx;
    Size children_count;
};

struct BuildReadableGhmTreeContext {
    Span<const mco_GhmDecisionNode> ghm_nodes;
    Span<ReadableGhmDecisionNode> out_nodes;

    int8_t cmd;

    Allocator *str_alloc;
};

static bool ProcessGhmNode(BuildReadableGhmTreeContext &ctx, Size ghm_node_idx);
static Size ProcessGhmTest(BuildReadableGhmTreeContext &ctx,
                           const mco_GhmDecisionNode &ghm_node,
                           ReadableGhmDecisionNode *out_node)
{
    DebugAssert(ghm_node.type == mco_GhmDecisionNode::Type::Test);

    out_node->key = Fmt(ctx.str_alloc, "%1%2%3",
                        FmtHex(ghm_node.u.test.function).Pad0(-2),
                        FmtHex(ghm_node.u.test.params[0]).Pad0(-2),
                        FmtHex(ghm_node.u.test.params[1]).Pad0(-2)).ptr;

    // FIXME: Check children_idx and children_count
    out_node->function = ghm_node.u.test.function;
    out_node->children_idx = ghm_node.u.test.children_idx;
    out_node->children_count = ghm_node.u.test.children_count;

    switch (ghm_node.u.test.function) {
        case 0:
        case 1: {
            if (ghm_node.u.test.params[0] == 0) {
                out_node->text = "DP";

                int8_t prev_cmd = ctx.cmd;
                for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                    Size child_idx = ghm_node.u.test.children_idx + i;

                    ctx.cmd = (int8_t)i;
                    ctx.out_nodes[child_idx].header = Fmt(ctx.str_alloc, "D-%1",
                                                          FmtArg(ctx.cmd).Pad0(-2)).ptr;
                    if (UNLIKELY(!ProcessGhmNode(ctx, child_idx)))
                        return -1;
                }
                ctx.cmd = prev_cmd;

                return ghm_node.u.test.children_idx;
            } else if (ghm_node.u.test.params[0] == 1) {
                out_node->text = "DP";

                for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                    Size child_idx = ghm_node.u.test.children_idx + i;

                    ctx.out_nodes[child_idx].header = Fmt(ctx.str_alloc, "D-%1%2",
                                                          FmtArg(ctx.cmd).Pad0(-2),
                                                          FmtArg(i).Pad0(-2)).ptr;
                    if (UNLIKELY(!ProcessGhmNode(ctx, child_idx)))
                        return -1;
                }

                return ghm_node.u.test.children_idx;
            } else {
                out_node->text = Fmt(ctx.str_alloc, "DP (byte %1)",
                                     ghm_node.u.test.params[0]).ptr;
            }
        } break;

        case 2: {
            out_node->text = Fmt(ctx.str_alloc, "Acte A$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 3: {
            if (ghm_node.u.test.params[1] == 1) {
                out_node->text = Fmt(ctx.str_alloc, "Age (jours) > %1",
                                     ghm_node.u.test.params[0]).ptr;
                if (ghm_node.u.test.params[0] == 7) {
                    out_node->reverse = "Age (jours) ≤ 7";
                }
            } else {
                out_node->text = Fmt(ctx.str_alloc, "Age > %1",
                                     ghm_node.u.test.params[0]).ptr;
            }
        } break;

        case 5: {
            out_node->text = Fmt(ctx.str_alloc, "DP D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 6: {
            out_node->text = Fmt(ctx.str_alloc, "DAS D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 7: {
            out_node->text = Fmt(ctx.str_alloc, "DP / DR / DAS D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 9: {
            // TODO: Text for test 9 is inexact
            out_node->text = Fmt(ctx.str_alloc, "Tous actes A$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 10: {
            out_node->text = Fmt(ctx.str_alloc, "2 actes A$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 13: {
            if (ghm_node.u.test.params[0] == 0) {
                out_node->text = Fmt(ctx.str_alloc, "DP D-%1",
                                     FmtArg(ghm_node.u.test.params[1]).Pad0(-2)).ptr;

                int8_t prev_cmd = ctx.cmd;
                for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                    Size child_idx = ghm_node.u.test.children_idx + i;

                    ctx.cmd = ghm_node.u.test.params[1];
                    if (UNLIKELY(!ProcessGhmNode(ctx, child_idx)))
                        return -1;
                }
                ctx.cmd = prev_cmd;

                return ghm_node.u.test.children_idx;
            } else if (ghm_node.u.test.params[0] == 1) {
                out_node->text = Fmt(ctx.str_alloc, "DP D-%1%2",
                                     FmtArg(ctx.cmd).Pad0(-2),
                                     FmtArg(ghm_node.u.test.params[1]).Pad0(-2)).ptr;
            } else {
                out_node->text = Fmt(ctx.str_alloc, "DP byte %1 = %2",
                                     ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
            }
        } break;

        case 14: {
            switch (ghm_node.u.test.params[0]) {
                case '1': { out_node->text = "Sexe = Homme"; } break;
                case '2': { out_node->text = "Sexe = Femme"; } break;
                default: { return -1; } break;
            }
        } break;

        case 18: {
            // TODO: Text for test 18 is inexact
            out_node->text = Fmt(ctx.str_alloc, "2 DAS D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 19: {
            switch (ghm_node.u.test.params[1]) {
                case 0: { out_node->text = Fmt(ctx.str_alloc, "Mode de sortie = %1",
                                               ghm_node.u.test.params[0]).ptr; } break;
                case 1: { out_node->text = Fmt(ctx.str_alloc, "Destination = %1",
                                               ghm_node.u.test.params[0]).ptr; } break;
                case 2: { out_node->text = Fmt(ctx.str_alloc, "Mode d'entrée = %1",
                                               ghm_node.u.test.params[0]).ptr; } break;
                case 3: { out_node->text = Fmt(ctx.str_alloc, "Provenance = %1",
                                               ghm_node.u.test.params[0]).ptr; } break;
                default: { return -1; } break;
            }
        } break;

        case 20: {
            out_node->text = Fmt(ctx.str_alloc, "Saut noeud %1",
                                 ghm_node.u.test.children_idx).ptr;
        } break;

        case 22: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            out_node->text = Fmt(ctx.str_alloc, "Durée < %1", param).ptr;
        } break;

        case 26: {
            out_node->text = Fmt(ctx.str_alloc, "DR D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 28: {
            out_node->text = Fmt(ctx.str_alloc, "Erreur non bloquante %1",
                                 ghm_node.u.test.params[0]).ptr;
        } break;

        case 29: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            out_node->text = Fmt(ctx.str_alloc, "Durée = %1", param).ptr;
        } break;

        case 30: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            out_node->text = Fmt(ctx.str_alloc, "Nombre de séances = %1", param).ptr;
            if (param == 0) {
                out_node->reverse = "Nombre de séances > 0";
            }
        } break;

        case 33: {
            out_node->text = Fmt(ctx.str_alloc, "Acte avec activité %1",
                                 ghm_node.u.test.params[0]).ptr;
        } break;

        case 34: {
            out_node->text = "Inversion DP / DR";
        } break;

        case 35: {
            out_node->text = "DP / DR inversés";
        } break;

        case 36: {
            out_node->text = Fmt(ctx.str_alloc, "DP ou DAS D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 38: {
            if (ghm_node.u.test.params[0] == ghm_node.u.test.params[1]) {
                out_node->text = Fmt(ctx.str_alloc, "GNN = %1", ghm_node.u.test.params[0]).ptr;
            } else {
                out_node->text = Fmt(ctx.str_alloc, "GNN %1 à %2",
                                     ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
            }
        } break;

        case 39: {
            out_node->text = "Calcul du GNN";
        } break;

        case 41: {
            out_node->text = Fmt(ctx.str_alloc, "DP / DR / DAS D-%1%2",
                                 FmtArg(ghm_node.u.test.params[0]).Pad0(-2),
                                 FmtArg(ghm_node.u.test.params[1]).Pad0(-2)).ptr;
        } break;

        case 42: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            out_node->text = Fmt(ctx.str_alloc, "Poids NN 1 à %1", param).ptr;
        } break;

        case 43: {
            out_node->text = Fmt(ctx.str_alloc, "DP ou DAS D-%1%2",
                                 FmtArg(ghm_node.u.test.params[0]).Pad0(-2),
                                 FmtArg(ghm_node.u.test.params[1]).Pad0(-2)).ptr;
        } break;

        default: {
            out_node->text = Fmt(ctx.str_alloc, "Test inconnu %1 (%2, %3)",
                                 ghm_node.u.test.function,
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;
    }

    for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
        Size child_idx = ghm_node.u.test.children_idx + i;
        if (UNLIKELY(!ProcessGhmNode(ctx, child_idx)))
            return -1;
    }

    return ghm_node.u.test.children_idx;
}

static bool ProcessGhmNode(BuildReadableGhmTreeContext &ctx, Size ghm_node_idx)
{
    for (Size i = 0;; i++) {
        if (UNLIKELY(i >= ctx.ghm_nodes.len)) {
            LogError("Empty GHM tree or infinite loop (%2)", ctx.ghm_nodes.len);
            return false;
        }

        DebugAssert(ghm_node_idx < ctx.ghm_nodes.len);
        const mco_GhmDecisionNode &ghm_node = ctx.ghm_nodes[ghm_node_idx];
        ReadableGhmDecisionNode *out_node = &ctx.out_nodes[ghm_node_idx];

        switch (ghm_node.type) {
            case mco_GhmDecisionNode::Type::Test: {
                ghm_node_idx = ProcessGhmTest(ctx, ghm_node, out_node);
                if (UNLIKELY(ghm_node_idx < 0))
                    return false;

                // GOTO is special
                if (ghm_node.u.test.function == 20)
                    return true;
            } break;

            case mco_GhmDecisionNode::Type::Ghm: {
                out_node->key = Fmt(ctx.str_alloc, "%1", ghm_node.u.ghm.ghm).ptr;
                if (ghm_node.u.ghm.error) {
                    out_node->text = Fmt(ctx.str_alloc, "GHM %1 [%2]",
                                         ghm_node.u.ghm.ghm, ghm_node.u.ghm.error).ptr;
                } else {
                    out_node->text = Fmt(ctx.str_alloc, "GHM %1", ghm_node.u.ghm.ghm).ptr;
                }
                return true;
            } break;
        }
    }

    return true;
}

// TODO: Move to libdrd, add classifier_tree export to drdR
static bool BuildReadableGhmTree(Span<const mco_GhmDecisionNode> ghm_nodes,
                                 HeapArray<ReadableGhmDecisionNode> *out_nodes,
                                 Allocator *str_alloc)
{
    if (!ghm_nodes.len)
        return true;

    BuildReadableGhmTreeContext ctx = {};

    ctx.ghm_nodes = ghm_nodes;
    out_nodes->Grow(ghm_nodes.len);
    ctx.out_nodes = MakeSpan(out_nodes->end(), ghm_nodes.len);
    memset(ctx.out_nodes.ptr, 0, ctx.out_nodes.len * SIZE(*ctx.out_nodes.ptr));
    ctx.str_alloc = str_alloc;

    if (!ProcessGhmNode(ctx, 0))
        return false;

    out_nodes->len += ghm_nodes.len;
    return true;
}

int ProduceMcoTree(const ConnectionInfo *conn, const char *url, Response *out_response)
{
    const mco_TableIndex *index;
    {
        int code = GetIndexFromRequest(conn, url, out_response, &index);
        if (code)
            return code;
    }

    // TODO: Generate ahead of time
    LinkedAllocator readable_nodes_alloc;
    HeapArray<ReadableGhmDecisionNode> readable_nodes;
    if (!BuildReadableGhmTree(index->ghm_nodes, &readable_nodes, &readable_nodes_alloc))
        return CreateErrorPage(500, out_response);

    return BuildJson([&](rapidjson::Writer<JsonStreamWriter> &writer) {
        LinkedAllocator temp_alloc;

        writer.StartArray();
        for (const ReadableGhmDecisionNode &readable_node: readable_nodes) {
            writer.StartObject();
            if (readable_node.header) {
                writer.Key("header"); writer.String(readable_node.header);
            }
            writer.Key("text"); writer.String(readable_node.text);
            if (readable_node.reverse) {
                writer.Key("reverse"); writer.String(readable_node.reverse);
            }
            if (readable_node.children_idx) {
                writer.Key("key"); writer.String(readable_node.key);
                writer.Key("test"); writer.Int(readable_node.function);
                writer.Key("children_idx"); writer.Int64(readable_node.children_idx);
                writer.Key("children_count"); writer.Int64(readable_node.children_count);
            }
            writer.EndObject();
        }
        writer.EndArray();
        return true;
    }, conn->compression_type, out_response);
}
