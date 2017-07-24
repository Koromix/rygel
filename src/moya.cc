#include "kutil.hh"
#include "fg_table.hh"
#include "fg_classifier.hh"

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
                             const TableInfo &table)
{
    DynamicArray<GhmDecisionNode> ghm_nodes;
    if (!ParseGhmDecisionTree(file_data, filename, table, &ghm_nodes))
        return false;

    PrintLn("    Decision Tree:");
    DumpDecisionNode(ghm_nodes, 0, 0);

    return true;
}

static bool DumpDiagnosisTable(const uint8_t *file_data, const char *filename,
                               const TableInfo &table)
{
    DynamicArray<DiagnosisInfo> diagnosess;
    if (!ParseDiagnosisTable(file_data, filename, table, &diagnosess))
        return false;

    PrintLn("    Diagnoses:");
    for (const DiagnosisInfo &diag: diagnosess) {
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
                               const TableInfo &table)
{
    DynamicArray<ProcedureInfo> procedures;
    if (!ParseProcedureTable(file_data, filename, table, &procedures))
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
                             const TableInfo &table)
{
    DynamicArray<GhmRootInfo> ghm_roots;
    if (!ParseGhmRootTable(file_data, filename, table, &ghm_roots))
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
                         const TableInfo &table)
{
    DynamicArray<GhsDecisionNode> ghs_nodes;
    if (!ParseGhsDecisionTree(file_data, filename, table, &ghs_nodes))
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

static bool DumpChildbirthTables(const uint8_t *file_data, const char *filename,
                                 const TableInfo &table)
{
    // FIXME: Make a dedicated childbirth parse function
    if (table.sections.len != 4) {
        // TODO: Error message
        return false;
    }

    DynamicArray<ValueRangeCell<2>> gnn_table;
    DynamicArray<ValueRangeCell<2>> cma_tables[3];
    if (!ParseValueRangeTable(file_data, filename, table.sections[0], &gnn_table))
        return false;
    for (size_t i = 0; i < CountOf(cma_tables); i++) {
        if (!ParseValueRangeTable(file_data, filename, table.sections[i + 1], &cma_tables[i]))
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

static bool DumpAuthorizationTable(const uint8_t *file_data, const char *filename,
                                   const TableInfo &table)
{
    DynamicArray<AuthorizationInfo> authorizations;
    if (!ParseAuthorizationTable(file_data, filename, table, &authorizations))
        return false;

    PrintLn("    Authorization Types:");
    for (const AuthorizationInfo &auth: authorizations) {
        PrintLn("      %1 [%2] => Function %3",
                auth.code, AuthorizationTypeNames[(int)auth.type], auth.function);
    }

    return true;
}

static bool DumpDiagnosisProcedureTables(const uint8_t *file_data, const char *filename,
                                         const TableInfo &table)
{
    DynamicArray<DiagnosisProcedurePair> diag_proc_pairs[2];
    for (size_t i = 0; i < CountOf(diag_proc_pairs); i++) {
        if (!ParseDiagnosisProcedureTable(file_data, filename, table.sections[i], &diag_proc_pairs[i]))
            return false;
    }

    for (size_t i = 0; i < CountOf(diag_proc_pairs); i++) {
        PrintLn("    List %1:", i + 1);
        for (const DiagnosisProcedurePair &pair: diag_proc_pairs[i]) {
            PrintLn("      %1 -- %2", pair.diag_code, pair.proc_code);
        }
        PrintLn();
    }

    return true;
}

static bool FgDump(const char *filename, bool detail = true)
{
    uint8_t *file_data;
    size_t file_len;
    if (!ReadFile(nullptr, filename, Megabytes(20), &file_data, &file_len))
        return false;
    DEFER { Allocator::Release(nullptr, file_data, file_len); };

    DynamicArray<TableInfo> tables;
    if (!ParseTableHeaders(file_data, file_len, filename, &tables))
        return false;

    PrintLn("%1", filename);
    for (const TableInfo &table: tables) {
        PrintLn("  Table '%1' build %2:", TableTypeNames[(int)table.type], table.build_date);
        PrintLn("    Header:");
        PrintLn("      Raw Type: %1", table.raw_type);
        PrintLn("      Version: %1.%2", table.version[0], table.version[1]);
        PrintLn("      Validity: %1 to %2", table.limit_dates[0], table.limit_dates[1]);
        PrintLn("      Sections:");
        for (size_t i = 0; i < table.sections.len; i++) {
            PrintLn("        %1. %2 -- %3 bytes -- %4 elements (%5 bytes / element)",
                    i, FmtHex(table.sections[i].raw_offset), table.sections[i].raw_len,
                    table.sections[i].values_count, table.sections[i].value_len);
        }
        PrintLn();

        if (detail) {
            switch (table.type) {
                case TableType::GhmDecisionTree: {
                    DumpDecisionTree(file_data, filename, table);
                } break;
                case TableType::DiagnosisTable: {
                    DumpDiagnosisTable(file_data, filename, table);
                } break;
                case TableType::ProcedureTable: {
                    DumpProcedureTable(file_data, filename, table);
                } break;
                case TableType::GhmRootTable: {
                    DumpGhmRootTable(file_data, filename, table);
                } break;
                case TableType::ChildbirthTable: {
                    DumpChildbirthTables(file_data, filename, table);
                } break;

                case TableType::GhsDecisionTree: {
                    DumpGhsTable(file_data, filename, table);
                } break;
                case TableType::AuthorizationTable: {
                    DumpAuthorizationTable(file_data, filename, table);
                } break;
                case TableType::DiagnosisProcedureTable: {
                    DumpDiagnosisProcedureTables(file_data, filename, table);
                } break;

                case TableType::UnknownTable:
                    break;
            }
            PrintLn();
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    static const char *const main_usage_str =
R"(Usage: moya command [options]

Commands:
    fg_dump                  Dump available classifier data tables
    fg_list                  Print diagnosis and procedure lists
    fg_pricing               Print GHS pricing tables
    fg_run                   Run classifier on patient data)";
    static const char *const fg_dump_usage_str =
R"(Usage: moya fg_dump [options] filename

Options:
    -h, --headers            Print only table headers)";
    static const char *const fg_tables_usage_str =
R"(Usage: moya fg_tables [options] filename

Options:
    -t, --table <filename>   Load table file or directory)";

    if (argc < 2) {
        PrintLn(stderr, "%1", main_usage_str);
        return 1;
    }

    if (argc < 2) {
        PrintLn(stderr, "%1", main_usage_str);
        return 1;
    }

#define COMMAND(Cmd) \
        if (!(strcmp(cmd, STRINGIFY(Cmd))))
#define REQUIRE_OPTION_VALUE(UsageStr) \
        do { \
            if (!opt_parser.ConsumeOptionValue()) { \
                PrintLn(stderr, "Option '%1' needs an argument", opt_parser.current_option); \
                PrintLn(stderr, "%1", (UsageStr)); \
                return 1; \
            } \
        } while (false)

    const char *cmd = argv[1];
    OptionParser opt_parser(argc - 1, argv + 1);

    COMMAND(fg_dump) {
        bool headers = false;
        {
            const char *opt;
            while ((opt = opt_parser.ConsumeOption())) {
                if (TestOption(opt, "-h", "--headers")) {
                    headers = true;
                } else if (TestOption(opt, "--help")) {
                    PrintLn(stdout, "%1", fg_dump_usage_str);
                    return 0;
                } else {
                    PrintLn(stderr, "Unknown option '%1'", opt);
                    PrintLn(stderr, "%1", fg_dump_usage_str);
                    return 1;
                }
            }
        }

        DynamicArray<const char *> filenames;
        opt_parser.ConsumeNonOptions(&filenames);
        if (!filenames.len) {
            PrintLn(stderr, "No filename provided");
            PrintLn(stderr, "%1", fg_dump_usage_str);
            return 1;
        }

        bool success = true;
        for (const char *filename: filenames) {
            success &= FgDump(filename, !headers);
        }
        return !success;
    }

    COMMAND(fg_tables) {
        Allocator temp_alloc;

        DynamicArray<const char *> filenames;
        bool verbose = false;
        {
            const char *opt;
            while ((opt = opt_parser.ConsumeOption())) {
                if (TestOption(opt, "-T", "--table-dir")) {
                    REQUIRE_OPTION_VALUE(fg_tables_usage_str);

                    // FIXME: Ugly copying?
                    // FIXME: Avoid use of Fmt, make full path directly
                    DynamicArray<FileInfo> files;
                    if (EnumerateDirectory(opt_parser.current_value, "*.tab", temp_alloc,
                                           &files, 1024) != EnumStatus::Done)
                        return 1;
                    for (const FileInfo &file: files) {
                        if (file.type == FileType::File) {
                            filenames.Append(Fmt(&temp_alloc, "%1/%2", opt_parser.current_value,
                                                 file.name));
                        }
                    }
                } else if (TestOption(opt, "-t", "--table-file")) {
                    REQUIRE_OPTION_VALUE(fg_tables_usage_str);
                    filenames.Append(opt_parser.current_value);
                } else if (TestOption(opt, "-v", "--verbose")) {
                    verbose = true;
                } else {
                    PrintLn(stderr, "Unknown option '%1'", opt);
                    PrintLn(stderr, "%1", fg_tables_usage_str);
                    return 1;
                }
            }
        }
        if (!filenames.len) {
            PrintLn(stderr, "No filename provided");
            PrintLn(stderr, "%1", fg_tables_usage_str);
            return 1;
        }

        ClassifierStore classifier_store = {};
        if (!LoadClassifierFiles(filenames, &classifier_store)) {
            if (!classifier_store.sets.len)
                return 1;
            LogError("Load incomplete, some information may be inaccurate");
        } else if (!classifier_store.sets.len) {
            return 0;
        }

        for (const ClassifierSet &set: classifier_store.sets) {
            PrintLn("%1 to %2:", set.limit_dates[0], set.limit_dates[1]);
            for (const TableInfo *table: set.tables) {
                if (!table)
                    continue;

                PrintLn("  %1: %2.%3",
                        TableTypeNames[(int)table->type], table->version[0], table->version[1]);
                if (verbose) {
                    PrintLn("    Validity: %1 to %2",
                            table->limit_dates[0], table->limit_dates[1]);
                    PrintLn("    Build: %1", table->build_date);
                }
            }
            PrintLn();
        }

        return 0;
    }

    COMMAND(fg_list) {
        PrintLn(stderr, "Not implemented");
        return 1;
    }

    COMMAND(fg_pricing) {
        PrintLn(stderr, "Not implemented");
        return 1;
    }

    COMMAND(fg_run) {
        PrintLn(stderr, "Not implemented");
        return 1;
    }

#undef REQUIRE_OPTION_VALUE
#undef COMMAND

    PrintLn(stderr, "Unknown command '%1'", cmd);
    PrintLn(stderr, "%1", main_usage_str);
    return 1;
}
