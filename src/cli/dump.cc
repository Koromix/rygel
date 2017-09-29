/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../core/libmoya.hh"
#include "dump.hh"

void DumpGhmDecisionTree(ArrayRef<const GhmDecisionNode> ghm_nodes,
                         size_t node_idx, int depth)
{
    while (node_idx < ghm_nodes.len) {
        const GhmDecisionNode &ghm_node = ghm_nodes[node_idx];

        switch (ghm_node.type) {
            case GhmDecisionNode::Type::Test: {
                PrintLn("      %1%2. %3(%4, %5) => %6 [%7]", FmtArg("  ").Repeat(depth), node_idx,
                        ghm_node.u.test.function, ghm_node.u.test.params[0],
                        ghm_node.u.test.params[1], ghm_node.u.test.children_idx,
                        ghm_node.u.test.children_count);

                if (ghm_node.u.test.function != 20) {
                    for (size_t i = 1; i < ghm_node.u.test.children_count; i++) {
                        DumpGhmDecisionTree(ghm_nodes, ghm_node.u.test.children_idx + i, depth + 1);
                    }

                    node_idx = ghm_node.u.test.children_idx;
                } else {
                    return;
                }
            } break;

            case GhmDecisionNode::Type::Ghm: {
                if (ghm_node.u.ghm.error) {
                    PrintLn("      %1%2. %3 (err = %4)", FmtArg("  ").Repeat(depth), node_idx,
                            ghm_node.u.ghm.code, ghm_node.u.ghm.error);
                } else {
                    PrintLn("      %1%2. %3", FmtArg("  ").Repeat(depth), node_idx,
                            ghm_node.u.ghm.code);
                }
                return;
            } break;
        }
    }
}

void DumpDiagnosisTable(ArrayRef<const DiagnosisInfo> diagnoses,
                        ArrayRef<const ExclusionInfo> exclusions)
{
    for (const DiagnosisInfo &diag: diagnoses) {
        const auto DumpMask = [&](Sex sex) {
            for (size_t i = 0; i < CountOf(diag.Attributes(sex).raw); i++) {
                Print(" %1", FmtBin(diag.Attributes(sex).raw[i]));
            }
            PrintLn();
        };

        PrintLn("      %1:", diag.code);
        if (diag.flags & (int)DiagnosisInfo::Flag::SexDifference) {
            PrintLn("        Male:");
            PrintLn("          Category: %1", diag.Attributes(Sex::Male).cmd);
            PrintLn("          Severity: %1", diag.Attributes(Sex::Male).severity + 1);
            Print("          Mask:");
            DumpMask(Sex::Male);

            PrintLn("        Female:");
            PrintLn("          Category: %1", diag.Attributes(Sex::Female).cmd);
            PrintLn("          Severity: %1", diag.Attributes(Sex::Female).severity + 1);
            Print("          Mask:");
            DumpMask(Sex::Female);
        } else {
            PrintLn("        Category: %1", diag.Attributes(Sex::Male).cmd);
            PrintLn("        Severity: %1", diag.Attributes(Sex::Male).severity + 1);
            Print("        Mask:");
            DumpMask(Sex::Male);
        }
        PrintLn("        Warnings: %1", FmtBin(diag.warnings));

        if (exclusions.len) {
            Print("        Exclusions (list %1):", diag.exclusion_set_idx);
            {
                if (diag.exclusion_set_idx <= exclusions.len) {
                    const ExclusionInfo *excl = &exclusions[diag.exclusion_set_idx];
                    for (const DiagnosisInfo &excl_diag: diagnoses) {
                        if (excl->raw[excl_diag.cma_exclusion_offset] & excl_diag.cma_exclusion_mask) {
                            Print(" %1", excl_diag.code);
                        }
                    }
                } else {
                    Print("Invalid list");
                }
            }
            PrintLn();
        }
    }
}

void DumpProcedureTable(ArrayRef<const ProcedureInfo> procedures)
{
    for (const ProcedureInfo &proc: procedures) {
        Print("      %1/%2 =", proc.code, proc.phase);
        for (size_t i = 0; i < CountOf(proc.bytes); i++) {
            Print(" %1", FmtBin(proc.bytes[i]));
        }
        PrintLn();

        PrintLn("        Validity: %1 to %2", proc.limit_dates[0], proc.limit_dates[1]);
    }
}

