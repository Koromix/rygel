#include "kutil.hh"
#include "dump.hh"
#include "tables.hh"

static void DumpDecisionNode(const ArrayRef<const GhmDecisionNode> nodes,
                             size_t node_idx, int depth)
{
    for (;;) {
        const GhmDecisionNode &ghm_node = nodes[node_idx];

        switch (ghm_node.type) {
            case GhmDecisionNode::Type::Test: {
                PrintLn("      %1%2. %3(%4, %5) => %6 [%7]", FmtArg("  ").Repeat(depth), node_idx,
                        ghm_node.u.test.function, ghm_node.u.test.params[0],
                        ghm_node.u.test.params[1], ghm_node.u.test.children_idx,
                        ghm_node.u.test.children_count);

                if (ghm_node.u.test.function != 20) {
                    for (size_t i = 1; i < ghm_node.u.test.children_count; i++) {
                        DumpDecisionNode(nodes, ghm_node.u.test.children_idx + i, depth + 1);
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

void DumpGhmDecisionTree(ArrayRef<const GhmDecisionNode> ghm_nodes)
{
    if (ghm_nodes.len) {
        DumpDecisionNode(ghm_nodes, 0, 0);
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

void DumpGhsDecisionTree(ArrayRef<const GhsDecisionNode> ghs_nodes)
{
    // This code is simplistic and assumes that test failures always go back
    // to a GHS (or nothing at all), which is necessarily true in ghsinfo.tab
    // even though our representation can do more.
    size_t test_until = SIZE_MAX;
    int test_depth = 0;
    for (size_t i = 0; i < ghs_nodes.len; i++) {
        const GhsDecisionNode &node = ghs_nodes[i];

        if (i == test_until) {
            test_until = SIZE_MAX;
            test_depth = 0;
        }

        switch (node.type) {
            case GhsDecisionNode::Type::Ghm: {
                PrintLn("      %1. GHM %2 [next %3]",
                        i, node.u.ghm.code, node.u.ghm.next_ghm_idx);
            } break;

            case GhsDecisionNode::Type::Test: {
                test_until = node.u.test.fail_goto_idx;
                test_depth++;

                PrintLn("      %1%2. Test %3(%4, %5) => %6",
                        FmtArg("  ").Repeat(test_depth), i,
                        node.u.test.function, node.u.test.params[0], node.u.test.params[1],
                        node.u.test.fail_goto_idx);
            } break;

            case GhsDecisionNode::Type::Ghs: {
                PrintLn("        %1%2. GHS %3 [duration = %4 to %5 days]",
                        FmtArg("  ").Repeat(test_depth), i,
                        node.u.ghs[0].code, node.u.ghs[0].low_duration_treshold,
                        node.u.ghs[0].high_duration_treshold);
            } break;
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

void DumpSupplementPairTable(ArrayRef<const DiagnosisProcedurePair> pairs)
{
    for (const DiagnosisProcedurePair &pair: pairs) {
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

                    case TableType::GhsDecisionTree: {
                        PrintLn("    GHS Decision Tree:");
                        DumpGhsDecisionTree(index.ghs_nodes);
                    } break;
                    case TableType::AuthorizationTable: {
                        PrintLn("    Authorization Types:");
                        DumpAuthorizationTable(index.authorizations);
                    } break;
                    case TableType::SupplementPairTable: {
                        for (size_t j = 0; j < CountOf(index.supplement_pairs); j++) {
                            PrintLn("    Supplement Pairs List %1:", j + 1);
                            DumpSupplementPairTable(index.supplement_pairs[j]);
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
