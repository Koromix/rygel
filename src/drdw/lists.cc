// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "drdw.hh"

Response ProduceIndexes(MHD_Connection *, const char *, CompressionType compression_type)
{
    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        writer.StartArray();
        for (const mco_TableIndex &index: drdw_table_set->indexes) {
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
    });

    return {200, response};
}

struct ReadableGhmDecisionNode {
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

                    ctx.cmd = i;
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
            out_node->text = Fmt(ctx.str_alloc, "Saut vers noeud %1",
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
            out_node->text = Fmt(ctx.str_alloc, "GNN ≥ %1 et ≤ %2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
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
            out_node->text = Fmt(ctx.str_alloc, "Poids (NN) > 0 et < %1", param).ptr;
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

Response ProduceClassifierTree(MHD_Connection *conn, const char *, CompressionType compression_type)
{
    Response response = {};
    const mco_TableIndex *index = GetIndexFromQueryString(conn, "classifier_tree.json", &response);
    if (!index)
        return response;

    // TODO: Generate ahead of time
    LinkedAllocator readable_nodes_alloc;
    HeapArray<ReadableGhmDecisionNode> readable_nodes;
    if (!BuildReadableGhmTree(index->ghm_nodes, &readable_nodes, &readable_nodes_alloc))
        return CreateErrorPage(500);

    response.code = 200;
    response.response = BuildJson(compression_type,
                                  [&](rapidjson::Writer<JsonStreamWriter> &writer) {
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
            writer.Key("test"); writer.Int(readable_node.function);
            if (readable_node.children_idx) {
                writer.Key("children_idx"); writer.Int64(readable_node.children_idx);
                writer.Key("children_count"); writer.Int64(readable_node.children_count);
            }
            writer.EndObject();
        }
        writer.EndArray();
        return true;
    });

    return response;
}

Response ProduceDiagnoses(MHD_Connection *conn, const char *, CompressionType compression_type)
{
    Response response = {};
    const mco_TableIndex *index = GetIndexFromQueryString(conn, "diagnoses.json", &response);
    if (!index)
        return response;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Diagnoses);
    {
        const char *spec_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "spec");
        if (spec_str) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Diagnoses) {
                LogError("Invalid diagnosis list specifier '%1'", spec_str);
                return CreateErrorPage(422);
            }
        }
    }

    response.code = 200;
    response.response = BuildJson(compression_type,
                                  [&](rapidjson::Writer<JsonStreamWriter> &writer) {
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
    });

    return response;
}

Response ProduceProcedures(MHD_Connection *conn, const char *, CompressionType compression_type)
{
    Response response = {};
    const mco_TableIndex *index = GetIndexFromQueryString(conn, "procedures.json", &response);
    if (!index)
        return response;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Procedures);
    {
        const char *spec_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "spec");
        if (spec_str) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Procedures) {
                LogError("Invalid procedure list specifier '%1'", spec_str);
                return CreateErrorPage(422);
            }
        }
    }

    response.code = 200;
    response.response = BuildJson(compression_type,
                                  [&](rapidjson::Writer<JsonStreamWriter> &writer) {
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
    });

    return response;
}

// TODO: Add ghm_ghs export to drdR
Response ProduceGhmGhs(MHD_Connection *conn, const char *, CompressionType compression_type)
{
    Response response = {};
    const mco_TableIndex *index = GetIndexFromQueryString(conn, "ghm_ghs.json", &response);
    if (!index)
        return response;

    const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
        *drdw_index_to_constraints[index - drdw_table_set->indexes.ptr];

    response.code = 200;
    response.response = BuildJson(compression_type,
                                  [&](rapidjson::Writer<JsonStreamWriter> &writer) {
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

                writer.Key("ghm"); writer.String(Fmt(buf, "%1", ghm_to_ghs_info.ghm).ptr);
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
    });

    return response;
}
