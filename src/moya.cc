#include "kutil.hh"
#include "fg_data.hh"

static void PrintUsage(FILE *fp = stdout)
{
    PrintLn(fp, "Usage: moya command [options]");
}
static void PrintFgDumpUsage(FILE *fp = stdout)
{
    PrintLn(fp, "Usage: moya fg_dump filename");
}

static void DumpDecisionNode(const ArrayRef<const DecisionNode> nodes,
                             size_t node_idx, int depth)
{
    for (;;) {
        const DecisionNode &node = nodes[node_idx];

        switch (node.type) {
            case DecisionNode::Type::Test: {
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

            case DecisionNode::Type::Leaf: {
                PrintLn("      %1%2. %3 (err = %4)", FmtArg("  ").Repeat(depth), node_idx,
                        node.u.leaf.ghm, node.u.leaf.error);
                return;
            } break;
        }
    }
}

static bool DumpDecisionTree(const uint8_t *file_data, const char *filename,
                             const TableInfo &table_info)
{
    DynamicArray<DecisionNode> nodes;
    if (!ParseDecisionTree(file_data, filename, table_info, &nodes))
        return false;

    PrintLn("    Decision Tree:");
    DumpDecisionNode(nodes, 0, 0);

    return true;
}

static bool DumpDiagnosticTable(const uint8_t *file_data, const char *filename,
                                const TableInfo &table_info)
{
    DynamicArray<DiagnosticInfo> diagnostics;
    if (!ParseDiagnosticTable(file_data, filename, table_info, &diagnostics))
        return false;

    PrintLn("    Diagnostics:");
    for (const DiagnosticInfo &dg_info: diagnostics) {
        PrintLn("      %1:", dg_info.code.str);

        Print("        Male:");
        for (size_t i = 0; i < CountOf(dg_info.sex[0].values); i++) {
            Print(" %1", FmtBin(dg_info.sex[0].values[i]));
        }
        PrintLn();
        Print("        Female:");
        for (size_t i = 0; i < CountOf(dg_info.sex[1].values); i++) {
            Print(" %1", FmtBin(dg_info.sex[1].values[i]));
        }
        PrintLn();

        PrintLn("        CMD Male: %1 - Female: %2", dg_info.sex[0].info.cmd,
                                                     dg_info.sex[1].info.cmd);
        PrintLn("        Warnings: %1", FmtBin(dg_info.warnings));
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
        PrintLn("      %1:", ghm_root.code.str);

        if (ghm_root.confirm_duration_treshold) {
            PrintLn("        Confirm if < %1 days (except for deaths and MCO transfers)",
                    ghm_root.confirm_duration_treshold);
        }

        if (ghm_root.allow_ambulatory) {
            PrintLn("        Ambulatory (J)");
        }
        if (ghm_root.short_duration_treshold) {
            PrintLn("        Short duration (T) if < %1 days", ghm_root.short_duration_treshold);
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
        PrintLn("    Version: %1.%2", table_info.version[0], table_info.version[1]);
        PrintLn("    Validity: %1 to %2", table_info.limit_dates[0], table_info.limit_dates[1]);
        PrintLn("    Sections:");
        for (size_t i = 0; i < table_info.sections.len; i++) {
            PrintLn("      %1. %2 -- %3 bytes -- %4 elements (%5 bytes / element)",
                    i, FmtHex(table_info.sections[i].raw_offset), table_info.sections[i].raw_len,
                    table_info.sections[i].values_count, table_info.sections[i].value_len);
        }
        PrintLn();

        if (detail) {
            switch (table_info.type) {
                case TableType::DecisionTree: {
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
                case TableType::ChildbirthInfo: {
                    DumpChildbirthTable(file_data, filename, table_info);
                } break;
            }
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    const char *cmd;
    if (argc < 2) {
        PrintUsage(stderr);
        return 1;
    }
    cmd = argv[1];

    if (!strcmp(cmd, "fg_dump")) {
        if (argc < 3) {
            PrintFgDumpUsage(stderr);
            return 1;
        }

        bool success = true;
        for (int i = 2; i < argc; i++) {
            success &= DumpTable(argv[i]);
        }
        return !success;
    }

    fprintf(stderr, "Unknown command '%s'\n", cmd);
    PrintUsage(stderr);
    return 1;
}
