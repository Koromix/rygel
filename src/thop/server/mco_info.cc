// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "mco_info.hh"
#include "mco.hh"

static int GetIndexFromRequest(const http_Request &request, http_Response *out_response,
                               const mco_TableIndex **out_index, drd_Sector *out_sector = nullptr)
{
    Date date = {};
    {
        const char *date_str = request.GetQueryValue("date");
        if (date_str) {
            date = Date::FromString(date_str);
        } else {
            LogError("Missing 'date' parameter");
        }
        if (!date.value)
            return http_ProduceErrorPage(422, out_response);
    }

    drd_Sector sector = drd_Sector::Public;
    if (out_sector) {
        const char *sector_str = request.GetQueryValue("sector");
        if (!sector_str) {
            LogError("Missing 'sector' parameter");
            return http_ProduceErrorPage(422, out_response);
        } else if (TestStr(sector_str, "public")) {
            sector = drd_Sector::Public;
        } else if (TestStr(sector_str, "private")) {
            sector = drd_Sector::Private;
        } else {
            LogError("Invalid 'sector' parameter");
            return http_ProduceErrorPage(422, out_response);
        }
    }

    const mco_TableIndex *index = mco_table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        return http_ProduceErrorPage(404, out_response);
    }

    // Redirect to the canonical URL for this version, to improve client-side caching
    if (date != index->limit_dates[0]) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        out_response->response.reset(response);

        {
            char url_buf[64];
            Fmt(url_buf, "%1?date=%2", request.url, index->limit_dates[0]);
            MHD_add_response_header(response, "Location", url_buf);
        }

        return 303;
    }

    *out_index = index;
    if (out_sector) {
        *out_sector = sector;
    }
    return 0;
}

int ProduceMcoDiagnoses(const http_Request &request, const User *, http_Response *out_response)
{
    const mco_TableIndex *index;
    if (int code = GetIndexFromRequest(request, out_response, &index); code)
        return code;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Diagnoses);
    {
        const char *spec_str = request.GetQueryValue("spec");
        if (spec_str) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Diagnoses) {
                LogError("Invalid diagnosis list specifier '%1'", spec_str);
                return http_ProduceErrorPage(422, out_response);
            }
        }
    }

    http_JsonPageBuilder json(request.compression_type);
    char buf[512];

    const auto WriteSexSpecificInfo = [&](const mco_DiagnosisInfo &diag_info,
                                          int sex) {
        if (diag_info.Attributes(sex).cmd) {
            json.Key("cmd");
            json.String(Fmt(buf, "D-%1",
                              FmtArg(diag_info.Attributes(sex).cmd).Pad0(-2)).ptr);
        }
        if (diag_info.Attributes(sex).cmd && diag_info.Attributes(sex).jump) {
            json.Key("main_list");
            json.String(Fmt(buf, "D-%1%2",
                              FmtArg(diag_info.Attributes(sex).cmd).Pad0(-2),
                              FmtArg(diag_info.Attributes(sex).jump).Pad0(-2)).ptr);
        }
        if (diag_info.Attributes(sex).severity) {
            json.Key("severity"); json.Int(diag_info.Attributes(sex).severity);
        }
    };

    json.StartArray();
    for (const mco_DiagnosisInfo &diag_info: index->diagnoses) {
        if (diag_info.flags & (int)mco_DiagnosisInfo::Flag::SexDifference) {
            if (spec.Match(diag_info.Attributes(1).raw)) {
                json.StartObject();
                json.Key("diag"); json.String(diag_info.diag.str);
                json.Key("sex"); json.String("Homme");
                WriteSexSpecificInfo(diag_info, 1);
                json.EndObject();
            }

            if (spec.Match(diag_info.Attributes(2).raw)) {
                json.StartObject();
                json.Key("diag"); json.String(diag_info.diag.str);
                json.Key("sex"); json.String("Femme");
                WriteSexSpecificInfo(diag_info, 2);
                json.EndObject();
            }
        } else if (spec.Match(diag_info.Attributes(1).raw)) {
            json.StartObject();
            json.Key("diag"); json.String(diag_info.diag.str);
            WriteSexSpecificInfo(diag_info, 1);
            json.EndObject();
        }
    }
    json.EndArray();

    return json.Finish(out_response);
}

