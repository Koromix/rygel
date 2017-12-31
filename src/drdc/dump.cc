// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"
#include "dump.hh"

void DumpGhmDecisionTree(Span<const GhmDecisionNode> ghm_nodes,
                         Size node_idx, int depth)
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
                    for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
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
                            ghm_node.u.ghm.ghm, ghm_node.u.ghm.error);
                } else {
                    PrintLn("      %1%2. %3", FmtArg("  ").Repeat(depth), node_idx,
                            ghm_node.u.ghm.ghm);
                }
                return;
            } break;
        }
    }
}

void DumpDiagnosisTable(Span<const DiagnosisInfo> diagnoses,
                        Span<const ExclusionInfo> exclusions)
{
    for (const DiagnosisInfo &diag: diagnoses) {
        const auto DumpMask = [&](Sex sex) {
            for (Size i = 0; i < ARRAY_SIZE(diag.Attributes(sex).raw); i++) {
                Print(" %1", FmtBin(diag.Attributes(sex).raw[i]));
            }
            PrintLn();
        };

        PrintLn("      %1:", diag.diag);
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
                        if (excl->raw[excl_diag.cma_exclusion_mask.offset] &
                                excl_diag.cma_exclusion_mask.value) {
                            Print(" %1", excl_diag.diag);
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

void DumpProcedureTable(Span<const ProcedureInfo> procedures)
{
    for (const ProcedureInfo &proc: procedures) {
        Print("      %1/%2 =", proc.proc, proc.phase);
        for (Size i = 0; i < ARRAY_SIZE(proc.bytes); i++) {
            Print(" %1", FmtBin(proc.bytes[i]));
        }
        PrintLn();

        PrintLn("        Validity: %1 to %2", proc.limit_dates[0], proc.limit_dates[1]);
    }
}

void DumpGhmRootTable(Span<const GhmRootInfo> ghm_roots)
{
    for (const GhmRootInfo &ghm_root: ghm_roots) {
        PrintLn("      %1:", ghm_root.ghm_root);

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

void DumpGhsAccessTable(Span<const GhsAccessInfo> ghs)
{
    GhmCode previous_ghm = {};
    for (const GhsAccessInfo &ghs_access_info: ghs) {
        if (ghs_access_info.ghm != previous_ghm) {
            PrintLn("      GHM %1:", ghs_access_info.ghm);
            previous_ghm = ghs_access_info.ghm;
        }
        PrintLn("        GHS %1 (public) / GHS %2 (private)",
                ghs_access_info.ghs[0], ghs_access_info.ghs[1]);

        if (ghs_access_info.unit_authorization) {
            PrintLn("          Requires unit authorization %1", ghs_access_info.unit_authorization);
        }
        if (ghs_access_info.bed_authorization) {
            PrintLn("          Requires bed authorization %1", ghs_access_info.bed_authorization);
        }
        if (ghs_access_info.minimal_duration) {
            PrintLn("          Requires duration >= %1 days", ghs_access_info.minimal_duration);
        }
        if (ghs_access_info.minimal_age) {
            PrintLn("          Requires age >= %1 years", ghs_access_info.minimal_age);
        }
        if (ghs_access_info.main_diagnosis_mask.value) {
            PrintLn("          Main Diagnosis List D$%1.%2",
                    ghs_access_info.main_diagnosis_mask.offset,
                    ghs_access_info.main_diagnosis_mask.value);
        }
        if (ghs_access_info.diagnosis_mask.value) {
            PrintLn("          Diagnosis List D$%1.%2",
                    ghs_access_info.diagnosis_mask.offset, ghs_access_info.diagnosis_mask.value);
        }
        for (const ListMask &mask: ghs_access_info.procedure_masks) {
            PrintLn("          Procedure List A$%1.%2", mask.offset, mask.value);
        }
    }
}

void DumpGhsPriceTable(Span<const GhsPriceInfo> ghs_prices)
{
    for (Size i = 0; i < ghs_prices.len;) {
        GhsCode ghs = ghs_prices[i].ghs;

        PrintLn("      GHS %1:", ghs);
        for (; i < ghs_prices.len && ghs_prices[i].ghs == ghs; i++) {
            const GhsPriceInfo &price_info = ghs_prices[i];

            PrintLn("        Public: %1 [exh = %2, exb = %3]",
                    FmtDouble(price_info.sectors[0].price_cents / 100.0, 2),
                    FmtDouble(price_info.sectors[0].exh_cents / 100.0, 2),
                    FmtDouble(price_info.sectors[0].exb_cents / 100.0, 2));
            PrintLn("        Private: %1 [exh = %2, exb = %3]",
                    FmtDouble(price_info.sectors[1].price_cents / 100.0, 2),
                    FmtDouble(price_info.sectors[1].exh_cents / 100.0, 2),
                    FmtDouble(price_info.sectors[1].exb_cents / 100.0, 2));
        }
        PrintLn();
    }
}

void DumpSeverityTable(Span<const ValueRangeCell<2>> cells)
{
    for (const ValueRangeCell<2> &cell: cells) {
        PrintLn("      %1-%2 and %3-%4 = %5",
                cell.limits[0].min, cell.limits[0].max, cell.limits[1].min, cell.limits[1].max,
                cell.value);
    }
}

void DumpAuthorizationTable(Span<const AuthorizationInfo> authorizations)
{
    for (const AuthorizationInfo &auth: authorizations) {
        PrintLn("      %1 [%2] => Function %3",
                auth.code, AuthorizationScopeNames[(int)auth.scope], auth.function);
    }
}

void DumpSupplementPairTable(Span<const SrcPair> pairs)
{
    for (const SrcPair &pair: pairs) {
        PrintLn("      %1 -- %2", pair.diag, pair.proc);
    }
}

void DumpTableSetHeaders(const TableSet &table_set)
{
    PrintLn("Headers:");
    for (const TableInfo &table: table_set.tables) {
        PrintLn("  Table '%1' build %2:", TableTypeNames[(int)table.type], table.build_date);
        PrintLn("    Source: %1", table.filename);
        PrintLn("    Raw Type: %1", table.raw_type);
        PrintLn("    Version: %1.%2", table.version[0], table.version[1]);
        PrintLn("    Validity: %1 to %2", table.limit_dates[0], table.limit_dates[1]);
        PrintLn("    Sections:");
        for (Size i = 0; i < table.sections.len; i++) {
            PrintLn("      %1. %2 -- %3 bytes -- %4 elements (%5 bytes / element)",
                    i, FmtHex((uint64_t)table.sections[i].raw_offset), table.sections[i].raw_len,
                    table.sections[i].values_count, table.sections[i].value_len);
        }
        PrintLn();
    }

    PrintLn("Index:");
    for (const TableIndex &index: table_set.indexes) {
        PrintLn("  %1 to %2:", index.limit_dates[0], index.limit_dates[1]);
        for (const TableInfo *table: index.tables) {
            if (!table)
                continue;

            PrintLn("    %1: %2.%3 [%4 -- %5, build: %6]",
                    TableTypeNames[(int)table->type], table->version[0], table->version[1],
                    table->limit_dates[0], table->limit_dates[1], table->build_date);
        }
        PrintLn();
    }
}

void DumpTableSetContent(const TableSet &table_set)
{
    PrintLn("Content:");
    for (const TableIndex &index: table_set.indexes) {
        PrintLn("  %1 to %2:", index.limit_dates[0], index.limit_dates[1]);
        // We don't really need to loop here, but we want the switch to get
        // warnings when we introduce new table types.
        for (Size i = 0; i < ARRAY_SIZE(index.tables); i++) {
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

                    for (Size j = 0; j < ARRAY_SIZE(index.cma_cells); j++) {
                        PrintLn("    CMA Table %1:", j + 1);
                        DumpSeverityTable(index.cma_cells[j]);
                        PrintLn();
                    }
                } break;

                case TableType::GhsAccessTable: {
                    PrintLn("    GHS Access Table:");
                    DumpGhsAccessTable(index.ghs);
                } break;
                case TableType::PriceTable: {
                    PrintLn("    Price Table:");
                    DumpGhsPriceTable(index.ghs_prices);
                } break;
                case TableType::AuthorizationTable: {
                    PrintLn("    Authorization Types:");
                    DumpAuthorizationTable(index.authorizations);
                } break;
                case TableType::SrcPairTable: {
                    for (Size j = 0; j < ARRAY_SIZE(index.src_pairs); j++) {
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

void DumpGhmRootCatalog(Span<const GhmRootDesc> ghm_roots)
{
    for (const GhmRootDesc &desc: ghm_roots) {
        PrintLn("  %1:", desc.ghm_root);
        PrintLn("    Description: %1", desc.ghm_root_desc);
        PrintLn("    DA: %1 -- %2", desc.da, desc.da_desc);
        PrintLn("    GA: %1 -- %2", desc.ga, desc.ga_desc);
    }
}

void DumpCatalogSet(const CatalogSet &catalog_set)
{
    PrintLn("GHM Roots:");
    DumpGhmRootCatalog(catalog_set.ghm_roots);
}