void DumpGhmRootTable(ArrayRef<const GhmRootInfo> ghm_roots)
{
    for (const GhmRootInfo &ghm_root: ghm_roots) {
        PrintLn("      %1:", ghm_root.code);

        if (ghm_root.confirm_duration_treshold) {
            PrintLn("        Confirm if < %1 days (except for deaths and MCO transfers)",
                    ghm_root.confirm_duration_treshold);
        }

        if (ghm_root.allow_ambulatory) {
            PrintLn("        Can be ambulatory (J)");
        }
        if (ghm_root.short_duration_treshold) {
            PrintLn("        Can be short duration (T) if < %1 days",
                    ghm_root.short_duration_treshold);
        }

        if (ghm_root.young_age_treshold) {
            PrintLn("        Increase severity if age < %1 years and severity < %2",
                    ghm_root.young_age_treshold, ghm_root.young_severity_limit + 1);
        }
        if (ghm_root.old_age_treshold) {
            PrintLn("        Increase severity if age >= %1 years and severity < %2",
                    ghm_root.old_age_treshold, ghm_root.old_severity_limit + 1);
        }

        if (ghm_root.childbirth_severity_list) {
            PrintLn("        Childbirth severity list %1", ghm_root.childbirth_severity_list);
        }
    }
}

void DumpGhsTable(ArrayRef<const GhsInfo> ghs)
{
    GhmCode previous_ghm = {};
    for (const GhsInfo &ghs_info: ghs) {
        if (ghs_info.ghm != previous_ghm) {
            PrintLn("      GHM %1:", ghs_info.ghm);
            previous_ghm = ghs_info.ghm;
        }
        PrintLn("        GHS %1 (public) / GHS %2 (private)",
                ghs_info.ghs[0], ghs_info.ghs[1]);

        if (ghs_info.unit_authorization) {
            PrintLn("          Requires unit authorization %1", ghs_info.unit_authorization);
        }
        if (ghs_info.bed_authorization) {
            PrintLn("          Requires bed authorization %1", ghs_info.bed_authorization);
        }
        if (ghs_info.minimal_duration) {
            PrintLn("          Requires duration >= %1 days", ghs_info.minimal_duration);
        }
        if (ghs_info.minimal_age) {
            PrintLn("          Requires age >= %1 years", ghs_info.minimal_age);
        }
        if (ghs_info.main_diagnosis_mask) {
            PrintLn("          Main Diagnosis List D$%1.%2",
                    ghs_info.main_diagnosis_offset, ghs_info.main_diagnosis_mask);
        }
        if (ghs_info.diagnosis_mask) {
            PrintLn("          Diagnosis List D$%1.%2",
                    ghs_info.diagnosis_offset, ghs_info.diagnosis_mask);
        }
        if (ghs_info.proc_mask) {
            PrintLn("          Procedure List A$%1.%2",
                    ghs_info.proc_offset, ghs_info.proc_mask);
        }
    }
}

void DumpSeverityTable(ArrayRef<const ValueRangeCell<2>> cells)
{
    for (const ValueRangeCell<2> &cell: cells) {
        PrintLn("      %1-%2 and %3-%4 = %5",
                cell.limits[0].min, cell.limits[0].max, cell.limits[1].min, cell.limits[1].max,
                cell.value);
    }
}

void DumpAuthorizationTable(ArrayRef<const AuthorizationInfo> authorizations)
{
    for (const AuthorizationInfo &auth: authorizations) {
        PrintLn("      %1 [%2] => Function %3",
                auth.code, AuthorizationTypeNames[(int)auth.type], auth.function);
    }
}

void DumpSupplementPairTable(ArrayRef<const SrcPair> pairs)
{
    for (const SrcPair &pair: pairs) {
        PrintLn("      %1 -- %2", pair.diag_code, pair.proc_code);
    }
}

