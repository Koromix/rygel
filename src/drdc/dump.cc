// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"
#include "dump.hh"

void mco_DumpGhmDecisionTree(Span<const mco_GhmDecisionNode> ghm_nodes,
                         Size node_idx, int depth)
{
    while (node_idx < ghm_nodes.len) {
        const mco_GhmDecisionNode &ghm_node = ghm_nodes[node_idx];

        switch (ghm_node.type) {
            case mco_GhmDecisionNode::Type::Test: {
                PrintLn("      %1%2. %3(%4, %5) => %6 [%7]", FmtArg("  ").Repeat(depth), node_idx,
                        ghm_node.u.test.function, ghm_node.u.test.params[0],
                        ghm_node.u.test.params[1], ghm_node.u.test.children_idx,
                        ghm_node.u.test.children_count);

                if (ghm_node.u.test.function != 20) {
                    for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                        mco_DumpGhmDecisionTree(ghm_nodes, ghm_node.u.test.children_idx + i, depth + 1);
                    }

                    node_idx = ghm_node.u.test.children_idx;
                } else {
                    return;
                }
            } break;

            case mco_GhmDecisionNode::Type::Ghm: {
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

void mco_DumpDiagnosisTable(Span<const mco_DiagnosisInfo> diagnoses,
                        Span<const mco_ExclusionInfo> exclusions)
{
    for (const mco_DiagnosisInfo &diag: diagnoses) {
        const auto DumpMask = [&](int8_t sex) {
            for (Size i = 0; i < ARRAY_SIZE(diag.Attributes(sex).raw); i++) {
                Print(" %1", FmtBin(diag.Attributes(sex).raw[i]));
            }
            PrintLn();
        };

        PrintLn("      %1:", diag.diag);
        if (diag.flags & (int)mco_DiagnosisInfo::Flag::SexDifference) {
            PrintLn("        Male:");
            PrintLn("          Category: %1", diag.Attributes(1).cmd);
            PrintLn("          Severity: %1", diag.Attributes(1).severity + 1);
            Print("          Mask:");
            DumpMask(1);

            PrintLn("        Female:");
            PrintLn("          Category: %1", diag.Attributes(2).cmd);
            PrintLn("          Severity: %1", diag.Attributes(2).severity + 1);
            Print("          Mask:");
            DumpMask(2);
        } else {
            PrintLn("        Category: %1", diag.Attributes(1).cmd);
            PrintLn("        Severity: %1", diag.Attributes(1).severity + 1);
            Print("        Mask:");
            DumpMask(1);
        }
        PrintLn("        Warnings: %1", FmtBin(diag.warnings));

        if (exclusions.len) {
            Assert(diag.exclusion_set_idx <= exclusions.len);
            const mco_ExclusionInfo *excl = &exclusions[diag.exclusion_set_idx];

            Print("        Exclusions (list %1):", diag.exclusion_set_idx);
            for (const mco_DiagnosisInfo &excl_diag: diagnoses) {
                if (excl->raw[excl_diag.cma_exclusion_mask.offset] &
                        excl_diag.cma_exclusion_mask.value) {
                    Print(" %1", excl_diag.diag);
                }
            }
            PrintLn();
        }
    }
}

void mco_DumpProcedureTable(Span<const mco_ProcedureInfo> procedures)
{
    for (const mco_ProcedureInfo &proc: procedures) {
        PrintLn("      %1/%2:", proc.proc, proc.phase);
        PrintLn("        Validity: %1 to %2", proc.limit_dates[0], proc.limit_dates[1]);
        {
            int activities_dec = 0;
            for (int activities_bin = proc.activities, i = 0; activities_bin; i++) {
                if (activities_bin & 0x1) {
                    activities_dec = (activities_dec * 10) + i;
                }
                activities_bin >>= 1;
            }
            PrintLn("        Activities: %1", activities_dec);
        }
        {
            LocalArray<FmtArg, 64> extensions;
            for (int extensions_bin = proc.extensions, i = 0; extensions_bin; i++) {
                if (extensions_bin & 0x1) {
                    extensions.Append(i);
                }
                extensions_bin >>= 1;
            }
            PrintLn("        Extensions: %1", extensions);
        }
        Print("        Mask: ");
        for (Size i = 0; i < ARRAY_SIZE(proc.bytes); i++) {
            Print(" %1", FmtBin(proc.bytes[i]));
        }
        PrintLn();
    }
}

void mco_DumpGhmRootTable(Span<const mco_GhmRootInfo> ghm_roots)
{
    for (const mco_GhmRootInfo &ghm_root: ghm_roots) {
        PrintLn("      GHM root %1:", ghm_root.ghm_root);

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

void mco_DumpGhmToGhsTable(Span<const mco_GhmToGhsInfo> ghs)
{
    mco_GhmCode previous_ghm = {};
    for (const mco_GhmToGhsInfo &ghm_to_ghs_info: ghs) {
        if (ghm_to_ghs_info.ghm != previous_ghm) {
            PrintLn("      GHM %1:", ghm_to_ghs_info.ghm);
            previous_ghm = ghm_to_ghs_info.ghm;
        }
        PrintLn("        GHS %1 (public) / GHS %2 (private)",
                ghm_to_ghs_info.Ghs(Sector::Public), ghm_to_ghs_info.Ghs(Sector::Private));

        if (ghm_to_ghs_info.unit_authorization) {
            PrintLn("          Requires unit authorization %1", ghm_to_ghs_info.unit_authorization);
        }
        if (ghm_to_ghs_info.bed_authorization) {
            PrintLn("          Requires bed authorization %1", ghm_to_ghs_info.bed_authorization);
        }
        if (ghm_to_ghs_info.minimal_duration) {
            PrintLn("          Requires duration >= %1 days", ghm_to_ghs_info.minimal_duration);
        }
        if (ghm_to_ghs_info.minimal_age) {
            PrintLn("          Requires age >= %1 years", ghm_to_ghs_info.minimal_age);
        }
        if (ghm_to_ghs_info.main_diagnosis_mask.value) {
            PrintLn("          Main Diagnosis List D$%1.%2",
                    ghm_to_ghs_info.main_diagnosis_mask.offset,
                    ghm_to_ghs_info.main_diagnosis_mask.value);
        }
        if (ghm_to_ghs_info.diagnosis_mask.value) {
            PrintLn("          Diagnosis List D$%1.%2",
                    ghm_to_ghs_info.diagnosis_mask.offset, ghm_to_ghs_info.diagnosis_mask.value);
        }
        for (const ListMask &mask: ghm_to_ghs_info.procedure_masks) {
            PrintLn("          Procedure List A$%1.%2", mask.offset, mask.value);
        }
    }
}

void mco_DumpGhsPriceTable(Span<const mco_GhsPriceInfo> ghs_prices)
{
    for (const mco_GhsPriceInfo &price_info: ghs_prices) {
        PrintLn("      GHS %1: %2 [exh = %3, exb = %4]", price_info.ghs,
                FmtDouble(price_info.price_cents / 100.0, 2),
                FmtDouble(price_info.exh_cents / 100.0, 2),
                FmtDouble(price_info.exb_cents / 100.0, 2));
    }
}

void mco_DumpSeverityTable(Span<const mco_ValueRangeCell<2>> cells)
{
    for (const mco_ValueRangeCell<2> &cell: cells) {
        PrintLn("      %1-%2 and %3-%4 = %5",
                cell.limits[0].min, cell.limits[0].max, cell.limits[1].min, cell.limits[1].max,
                cell.value);
    }
}

void mco_DumpAuthorizationTable(Span<const mco_AuthorizationInfo> authorizations)
{
    for (const mco_AuthorizationInfo &auth: authorizations) {
        PrintLn("      %1 [%2] => Function %3",
                auth.type.st.code, mco_AuthorizationScopeNames[(int)auth.type.st.scope], auth.function);
    }
}

void DumpSupplementPairTable(Span<const mco_SrcPair> pairs)
{
    for (const mco_SrcPair &pair: pairs) {
        PrintLn("      %1 -- %2", pair.diag, pair.proc);
    }
}

void mco_DumpTableSetHeaders(const mco_TableSet &table_set)
{
    PrintLn("Headers:");
    for (const mco_TableInfo &table: table_set.tables) {
        PrintLn("  Table '%1' build %2:", mco_TableTypeNames[(int)table.type], table.build_date);
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
    for (const mco_TableIndex &index: table_set.indexes) {
        PrintLn("  %1 to %2:", index.limit_dates[0], index.limit_dates[1]);
        for (const mco_TableInfo *table: index.tables) {
            if (!table)
                continue;

            PrintLn("    %1: %2.%3 [%4 -- %5, build: %6]",
                    mco_TableTypeNames[(int)table->type], table->version[0], table->version[1],
                    table->limit_dates[0], table->limit_dates[1], table->build_date);
        }
        PrintLn();
    }
}

void mco_DumpTableSetContent(const mco_TableSet &table_set)
{
    PrintLn("Content:");
    for (const mco_TableIndex &index: table_set.indexes) {
        PrintLn("  %1 to %2:", index.limit_dates[0], index.limit_dates[1]);
        // We don't really need to loop here, but we want the switch to get
        // warnings when we introduce new table types.
        for (Size i = 0; i < ARRAY_SIZE(index.tables); i++) {
            if (!index.tables[i])
                continue;

            switch ((mco_TableType)i) {
                case mco_TableType::GhmDecisionTree: {
                    PrintLn("    GHM Decision Tree:");
                    mco_DumpGhmDecisionTree(index.ghm_nodes);
                    PrintLn();
                } break;
                case mco_TableType::DiagnosisTable: {
                    PrintLn("    Diagnoses:");
                    mco_DumpDiagnosisTable(index.diagnoses, index.exclusions);
                    PrintLn();
                } break;
                case mco_TableType::ProcedureTable: {
                    PrintLn("    Procedures:");
                    mco_DumpProcedureTable(index.procedures);
                    PrintLn();
                } break;
                case mco_TableType::ProcedureExtensionTable: {} break;
                case mco_TableType::GhmRootTable: {
                    PrintLn("    GHM Roots:");
                    mco_DumpGhmRootTable(index.ghm_roots);
                    PrintLn();
                } break;
                case mco_TableType::SeverityTable: {
                    PrintLn("    GNN Table:");
                    mco_DumpSeverityTable(index.gnn_cells);
                    PrintLn();

                    for (Size j = 0; j < ARRAY_SIZE(index.cma_cells); j++) {
                        PrintLn("    CMA Table %1:", j + 1);
                        mco_DumpSeverityTable(index.cma_cells[j]);
                        PrintLn();
                    }
                } break;

                case mco_TableType::GhmToGhsTable: {
                    PrintLn("    GHM To GHS Table:");
                    mco_DumpGhmToGhsTable(index.ghs);
                } break;
                case mco_TableType::PriceTable: {
                    PrintLn("    Price Table:");
                    PrintLn("      Public:");
                    mco_DumpGhsPriceTable(index.ghs_prices[0]);
                    PrintLn("      Private:");
                    mco_DumpGhsPriceTable(index.ghs_prices[1]);
                } break;
                case mco_TableType::AuthorizationTable: {
                    PrintLn("    Authorization Types:");
                    mco_DumpAuthorizationTable(index.authorizations);
                } break;
                case mco_TableType::SrcPairTable: {
                    for (Size j = 0; j < ARRAY_SIZE(index.src_pairs); j++) {
                        PrintLn("    Supplement Pairs List %1:", j + 1);
                        DumpSupplementPairTable(index.src_pairs[j]);
                        PrintLn();
                    }
                } break;

                case mco_TableType::UnknownTable:
                    break;
            }
        }
        PrintLn();
    }
}