int ProduceMcoProcedures(const http_Request &request, const User *, http_Response *out_response)
{
    const mco_TableIndex *index;
    if (int code = GetIndexFromRequest(request, out_response, &index); code)
        return code;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Procedures);
    {
        const char *spec_str = request.GetQueryValue("spec");
        if (spec_str) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Procedures) {
                LogError("Invalid procedure list specifier '%1'", spec_str);
                return http_ProduceErrorPage(422, out_response);
            }
        }
    }

    http_JsonPageBuilder json(request.compression_type);
    char buf[512];

    json.StartArray();
    for (const mco_ProcedureInfo &proc_info: index->procedures) {
        if (spec.Match(proc_info.bytes)) {
            json.StartObject();
            json.Key("proc"); json.String(proc_info.proc.str);
            json.Key("begin_date"); json.String(Fmt(buf, "%1", proc_info.limit_dates[0]).ptr);
            json.Key("end_date"); json.String(Fmt(buf, "%1", proc_info.limit_dates[1]).ptr);
            json.Key("phase"); json.Int(proc_info.phase);
            json.Key("activities"); json.Int(proc_info.ActivitiesToDec());
            if (proc_info.extensions > 1) {
                json.Key("extensions"); json.Int(proc_info.ExtensionsToDec());
            }
            json.EndObject();
        }
    }
    json.EndArray();

    return json.Finish(out_response);
}

