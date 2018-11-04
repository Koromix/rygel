// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "mco_dump.hh"

void mco_DumpGhmDecisionTree(Span<const mco_GhmDecisionNode> ghm_nodes,
                             Size node_idx, int depth, StreamWriter *out_st)
{
    if (!ghm_nodes.len)
        return;

    for (Size i = 0;; i++) {
        if (UNLIKELY(i >= ghm_nodes.len)) {
            LogError("Empty GHM tree or infinite loop (%2)", ghm_nodes.len);
            return;
        }

        DebugAssert(node_idx < ghm_nodes.len);
        const mco_GhmDecisionNode &ghm_node = ghm_nodes[node_idx];

        switch (ghm_node.type) {
            case mco_GhmDecisionNode::Type::Test: {
                PrintLn(out_st, "      %1%2. %3(%4, %5) => %6 [%7]", FmtArg("  ").Repeat(depth), node_idx,
                        ghm_node.u.test.function, ghm_node.u.test.params[0],
                        ghm_node.u.test.params[1], ghm_node.u.test.children_idx,
                        ghm_node.u.test.children_count);

                if (ghm_node.u.test.function != 20) {
                    for (Size j = 1; j < ghm_node.u.test.children_count; j++) {
                        mco_DumpGhmDecisionTree(ghm_nodes, ghm_node.u.test.children_idx + j,
                                                depth + 1, out_st);
                    }

                    node_idx = ghm_node.u.test.children_idx;
                } else {
                    return;
                }
            } break;

            case mco_GhmDecisionNode::Type::Ghm: {
                if (ghm_node.u.ghm.error) {
                    PrintLn(out_st, "      %1%2. %3 (err = %4)", FmtArg("  ").Repeat(depth), node_idx,
                            ghm_node.u.ghm.ghm, ghm_node.u.ghm.error);
                } else {
                    PrintLn(out_st, "      %1%2. %3", FmtArg("  ").Repeat(depth), node_idx,
                            ghm_node.u.ghm.ghm);
                }
                return;
            } break;
        }
    }
}

void mco_DumpDiagnosisTable(Span<const mco_DiagnosisInfo> diagnoses,
                            Span<const mco_ExclusionInfo> exclusions,
                            StreamWriter *out_st)
{
    for (const mco_DiagnosisInfo &diag: diagnoses) {
        const auto DumpMask = [&](int8_t sex) {
            for (Size i = 0; i < ARRAY_SIZE(diag.Attributes(sex).raw); i++) {
                Print(out_st, " 0b%1", FmtBin(diag.Attributes(sex).raw[i]).Pad0(-8));
            }
            PrintLn(out_st);
        };

        PrintLn(out_st, "      %1:", diag.diag);
        if (diag.flags & (int)mco_DiagnosisInfo::Flag::SexDifference) {
            PrintLn(out_st, "        Male:");
            PrintLn(out_st, "          Category: %1", diag.Attributes(1).cmd);
            PrintLn(out_st, "          Severity: %1", diag.Attributes(1).severity + 1);
            Print(out_st, "          Mask:");
            DumpMask(1);

            PrintLn(out_st, "        Female:");
            PrintLn(out_st, "          Category: %1", diag.Attributes(2).cmd);
            PrintLn(out_st, "          Severity: %1", diag.Attributes(2).severity + 1);
            Print(out_st, "          Mask:");
            DumpMask(2);
        } else {
            PrintLn(out_st, "        Category: %1", diag.Attributes(1).cmd);
            PrintLn(out_st, "        Severity: %1", diag.Attributes(1).severity + 1);
            Print(out_st, "        Mask:");
            DumpMask(1);
        }
        PrintLn(out_st, "        Warnings: 0b%1", FmtBin(diag.warnings).Pad0(-8 * SIZE(diag.warnings)));

        if (exclusions.len) {
            Assert(diag.exclusion_set_idx <= exclusions.len);
            const mco_ExclusionInfo *excl = &exclusions[diag.exclusion_set_idx];

            Print(out_st, "        Exclusions (list %1):", diag.exclusion_set_idx);
            for (const mco_DiagnosisInfo &excl_diag: diagnoses) {
                if (excl->raw[excl_diag.cma_exclusion_mask.offset] &
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
        PrintLn(out_st, "      %1/%2:", proc.proc, proc.phase);
        PrintLn(out_st, "        Validity: %1 to %2", proc.limit_dates[0], proc.limit_dates[1]);
        PrintLn(out_st, "        Activities: %1", proc.ActivitiesToDec());
        PrintLn(out_st, "        Extensions: %1", proc.ExtensionsToDec());
        Print(out_st, "        Mask: ");
        for (Size i = 0; i < ARRAY_SIZE(proc.bytes); i++) {
            Print(out_st, " 0b%1", FmtBin(proc.bytes[i]).Pad0(-8));
        }
        PrintLn(out_st);
    }
}

void mco_DumpGhmRootTable(Span<const mco_GhmRootInfo> ghm_roots, StreamWriter *out_st)
{
    for (const mco_GhmRootInfo &ghm_root: ghm_roots) {
        PrintLn(out_st, "      GHM root %1:", ghm_root.ghm_root);

        if (ghm_root.confirm_duration_treshold) {
            PrintLn(out_st, "        Confirm if < %1 days (except for deaths and MCO transfers)",
                    ghm_root.confirm_duration_treshold);
        }

        if (ghm_root.allow_ambulatory) {
            PrintLn(out_st, "        Can be ambulatory (J)");
        }
        if (ghm_root.short_duration_treshold) {
            PrintLn(out_st, "        Can be short duration (T) if < %1 days",
                    ghm_root.short_duration_treshold);
        }

        if (ghm_root.young_age_treshold) {
            PrintLn(out_st, "        Increase severity if age < %1 years and severity < %2",
                    ghm_root.young_age_treshold, ghm_root.young_severity_limit + 1);
        }
        if (ghm_root.old_age_treshold) {
            PrintLn(out_st, "        Increase severity if age >= %1 years and severity < %2",
                    ghm_root.old_age_treshold, ghm_root.old_severity_limit + 1);
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
                ghm_to_ghs_info.Ghs(Sector::Public), ghm_to_ghs_info.Ghs(Sector::Private));

        if (ghm_to_ghs_info.unit_authorization) {
            PrintLn(out_st, "          Requires unit authorization %1", ghm_to_ghs_info.unit_authorization);
        }
        if (ghm_to_ghs_info.bed_authorization) {
            PrintLn(out_st, "          Requires bed authorization %1", ghm_to_ghs_info.bed_authorization);
        }
        if (ghm_to_ghs_info.minimal_duration) {
            PrintLn(out_st, "          Requires duration >= %1 days", ghm_to_ghs_info.minimal_duration);
        }
        if (ghm_to_ghs_info.minimal_age) {
            PrintLn(out_st, "          Requires age >= %1 years", ghm_to_ghs_info.minimal_age);
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
        for (const ListMask &mask: ghm_to_ghs_info.procedure_masks) {
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
        for (Size i = 0; i < ARRAY_SIZE(index.tables); i++) {
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

                    for (Size j = 0; j < ARRAY_SIZE(index.cma_cells); j++) {
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
                    for (Size j = 0; j < ARRAY_SIZE(index.src_pairs); j++) {
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
