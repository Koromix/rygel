#include "kutil.hh"
#include "fg_data.hh"

static void DumpDecisionNode(const ArrayRef<const GhmDecisionNode> nodes,
                             size_t node_idx, int depth)
{
    for (;;) {
        const GhmDecisionNode &node = nodes[node_idx];

        switch (node.type) {
            case GhmDecisionNode::Type::Test: {
                PrintLn("      %1%2. %3(%4, %5) => %6 [%7]", FmtArg("  ").Repeat(depth), node_idx,
                        node.u.test.function, node.u.test.params[0], node.u.test.params[1],
                        node.u.test.children_idx, node.u.test.children_count);

                if (node.u.test.function != 20) {
                    for (size_t i = 1; i < node.u.test.children_count; i++) {
                        DumpDecisionNode(nodes, node.u.test.children_idx + i, depth + 1);
                    }

                    node_idx = node.u.test.children_idx;
                } else {
                    return;
                }
            } break;

            case GhmDecisionNode::Type::Ghm: {
                PrintLn("      %1%2. %3 (err = %4)", FmtArg("  ").Repeat(depth), node_idx,
                        node.u.ghm.code, node.u.ghm.error);
                return;
            } break;
        }
    }
}

static bool DumpDecisionTree(const uint8_t *file_data, const char *filename,
                             const TableInfo &table_info)
{
    DynamicArray<GhmDecisionNode> ghm_nodes;
    if (!ParseGhmDecisionTree(file_data, filename, table_info, &ghm_nodes))
        return false;

    PrintLn("    Decision Tree:");
    DumpDecisionNode(ghm_nodes, 0, 0);

    return true;
}

static bool DumpDiagnosticTable(const uint8_t *file_data, const char *filename,
                                const TableInfo &table_info)
{
    DynamicArray<DiagnosticInfo> diagnostics;
    if (!ParseDiagnosticTable(file_data, filename, table_info, &diagnostics))
        return false;

    PrintLn("    Diagnostics:");
    for (const DiagnosticInfo &diag: diagnostics) {
        PrintLn("      %1:", diag.code);

        Print("        Male:");
        for (size_t i = 0; i < CountOf(diag.sex[0].values); i++) {
            Print(" %1", FmtBin(diag.sex[0].values[i]));
        }
        PrintLn();
        Print("        Female:");
        for (size_t i = 0; i < CountOf(diag.sex[1].values); i++) {
            Print(" %1", FmtBin(diag.sex[1].values[i]));
        }
        PrintLn();

        PrintLn("        CMD Male: %1 - Female: %2", diag.sex[0].info.cmd,
                                                     diag.sex[1].info.cmd);
        PrintLn("        Warnings: %1", FmtBin(diag.warnings));
    }

    return true;
}

static bool DumpProcedureTable(const uint8_t *file_data, const char *filename,
                               const TableInfo &table_info)
{
    DynamicArray<ProcedureInfo> procedures;
    if (!ParseProcedureTable(file_data, filename, table_info, &procedures))
        return false;

    PrintLn("    Procedures:");
    for (const ProcedureInfo &proc: procedures) {
        Print("      %1/%2 =", proc.code, proc.phase);
        for (size_t i = 0; i < CountOf(proc.values); i++) {
            Print(" %1", FmtBin(proc.values[i]));
        }
        PrintLn();

        PrintLn("        Validity: %1 to %2", proc.limit_dates[0], proc.limit_dates[1]);
    }
    PrintLn();

    return true;
}

static bool DumpGhmRootTable(const uint8_t *file_data, const char *filename,
                             const TableInfo &table_info)
{
    DynamicArray<GhmRootInfo> ghm_roots;
    if (!ParseGhmRootTable(file_data, filename, table_info, &ghm_roots))
        return false;

    PrintLn("    GHM roots:");
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
            PrintLn("        Can be short duration (T) if < %1 days", ghm_root.short_duration_treshold);
        }

        if (ghm_root.young_age_treshold) {
            PrintLn("        Increase severity if age < %1 years and severity < %2",
                    ghm_root.young_age_treshold, ghm_root.young_severity_limit);
        }
        if (ghm_root.old_age_treshold) {
            PrintLn("        Increase severity if age >= %1 years and severity < %2",
                    ghm_root.old_age_treshold, ghm_root.old_severity_limit);
        }

        if (ghm_root.childbirth_severity_list) {
            PrintLn("        Childbirth severity list %1", ghm_root.childbirth_severity_list);
        }
    }
    PrintLn();

    return true;
}

static bool DumpGhsTable(const uint8_t *file_data, const char *filename,
                         const TableInfo &table_info)
{
    DynamicArray<GhsDecisionNode> ghs_nodes;
    if (!ParseGhsDecisionTable(file_data, filename, table_info, &ghs_nodes))
        return false;

    PrintLn("    GHS nodes:");
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

    return true;
}

