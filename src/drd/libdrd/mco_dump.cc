// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "mco_dump.hh"

namespace K {

struct BuildReadableTreeContext {
    Span<const mco_GhmDecisionNode> ghm_nodes;
    Span<mco_ReadableGhmNode> out_nodes;

    int8_t cmd;

    Allocator *str_alloc;
};

static bool ProcessGhmNode(BuildReadableTreeContext &ctx, Size node_idx);
static Size ProcessGhmTest(BuildReadableTreeContext &ctx, const mco_GhmDecisionNode &ghm_node,
                           mco_ReadableGhmNode *out_node)
{
    K_ASSERT(ghm_node.function != 12);

    out_node->key = Fmt(ctx.str_alloc, "%1%2%3",
                        FmtHex(ghm_node.function, 2),
                        FmtHex(ghm_node.u.test.params[0], 2),
                        FmtHex(ghm_node.u.test.params[1], 2)).ptr;
    out_node->type = "test";

    out_node->function = ghm_node.function;
    out_node->children_idx = ghm_node.u.test.children_idx;
    out_node->children_count = ghm_node.u.test.children_count;
    K_ASSERT(out_node->children_idx <= ctx.ghm_nodes.len - out_node->children_count);

    switch (ghm_node.function) {
        case 0:
        case 1: {
            if (ghm_node.u.test.params[0] == 0) {
                out_node->text = "DP";

                int8_t prev_cmd = ctx.cmd;
                for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                    Size child_idx = ghm_node.u.test.children_idx + i;

                    ctx.cmd = (int8_t)i;
                    ctx.out_nodes[child_idx].header = Fmt(ctx.str_alloc, "D-%1", FmtInt(ctx.cmd, 2)).ptr;
                    if (!ProcessGhmNode(ctx, child_idx)) [[unlikely]]
                        return -1;
                }
                ctx.cmd = prev_cmd;

                return ghm_node.u.test.children_idx;
            } else if (ghm_node.u.test.params[0] == 1) {
                out_node->text = "DP";

                for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                    Size child_idx = ghm_node.u.test.children_idx + i;

                    ctx.out_nodes[child_idx].header = Fmt(ctx.str_alloc, "D-%1%2",
                                                          FmtInt(ctx.cmd, 2), FmtInt(i, 2)).ptr;
                    if (!ProcessGhmNode(ctx, child_idx)) [[unlikely]]
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
            // XXX: Text for test 9 is inexact
            out_node->text = Fmt(ctx.str_alloc, "Tous actes A$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 10: {
            out_node->text = Fmt(ctx.str_alloc, "2 actes A$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 13: {
            if (ghm_node.u.test.params[0] == 0) {
                out_node->text = Fmt(ctx.str_alloc, "DP D-%1", FmtInt(ghm_node.u.test.params[1], 2)).ptr;

                int8_t prev_cmd = ctx.cmd;
                for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                    Size child_idx = ghm_node.u.test.children_idx + i;

                    ctx.cmd = ghm_node.u.test.params[1];
                    if (!ProcessGhmNode(ctx, child_idx)) [[unlikely]]
                        return -1;
                }
                ctx.cmd = prev_cmd;

                return ghm_node.u.test.children_idx;
            } else if (ghm_node.u.test.params[0] == 1) {
                out_node->text = Fmt(ctx.str_alloc, "DP D-%1%2",
                                     FmtInt(ctx.cmd, 2), FmtInt(ghm_node.u.test.params[1], 2)).ptr;
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
            // XXX: Text for test 18 is inexact
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

        case 40: {
            out_node->text = "Annulation erreurs 80 et 222";
        } break;

        case 41: {
            out_node->text = Fmt(ctx.str_alloc, "DP / DR / DAS D-%1%2",
                                 FmtInt(ghm_node.u.test.params[0], 2),
                                 FmtInt(ghm_node.u.test.params[1], 2)).ptr;
        } break;

        case 42: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            out_node->text = Fmt(ctx.str_alloc, "Poids NN 1 à %1", param).ptr;
        } break;

        case 43: {
            out_node->text = Fmt(ctx.str_alloc, "DP ou DAS D-%1%2",
                                 FmtInt(ghm_node.u.test.params[0], 2),
                                 FmtInt(ghm_node.u.test.params[1], 2)).ptr;
        } break;

        default: {
            out_node->text = Fmt(ctx.str_alloc, "Test inconnu %1 (%2, %3)",
                                 ghm_node.function,
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;
    }

    for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
        Size child_idx = ghm_node.u.test.children_idx + i;
        if (!ProcessGhmNode(ctx, child_idx)) [[unlikely]]
            return -1;
    }

    return ghm_node.u.test.children_idx;
}

static bool ProcessGhmNode(BuildReadableTreeContext &ctx, Size node_idx)
{
    for (Size i = 0;; i++) {
        K_ASSERT(i < ctx.ghm_nodes.len); // Infinite loops
        K_ASSERT(node_idx < ctx.ghm_nodes.len);

        const mco_GhmDecisionNode &ghm_node = ctx.ghm_nodes[node_idx];
        mco_ReadableGhmNode *out_node = &ctx.out_nodes[node_idx];

        if (ghm_node.function != 12) {
            node_idx = ProcessGhmTest(ctx, ghm_node, out_node);
            if (node_idx < 0) [[unlikely]]
                return false;

            // GOTO is special
            if (ghm_node.function == 20)
                return true;
        } else {
            out_node->key = Fmt(ctx.str_alloc, "%1", ghm_node.u.ghm.ghm).ptr;
            out_node->type = "ghm";

            if (ghm_node.u.ghm.error) {
                out_node->text = Fmt(ctx.str_alloc, "GHM %1 [%2]",
                                     ghm_node.u.ghm.ghm, ghm_node.u.ghm.error).ptr;
            } else {
                out_node->text = Fmt(ctx.str_alloc, "GHM %1", ghm_node.u.ghm.ghm).ptr;
            }
            return true;
        }
    }

    K_UNREACHABLE();
}

// XXX: Add classifier_tree export to drdR
bool mco_BuildReadableGhmTree(Span<const mco_GhmDecisionNode> ghm_nodes, Allocator *str_alloc,
                              HeapArray<mco_ReadableGhmNode> *out_nodes)
{
    if (!ghm_nodes.len)
        return true;

    BuildReadableTreeContext ctx = {};

    ctx.ghm_nodes = ghm_nodes;
    out_nodes->Grow(ghm_nodes.len);
    ctx.out_nodes = MakeSpan(out_nodes->end(), ghm_nodes.len);
    MemSet(ctx.out_nodes.ptr, 0, ctx.out_nodes.len * K_SIZE(*ctx.out_nodes.ptr));
    ctx.str_alloc = str_alloc;

    if (!ProcessGhmNode(ctx, 0))
        return false;

    out_nodes->len += ghm_nodes.len;
    return true;
}

static void DumpReadableNodes(Span<const mco_ReadableGhmNode> readable_nodes,
                              Size node_idx, int depth, StreamWriter *out_st)
{
    for (Size i = 0;; i++) {
        K_ASSERT(i < readable_nodes.len); // Infinite loops
        K_ASSERT(node_idx < readable_nodes.len);

        const mco_ReadableGhmNode &readable_node = readable_nodes[node_idx];

        PrintLn(out_st, "    %1[%2] %3", FmtRepeat("  ", depth), node_idx, readable_node.text);

        if (readable_node.function != 20 && readable_node.children_idx) {
            for (Size j = 1; j < readable_node.children_count; j++) {
                DumpReadableNodes(readable_nodes, readable_node.children_idx + j, depth + 1, out_st);
            }

            node_idx = readable_node.children_idx;
        } else {
            // Stop at GOTO and GHM nodes
            return;
        }
    }
}

void mco_DumpGhmDecisionTree(Span<const mco_ReadableGhmNode> readable_nodes, StreamWriter *out_st)
{
    DumpReadableNodes(readable_nodes, 0, 1, out_st);
}

void mco_DumpGhmDecisionTree(Span<const mco_GhmDecisionNode> ghm_nodes, StreamWriter *out_st)
{
    BlockAllocator temp_alloc;

    HeapArray<mco_ReadableGhmNode> readable_nodes;
    mco_BuildReadableGhmTree(ghm_nodes, &temp_alloc, &readable_nodes);

    mco_DumpGhmDecisionTree(readable_nodes, out_st);
}

void mco_DumpDiagnosisTable(Span<const mco_DiagnosisInfo> diagnoses,
                            Span<const mco_ExclusionInfo> exclusions,
                            StreamWriter *out_st)
{
    for (const mco_DiagnosisInfo &diag_info: diagnoses) {
        const char *sex_str = nullptr;
        switch (diag_info.sexes) {
            case 0x1: { sex_str = " (male)"; } break;
            case 0x2: { sex_str = " (female)"; } break;
            case 0x3: { sex_str = ""; } break;
        }

        PrintLn(out_st, "      %1%2:", diag_info.diag, sex_str);
        PrintLn(out_st, "        Category: %1", diag_info.cmd);
        PrintLn(out_st, "        Severity: %1", diag_info.severity + 1);
        Print(out_st, "        Mask:");
        for (Size i = 0; i < K_LEN(diag_info.raw); i++) {
            Print(out_st, " 0b%1", FmtBin(diag_info.raw[i], 8));
        }
        PrintLn(out_st);

        if (exclusions.len) {
            K_ASSERT(diag_info.exclusion_set_idx <= exclusions.len);
            const mco_ExclusionInfo *excl_info = &exclusions[diag_info.exclusion_set_idx];

            Print(out_st, "        Exclusions (list %1):", diag_info.exclusion_set_idx);
            for (const mco_DiagnosisInfo &excl_diag: diagnoses) {
                if (excl_info->raw[excl_diag.cma_exclusion_mask.offset] &
                        excl_diag.cma_exclusion_mask.value) {
                    Print(out_st, " %1", excl_diag.diag);
                }
            }
            PrintLn(out_st);
        }
    }
}

void mco_DumpProcedureTable(Span<const mco_ProcedureInfo> procedures, StreamWriter *out_st)
{
    for (const mco_ProcedureInfo &proc: procedures) {
       char buf[512];

        PrintLn(out_st, "      %1/%2:", proc.proc, proc.phase);
        PrintLn(out_st, "        Validity: %1 to %2", proc.limit_dates[0], proc.limit_dates[1]);
        PrintLn(out_st, "        Activities: %1", proc.ActivitiesToStr(buf));
        PrintLn(out_st, "        Extensions: %1", proc.ExtensionsToStr(buf));
        Print(out_st, "        Mask: ");
        for (Size i = 0; i < K_LEN(proc.bytes); i++) {
            Print(out_st, " 0b%1", FmtBin(proc.bytes[i], 8));
        }
        PrintLn(out_st);
    }
}

void mco_DumpGhmRootTable(Span<const mco_GhmRootInfo> ghm_roots, StreamWriter *out_st)
{
    for (const mco_GhmRootInfo &ghm_root: ghm_roots) {
        PrintLn(out_st, "      GHM root %1:", ghm_root.ghm_root);

        if (ghm_root.confirm_duration_threshold) {
            PrintLn(out_st, "        Confirm if < %1 days (except for deaths and MCO transfers)",
                    ghm_root.confirm_duration_threshold);
        }

        if (ghm_root.allow_ambulatory) {
            PrintLn(out_st, "        Can be ambulatory (J)");
        }
        if (ghm_root.short_duration_threshold) {
            PrintLn(out_st, "        Can be short duration (T) if < %1 days",
                    ghm_root.short_duration_threshold);
        }

        if (ghm_root.young_age_threshold) {
            PrintLn(out_st, "        Increase severity if age < %1 years and severity < %2",
                    ghm_root.young_age_threshold, ghm_root.young_severity_limit + 1);
        }
        if (ghm_root.old_age_threshold) {
            PrintLn(out_st, "        Increase severity if age >= %1 years and severity < %2",
                    ghm_root.old_age_threshold, ghm_root.old_severity_limit + 1);
        }

        if (ghm_root.childbirth_severity_list) {
            PrintLn(out_st, "        Childbirth severity list %1", ghm_root.childbirth_severity_list);
        }
    }
}

void mco_DumpGhmToGhsTable(Span<const mco_GhmToGhsInfo> ghs, StreamWriter *out_st)
{
    mco_GhmCode previous_ghm = {};
    for (const mco_GhmToGhsInfo &ghm_to_ghs_info: ghs) {
        if (ghm_to_ghs_info.ghm != previous_ghm) {
            PrintLn(out_st, "      GHM %1:", ghm_to_ghs_info.ghm);
            previous_ghm = ghm_to_ghs_info.ghm;
        }
        PrintLn(out_st, "        GHS %1 (public) / GHS %2 (private)",
                ghm_to_ghs_info.Ghs(drd_Sector::Public), ghm_to_ghs_info.Ghs(drd_Sector::Private));

        if (ghm_to_ghs_info.unit_authorization) {
            PrintLn(out_st, "          Requires unit authorization %1", ghm_to_ghs_info.unit_authorization);
        }
        if (ghm_to_ghs_info.bed_authorization) {
            PrintLn(out_st, "          Requires bed authorization %1", ghm_to_ghs_info.bed_authorization);
        }
        if (ghm_to_ghs_info.minimum_duration) {
            PrintLn(out_st, "          Requires duration >= %1 days", ghm_to_ghs_info.minimum_duration);
        }
        if (ghm_to_ghs_info.minimum_age) {
            PrintLn(out_st, "          Requires age >= %1 years", ghm_to_ghs_info.minimum_age);
        }
        if (ghm_to_ghs_info.main_diagnosis_mask.value) {
            PrintLn(out_st, "          Main Diagnosis List D$%1.%2",
                    ghm_to_ghs_info.main_diagnosis_mask.offset,
                    ghm_to_ghs_info.main_diagnosis_mask.value);
        }
        if (ghm_to_ghs_info.diagnosis_mask.value) {
            PrintLn(out_st, "          Diagnosis List D$%1.%2",
                    ghm_to_ghs_info.diagnosis_mask.offset, ghm_to_ghs_info.diagnosis_mask.value);
        }
        for (const drd_ListMask &mask: ghm_to_ghs_info.procedure_masks) {
            PrintLn(out_st, "          Procedure List A$%1.%2", mask.offset, mask.value);
        }
    }
}

void mco_DumpGhsPriceTable(Span<const mco_GhsPriceInfo> ghs_prices, StreamWriter *out_st)
{
    for (const mco_GhsPriceInfo &price_info: ghs_prices) {
        PrintLn(out_st, "        GHS %1: %2 [exh = %3, exb = %4%5%6]", price_info.ghs,
                FmtDouble(price_info.ghs_cents / 100.0, 2),
                FmtDouble(price_info.exh_cents / 100.0, 2),
                FmtDouble(price_info.exb_cents / 100.0, 2),
                (price_info.flags & (int)mco_GhsPriceInfo::Flag::ExbOnce) ? "*" : "",
                (price_info.flags & (int)mco_GhsPriceInfo::Flag::Minoration) ? ", minoration" : "");
    }
}

void mco_DumpSeverityTable(Span<const mco_ValueRangeCell<2>> cells, StreamWriter *out_st)
{
    for (const mco_ValueRangeCell<2> &cell: cells) {
        PrintLn(out_st, "      %1-%2 and %3-%4 = %5",
                cell.limits[0].min, cell.limits[0].max, cell.limits[1].min, cell.limits[1].max,
                cell.value);
    }
}

void mco_DumpAuthorizationTable(Span<const mco_AuthorizationInfo> authorizations,
                                StreamWriter *out_st)
{
    for (const mco_AuthorizationInfo &auth: authorizations) {
        PrintLn(out_st, "      %1 [%2] => Function %3",
                auth.type.st.code, mco_AuthorizationScopeNames[(int)auth.type.st.scope], auth.function);
    }
}

void DumpSupplementPairTable(Span<const mco_SrcPair> pairs, StreamWriter *out_st)
{
    for (const mco_SrcPair &pair: pairs) {
        PrintLn(out_st, "      %1 -- %2", pair.diag, pair.proc);
    }
}

void mco_DumpTableSetHeaders(const mco_TableSet &table_set, StreamWriter *out_st)
{
    PrintLn(out_st, "Headers:");
    for (const mco_TableInfo &table: table_set.tables) {
        PrintLn(out_st, "  Table '%1' build %2:", mco_TableTypeNames[(int)table.type], table.build_date);
        PrintLn(out_st, "    Source: %1", table.filename);
        PrintLn(out_st, "    Raw Type: %1", table.raw_type);
        PrintLn(out_st, "    Version: %1.%2", table.version[0], table.version[1]);
        PrintLn(out_st, "    Validity: %1 to %2", table.limit_dates[0], table.limit_dates[1]);
        PrintLn(out_st, "    Sections:");
        for (Size i = 0; i < table.sections.len; i++) {
            PrintLn(out_st, "      %1. 0x%2 -- %3 bytes -- %4 elements (%5 bytes / element)",
                    i, FmtHex((uint64_t)table.sections[i].raw_offset), table.sections[i].raw_len,
                    table.sections[i].values_count, table.sections[i].value_len);
        }
        PrintLn(out_st);
    }

    PrintLn(out_st, "Index:");
    for (const mco_TableIndex &index: table_set.indexes) {
        PrintLn(out_st, "  %1 to %2%3:", index.limit_dates[0], index.limit_dates[1],
                                 index.valid ? "" : " (incomplete)");
        for (const mco_TableInfo *table: index.tables) {
            if (!table)
                continue;

            PrintLn(out_st, "    %1: %2.%3 [%4 -- %5, build: %6]",
                    mco_TableTypeNames[(int)table->type], table->version[0], table->version[1],
                    table->limit_dates[0], table->limit_dates[1], table->build_date);
        }
        PrintLn(out_st);
    }
}

void mco_DumpTableSetContent(const mco_TableSet &table_set, StreamWriter *out_st)
{
    PrintLn(out_st, "Content:");
    for (const mco_TableIndex &index: table_set.indexes) {
        PrintLn(out_st, "  %1 to %2%3:", index.limit_dates[0], index.limit_dates[1],
                                     index.valid ? "" : " (incomplete)");
        // We don't really need to loop here, but we want the switch to get
        // warnings when we introduce new table types.
        for (Size i = 0; i < K_LEN(index.tables); i++) {
            if (!index.tables[i])
                continue;

            switch ((mco_TableType)i) {
                case mco_TableType::GhmDecisionTree: {
                    PrintLn(out_st, "    GHM Decision Tree:");
                    mco_DumpGhmDecisionTree(index.ghm_nodes, out_st);
                    PrintLn(out_st);
                } break;
                case mco_TableType::DiagnosisTable: {
                    PrintLn(out_st, "    Diagnoses:");
                    mco_DumpDiagnosisTable(index.diagnoses, index.exclusions, out_st);
                    PrintLn(out_st);
                } break;
                case mco_TableType::ProcedureTable: {
                    PrintLn(out_st, "    Procedures:");
                    mco_DumpProcedureTable(index.procedures, out_st);
                    PrintLn(out_st);
                } break;
                case mco_TableType::ProcedureAdditionTable: {} break;
                case mco_TableType::ProcedureExtensionTable: {} break;
                case mco_TableType::GhmRootTable: {
                    PrintLn(out_st, "    GHM Roots:");
                    mco_DumpGhmRootTable(index.ghm_roots, out_st);
                    PrintLn(out_st);
                } break;
                case mco_TableType::SeverityTable: {
                    PrintLn(out_st, "    GNN Table:");
                    mco_DumpSeverityTable(index.gnn_cells, out_st);
                    PrintLn(out_st);

                    for (Size j = 0; j < K_LEN(index.cma_cells); j++) {
                        PrintLn(out_st, "    CMA Table %1:", j + 1);
                        mco_DumpSeverityTable(index.cma_cells[j], out_st);
                        PrintLn(out_st);
                    }
                } break;

                case mco_TableType::GhmToGhsTable: {
                    PrintLn(out_st, "    GHM To GHS Table:");
                    mco_DumpGhmToGhsTable(index.ghs, out_st);
                } break;
                case mco_TableType::AuthorizationTable: {
                    PrintLn(out_st, "    Authorization Types:");
                    mco_DumpAuthorizationTable(index.authorizations, out_st);
                } break;
                case mco_TableType::SrcPairTable: {
                    for (Size j = 0; j < K_LEN(index.src_pairs); j++) {
                        PrintLn(out_st, "    Supplement Pairs List %1:", j + 1);
                        DumpSupplementPairTable(index.src_pairs[j], out_st);
                        PrintLn(out_st);
                    }
                } break;

                case mco_TableType::PriceTablePublic:
                case mco_TableType::PriceTablePrivate: {
                    PrintLn(out_st, "    %1:", mco_TableTypeNames[i]);
                    Size sector_idx = i - (int)mco_TableType::PriceTablePublic;
                    mco_DumpGhsPriceTable(index.ghs_prices[sector_idx], out_st);
                } break;
                case mco_TableType::GhsMinorationTable: {} break;

                case mco_TableType::UnknownTable:
                    break;
            }
        }
        PrintLn(out_st);
    }
}

}