void DumpTableSet(const TableSet &table_set, bool detail)
{
    PrintLn("Headers:");
    for (const TableInfo &table: table_set.tables) {
        PrintLn("  Table '%1' build %2:", TableTypeNames[(int)table.type], table.build_date);
        PrintLn("    Raw Type: %1", table.raw_type);
        PrintLn("    Version: %1.%2", table.version[0], table.version[1]);
        PrintLn("    Validity: %1 to %2", table.limit_dates[0], table.limit_dates[1]);
        PrintLn("    Sections:");
        for (size_t i = 0; i < table.sections.len; i++) {
            PrintLn("      %1. %2 -- %3 bytes -- %4 elements (%5 bytes / element)",
                    i, FmtHex(table.sections[i].raw_offset), table.sections[i].raw_len,
                    table.sections[i].values_count, table.sections[i].value_len);
        }
        PrintLn();
    }

    if (detail) {
        PrintLn("Content:");
        for (const TableIndex &index: table_set.indexes) {
            PrintLn("  %1 to %2:", index.limit_dates[0], index.limit_dates[1]);
            // We don't really need to loop here, but we want the switch to get
            // warnings when we introduce new table types.
            for (size_t i = 0; i < CountOf(index.tables); i++) {
                if (!index.tables[i])
                    continue;

                switch ((TableType)i) {
                    case TableType::GhmDecisionTree: {
                        PrintLn("    GHM Decision Tree:");
                        DumpGhmDecisionTree(index.ghm_nodes);
                        PrintLn();
                    } break;
                    case TableType::DiagnosisTable: {
                        PrintLn("    Diagnoses:");
                        DumpDiagnosisTable(index.diagnoses, index.exclusions);
                        PrintLn();
                    } break;
                    case TableType::ProcedureTable: {
                        PrintLn("    Procedures:");
                        DumpProcedureTable(index.procedures);
                        PrintLn();
                    } break;
                    case TableType::GhmRootTable: {
                        PrintLn("    GHM Roots:");
                        DumpGhmRootTable(index.ghm_roots);
                        PrintLn();
                    } break;
                    case TableType::SeverityTable: {
                        PrintLn("    GNN Table:");
                        DumpSeverityTable(index.gnn_cells);
                        PrintLn();

                        for (size_t j = 0; j < CountOf(index.cma_cells); j++) {
                            PrintLn("    CMA Table %1:", j + 1);
                            DumpSeverityTable(index.cma_cells[j]);
                            PrintLn();
                        }
                    } break;

                    case TableType::GhsTable: {
                        PrintLn("    GHS Table:");
                        DumpGhsTable(index.ghs);
                    } break;
                    case TableType::AuthorizationTable: {
                        PrintLn("    Authorization Types:");
                        DumpAuthorizationTable(index.authorizations);
                    } break;
                    case TableType::SrcPairTable: {
                        for (size_t j = 0; j < CountOf(index.src_pairs); j++) {
                            PrintLn("    Supplement Pairs List %1:", j + 1);
                            DumpSupplementPairTable(index.src_pairs[j]);
                            PrintLn();
                        }
                    } break;

                    case TableType::UnknownTable:
                        break;
                }
            }
            PrintLn();
        }
    }
}

void DumpGhsPricings(ArrayRef<const GhsPricing> ghs_pricings)
{
    for (size_t i = 0; i < ghs_pricings.len;) {
        GhsCode ghs_code = ghs_pricings[i].code;

        PrintLn("GHS %1:", ghs_code);

        for (; i < ghs_pricings.len && ghs_pricings[i].code == ghs_code; i++) {
            const GhsPricing &pricing = ghs_pricings[i];

            PrintLn("  %2 to %3:",
                    pricing.code, pricing.limit_dates[0], pricing.limit_dates[1]);
            PrintLn("    Public: %1 [exh = %2, exb = %3]",
                    FmtDouble(pricing.sectors[0].price_cents / 100.0, 2),
                    FmtDouble(pricing.sectors[0].exh_cents / 100.0, 2),
                    FmtDouble(pricing.sectors[0].exb_cents / 100.0, 2));
            PrintLn("    Private: %1 [exh = %2, exb = %3]",
                    FmtDouble(pricing.sectors[1].price_cents / 100.0, 2),
                    FmtDouble(pricing.sectors[1].exh_cents / 100.0, 2),
                    FmtDouble(pricing.sectors[1].exb_cents / 100.0, 2));
        }
    }
}

void DumpPricingSet(const PricingSet &pricing_set)
{
    DumpGhsPricings(pricing_set.ghs_pricings);
}