int ProduceMcoGhmGhs(const http_Request &request, const User *, http_Response *out_response)
{
    const mco_TableIndex *index;
    drd_Sector sector;
    if (int code = GetIndexFromRequest(request, out_response, &index, &sector); code)
        return code;

    const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
        *mco_index_to_constraints[index - mco_table_set.indexes.ptr];

    http_JsonPageBuilder json(request.compression_type);
    char buf[512];

    json.StartArray();
    for (const mco_GhmRootInfo &ghm_root_info: index->ghm_roots) {
        Span<const mco_GhmToGhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
        for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
            mco_GhsCode ghs = ghm_to_ghs_info.Ghs(sector);

            const mco_GhsPriceInfo *ghs_price_info = index->FindGhsPrice(ghs, sector);
            const mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);

            uint32_t combined_durations = 0;
            if (constraint) {
                combined_durations = constraint->durations &
                                     ~((1u << ghm_to_ghs_info.minimum_duration) - 1);
            }

            json.StartObject();

            json.Key("ghm"); json.String(ghm_to_ghs_info.ghm.ToString(buf).ptr);
            json.Key("ghm_root"); json.String(ghm_to_ghs_info.ghm.Root().ToString(buf).ptr);
            if (ghm_root_info.young_severity_limit) {
                json.Key("young_age_treshold"); json.Int(ghm_root_info.young_age_treshold);
                json.Key("young_severity_limit"); json.Int(ghm_root_info.young_severity_limit);
            }
            if (ghm_root_info.old_severity_limit) {
                json.Key("old_age_treshold"); json.Int(ghm_root_info.old_age_treshold);
                json.Key("old_severity_limit"); json.Int(ghm_root_info.old_severity_limit);
            }
            json.Key("durations"); json.Uint(combined_durations);

            json.Key("ghs"); json.Int(ghs.number);
            if ((combined_durations & 1) && constraint &&
                    (constraint->warnings & (int)mco_GhmConstraint::Warning::PreferCmd28)) {
                json.Key("warn_cmd28"); json.Bool(true);
            }
            if (ghm_root_info.confirm_duration_treshold) {
                json.Key("confirm_treshold"); json.Int(ghm_root_info.confirm_duration_treshold);
            }
            if (ghm_to_ghs_info.unit_authorization) {
                json.Key("unit_authorization"); json.Int(ghm_to_ghs_info.unit_authorization);
            }
            if (ghm_to_ghs_info.bed_authorization) {
                json.Key("bed_authorization"); json.Int(ghm_to_ghs_info.bed_authorization);
            }
            if (ghm_to_ghs_info.minimum_duration) {
                json.Key("minimum_duration"); json.Int(ghm_to_ghs_info.minimum_duration);
            }
            if (ghm_to_ghs_info.minimum_age) {
                json.Key("minimum_age"); json.Int(ghm_to_ghs_info.minimum_age);
            }
            if (ghm_to_ghs_info.main_diagnosis_mask.value) {
                json.Key("main_diagnosis");
                json.String(Fmt(buf, "D$%1.%2",
                                  ghm_to_ghs_info.main_diagnosis_mask.offset,
                                  ghm_to_ghs_info.main_diagnosis_mask.value).ptr);
            }
            if (ghm_to_ghs_info.diagnosis_mask.value) {
                json.Key("diagnoses");
                json.String(Fmt(buf, "D$%1.%2",
                                  ghm_to_ghs_info.diagnosis_mask.offset,
                                  ghm_to_ghs_info.diagnosis_mask.value).ptr);
            }
            if (ghm_to_ghs_info.procedure_masks.len) {
                json.Key("procedures"); json.StartArray();
                for (const ListMask &mask: ghm_to_ghs_info.procedure_masks) {
                    json.String(Fmt(buf, "A$%1.%2", mask.offset, mask.value).ptr);
                }
                json.EndArray();
            }

            if (ghs_price_info) {
                if (ghs_price_info->flags & (int)mco_GhsPriceInfo::Flag::Minoration) {
                    json.Key("warn_ucd"); json.Bool(true);
                }
                json.Key("ghs_cents"); json.Int(ghs_price_info->ghs_cents);
                json.Key("ghs_coefficient"); json.Double(index->GhsCoefficient(sector));
                if (ghs_price_info->exh_treshold) {
                    json.Key("exh_treshold"); json.Int(ghs_price_info->exh_treshold);
                    json.Key("exh_cents"); json.Int(ghs_price_info->exh_cents);
                }
                if (ghs_price_info->exb_treshold) {
                    json.Key("exb_treshold"); json.Int(ghs_price_info->exb_treshold);
                    json.Key("exb_cents"); json.Int(ghs_price_info->exb_cents);
                    if (ghs_price_info->flags & (int)mco_GhsPriceInfo::Flag::ExbOnce) {
                        json.Key("exb_once"); json.Bool(true);
                    }
                }
            }

            json.EndObject();
        }
    }
    json.EndArray();

    return json.Finish(out_response);
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

int ProduceMcoTree(const http_Request &request, const User *, http_Response *out_response)
{
    const mco_TableIndex *index;
    if (int code = GetIndexFromRequest(request, out_response, &index); code)
        return code;

    // TODO: Generate ahead of time
    BlockAllocator readable_nodes_alloc;
    HeapArray<ReadableGhmDecisionNode> readable_nodes;
    if (!BuildReadableGhmTree(index->ghm_nodes, &readable_nodes, &readable_nodes_alloc))
        return http_ProduceErrorPage(500, out_response);

    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    for (const ReadableGhmDecisionNode &readable_node: readable_nodes) {
        json.StartObject();
        if (readable_node.header) {
            json.Key("header"); json.String(readable_node.header);
        }
        json.Key("text"); json.String(readable_node.text);
        if (readable_node.reverse) {
            json.Key("reverse"); json.String(readable_node.reverse);
        }
        if (readable_node.children_idx) {
            json.Key("key"); json.String(readable_node.key);
            json.Key("test"); json.Int(readable_node.function);
            json.Key("children_idx"); json.Int64(readable_node.children_idx);
            json.Key("children_count"); json.Int64(readable_node.children_count);
        }
        json.EndObject();
    }
    json.EndArray();

    return json.Finish(out_response);
}