static bool DumpChildbirthTable(const uint8_t *file_data, const char *filename,
                                const TableInfo &table_info)
{
    // FIXME: Make a dedicated childbirth parse function
    if (table_info.sections.len != 4) {
        // TODO: Error message
        return false;
    }

    DynamicArray<ValueRangeCell<2>> gnn_table;
    DynamicArray<ValueRangeCell<2>> cma_tables[3];
    if (!ParseValueRangeTable(file_data, filename, table_info.sections[0], &gnn_table))
        return false;
    for (size_t i = 0; i < CountOf(cma_tables); i++) {
        if (!ParseValueRangeTable(file_data, filename, table_info.sections[i + 1], &cma_tables[i]))
            return false;
    }

    PrintLn("    GNN Table:");
    for (const ValueRangeCell<2> &cell: gnn_table) {
        PrintLn("      Weight %1-%2 and GA %3-%4 = %5",
                cell.limits[0].min, cell.limits[0].max, cell.limits[1].min, cell.limits[1].max,
                cell.value);
    }
    PrintLn();

    for (size_t i = 0; i < CountOf(cma_tables); i++) {
        PrintLn("    CMA Table %1:", i);
        for (const ValueRangeCell<2> &cell: cma_tables[i]) {
            if (cell.limits[1].min + 1 == cell.limits[1].max) {
                PrintLn("      GA %1-%2 and severity %3 => %4",
                        cell.limits[0].min, cell.limits[0].max,
                        (char)(cell.limits[1].min + 65), (char)(cell.value + 65));
            } else {
                PrintLn("      GA %1-%2 and severity %3-%4 => %5",
                        cell.limits[0].min, cell.limits[0].max,
                        (char)(cell.limits[1].min + 65), (char)(cell.limits[1].max + 65),
                        (char)(cell.value + 65));
            }
        }
        PrintLn();
    }

    return true;
}

static bool DumpTable(const char *filename, bool detail = true)
{
    uint8_t *file_data;
    uint64_t file_len;
    if (!ReadFile(nullptr, filename, Megabytes(20), &file_data, &file_len))
        return false;
    DEFER { Allocator::Release(nullptr, file_data, file_len); };

    DynamicArray<TableInfo> tables;
    if (!ParseTableHeaders(file_data, file_len, filename, &tables))
        return false;

    PrintLn("%1", filename);
    for (const TableInfo &table_info: tables) {
        PrintLn("  Table '%1' build %2:", TableTypeNames[(int)table_info.type], table_info.build_date);
        PrintLn("    Header:");
        PrintLn("      Version: %1.%2", table_info.version[0], table_info.version[1]);
        PrintLn("      Validity: %1 to %2", table_info.limit_dates[0], table_info.limit_dates[1]);
        PrintLn("      Sections:");
        for (size_t i = 0; i < table_info.sections.len; i++) {
            PrintLn("        %1. %2 -- %3 bytes -- %4 elements (%5 bytes / element)",
                    i, FmtHex(table_info.sections[i].raw_offset), table_info.sections[i].raw_len,
                    table_info.sections[i].values_count, table_info.sections[i].value_len);
        }
        PrintLn();

        if (detail) {
            switch (table_info.type) {
                case TableType::GhmDecisionTree: {
                    DumpDecisionTree(file_data, filename, table_info);
                } break;
                case TableType::DiagnosticInfo: {
                    DumpDiagnosticTable(file_data, filename, table_info);
                } break;
                case TableType::ProcedureInfo: {
                    DumpProcedureTable(file_data, filename, table_info);
                } break;
                case TableType::GhmRootInfo: {
                    DumpGhmRootTable(file_data, filename, table_info);
                } break;
                case TableType::GhsDecisionTree: {
                    DumpGhsTable(file_data, filename, table_info);
                } break;
                case TableType::ChildbirthInfo: {
                    DumpChildbirthTable(file_data, filename, table_info);
                } break;
            }
            PrintLn();
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    const char *cmd;
    if (argc < 2) {
generic_usage:
        Print(stderr,
R"(Usage: moya command [options]

Commands:
    fg_dump        Dump available classifier data tables
    fg_pricing     Print GHS pricing tables
    fg_run         Run classifier on patient data
)");
        return 1;
    }
    cmd = argv[1];

#define COMMAND(Cmd) \
        if (!(strcmp(cmd, STRINGIFY(Cmd))))

    COMMAND(fg_dump) {
        if (argc < 3) {
            PrintLn(stderr, "Usage: moya fg_dump filename");
            return 1;
        }

        bool success = true;
        for (int i = 2; i < argc; i++) {
            success &= DumpTable(argv[i]);
        }
        return !success;
    }
    COMMAND(fg_pricing) {
        PrintLn(stderr, "Not implemented");
        return 1;
    }
    COMMAND(fg_run) {
        PrintLn(stderr, "Not implemented");
        return 1;
    }

#undef COMMAND

    PrintLn(stderr, "Unknown command '%1'", cmd);
    goto generic_usage;
}
