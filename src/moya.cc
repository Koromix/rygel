#include "kutil.hh"
#include "classifier.hh"
#include "data_care.hh"
#include "data_fg.hh"

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

static void DumpGhmDecisionTree(ArrayRef<const GhmDecisionNode> ghm_nodes)
{
    if (ghm_nodes.len) {
        DumpDecisionNode(ghm_nodes, 0, 0);
    }
}

static void DumpDiagnosisTable(ArrayRef<const DiagnosisInfo> diagnoses)
{
    for (const DiagnosisInfo &diag: diagnoses) {
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
}

static void DumpProcedureTable(ArrayRef<const ProcedureInfo> procedures)
{
    for (const ProcedureInfo &proc: procedures) {
        Print("      %1/%2 =", proc.code, proc.phase);
        for (size_t i = 0; i < CountOf(proc.values); i++) {
            Print(" %1", FmtBin(proc.values[i]));
        }
        PrintLn();

        PrintLn("        Validity: %1 to %2", proc.limit_dates[0], proc.limit_dates[1]);
    }
}

static void DumpGhmRootTable(ArrayRef<const GhmRootInfo> ghm_roots)
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
}

static void DumpGhsDecisionTree(ArrayRef<const GhsDecisionNode> ghs_nodes)
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

static void DumpSeverityTable(ArrayRef<const ValueRangeCell<2>> cells)
{
    for (const ValueRangeCell<2> &cell: cells) {
        PrintLn("      %1-%2 and %3-%4 = %5",
                cell.limits[0].min, cell.limits[0].max, cell.limits[1].min, cell.limits[1].max,
                cell.value);
    }
}

static void DumpAuthorizationTable(ArrayRef<const AuthorizationInfo> authorizations)
{
    for (const AuthorizationInfo &auth: authorizations) {
        PrintLn("      %1 [%2] => Function %3",
                auth.code, AuthorizationTypeNames[(int)auth.type], auth.function);
    }
}

static void DumpSupplementPairTable(ArrayRef<const DiagnosisProcedurePair> pairs)
{
    for (const DiagnosisProcedurePair &pair: pairs) {
        PrintLn("      %1 -- %2", pair.diag_code, pair.proc_code);
    }
}

static void DumpClassifierStore(const ClassifierStore &store, bool detail = true)
{
    PrintLn("Headers:");
    for (const TableInfo &table: store.tables) {
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
        for (const ClassifierSet &set: store.sets) {
            // We don't really need to loop here, but we want the switch to get
            // warnings when we introduce new table types.
            for (size_t i = 0; i < CountOf(set.tables); i++) {
                if (!set.tables[i])
                    continue;

                switch ((TableType)i) {
                    case TableType::GhmDecisionTree: {
                        PrintLn("  GHM Decision Tree:");
                        DumpGhmDecisionTree(store.ghm_nodes.Take(set.ghm_nodes));
                        PrintLn();
                    } break;
                    case TableType::DiagnosisTable: {
                        PrintLn("  Diagnoses:");
                        DumpDiagnosisTable(store.diagnoses.Take(set.diagnoses));
                        PrintLn();
                    } break;
                    case TableType::ProcedureTable: {
                        PrintLn("  Procedures:");
                        DumpProcedureTable(store.procedures.Take(set.procedures));
                        PrintLn();
                    } break;
                    case TableType::GhmRootTable: {
                        PrintLn("  GHM Roots:");
                        DumpGhmRootTable(store.ghm_roots.Take(set.ghm_roots));
                        PrintLn();
                    } break;
                    case TableType::SeverityTable: {
                        PrintLn("  GNN Table:");
                        DumpSeverityTable(store.gnn_cells.Take(set.gnn_cells));
                        PrintLn();

                        for (size_t j = 0; j < CountOf(set.cma_cells); j++) {
                            PrintLn("  CMA Table %1:", j + 1);
                            DumpSeverityTable(store.cma_cells[j].Take(set.cma_cells[j]));
                            PrintLn();
                        }
                    } break;

                    case TableType::GhsDecisionTree: {
                        PrintLn("  GHS Decision Tree:");
                        DumpGhsDecisionTree(store.ghs_nodes.Take(set.ghs_nodes));
                    } break;
                    case TableType::AuthorizationTable: {
                        PrintLn("  Authorization Types:");
                        DumpAuthorizationTable(store.authorizations.Take(set.authorizations));
                    } break;
                    case TableType::SupplementPairTable: {
                        for (size_t j = 0; j < CountOf(set.supplement_pairs); j++) {
                            PrintLn("  Supplement Pairs List %1:", j + 1);
                            DumpSupplementPairTable(
                                store.supplement_pairs[j].Take(set.supplement_pairs[j]));
                            PrintLn();
                        }
                    } break;

                    case TableType::UnknownTable:
                        break;
                }
            }
        }
    }
}

static Date main_set_date = {};
static DynamicArray<const char *> main_table_filenames;
static ClassifierStore main_classifier_store = {};

static const ClassifierStore *GetMainClassifierStore()
{
    if (!main_classifier_store.sets.len) {
        if (!main_table_filenames.len) {
            LogError("No table provided");
            return nullptr;
        }
        LoadClassifierStore(main_table_filenames, &main_classifier_store);
        if (!main_classifier_store.sets.len)
            return nullptr;
    }

    return &main_classifier_store;
}

struct ListSpecifier {
    enum class Table {
        Diagnoses,
        Procedures
    };
    enum class Type {
        Mask
    };

    Table table;

    Type type;
    struct {
        struct {
            uint8_t offset;
            uint8_t mask;
        } mask;
    } u;

    bool Match(ArrayRef<const uint8_t> values) const;
};

bool ListSpecifier::Match(ArrayRef<const uint8_t> values) const
{
    switch (type) {
        case Type::Mask: {
            return u.mask.offset < values.len &&
                   values[u.mask.offset] & u.mask.mask;
        } break;
    }

    Assert(false);
}

// FIXME: Return invalid specifier instead
static bool ParseListSpecifier(const char *spec_str, ListSpecifier *out_spec)
{
    ListSpecifier spec = {};

    if (!spec_str[0] || !spec_str[1])
        goto error;

    switch (spec_str[0]) {
        case 'd': case 'D': { spec.table = ListSpecifier::Table::Diagnoses; } break;
        case 'a': case 'A': { spec.table = ListSpecifier::Table::Procedures; } break;

        default:
            goto error;
    }

    switch (spec_str[1]) {
        case '$': {
            spec.type = ListSpecifier::Type::Mask;
            if (sscanf(spec_str + 2, "%" SCNu8 ".%" SCNu8,
                       &spec.u.mask.offset, &spec.u.mask.mask) != 2)
                goto error;
        } break;

        default:
            goto error;
    }

    *out_spec = spec;
    return true;

error:
    LogError("Malformed list specifier '%1'", spec_str);
    return false;
}

int main(int argc, char **argv)
{
    static const char *const main_usage_str =
R"(Usage: moya command [options]

Commands:
    dump                  Dump available classifier data tables
    list                  Print diagnosis and procedure lists
    tables
    pricing               Print GHS pricing tables
    run                   Run classifier on patient data)";

    static const char *const dump_usage_str =
R"(Usage: moya fg_dump [options] filename

Options:
    -h, --headers            Print only table headers)";

    static const char *const list_usage_str =
R"(Usage: moya fg_tables [options] filename

Options:
    -t, --table <filename>   Load table file or directory)";

    static const char *const tables_usage_str =
R"(Usage: moya fg_tables [options] filename

Options:
    -t, --table <filename>   Load table file or directory)";

    static const char *const run_usage_str =
R"(Usage: moya fg_tables [options] filename

Options:
    -t, --table <filename>   Load table file or directory)";

    if (argc < 2) {
        PrintLn(stderr, "%1", main_usage_str);
        return 1;
    }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "help")) {
        if (argc > 2 && argv[2][0] != '-') {
            std::swap(argv[1], argv[2]);
            argv[2] = (char *)"--help";
        } else {
            PrintLn("%1", main_usage_str);
            return 1;
        }
    }

#define COMMAND(Cmd) \
        if (!(strcmp(cmd, STRINGIFY(Cmd))))
#define REQUIRE_OPTION_VALUE(UsageStr, ReturnValue) \
        do { \
            if (!opt_parser.ConsumeOptionValue()) { \
                PrintLn(stderr, "Option '%1' needs an argument", opt_parser.current_option); \
                PrintLn(stderr, "%1", (UsageStr)); \
                return (ReturnValue); \
            } \
        } while (false)

    Allocator temp_alloc;

    const char *cmd = argv[1];
    OptionParser opt_parser(argc - 1, argv + 1);

    const auto HandleMainOption = [&](const char *usage_str) {
        if (TestOption(opt_parser.current_option, "-T", "--table-dir")) {
            REQUIRE_OPTION_VALUE(main_usage_str, false);

            // FIXME: Ugly copying?
            // FIXME: Avoid use of Fmt, make full path directly
            DynamicArray<FileInfo> files;
            if (EnumerateDirectory(opt_parser.current_value, "*.tab", temp_alloc,
                                   &files, 1024) != EnumStatus::Done)
                return false;
            for (const FileInfo &file: files) {
                if (file.type == FileType::File) {
                    const char *filename =
                        Fmt(&temp_alloc, "%1%/%2", opt_parser.current_value, file.name);
                    main_table_filenames.Append(filename);
                }
            }

            return true;
        } else if (TestOption(opt_parser.current_option, "-t", "--table-file")) {
            REQUIRE_OPTION_VALUE(main_usage_str, false);
            main_table_filenames.Append(opt_parser.current_value);
            return true;
        } else if (TestOption(opt_parser.current_option, "-d", "--table-date")) {
            REQUIRE_OPTION_VALUE(main_usage_str, false);
            main_set_date = ParseDateString(opt_parser.current_value);
            return !!main_set_date.value;
        } else {
            PrintLn(stderr, "Unknown option '%1'", opt_parser.current_option);
            PrintLn(stderr, "%1", usage_str);
            return false;
        }
    };

    COMMAND(dump) {
        bool headers = false;
        {
            const char *opt;
            while ((opt = opt_parser.ConsumeOption())) {
                if (TestOption(opt, "--help")) {
                    PrintLn(stdout, "%1", dump_usage_str);
                    return 0;
                } else if (TestOption(opt, "-h", "--headers")) {
                    headers = true;
                } else if (!HandleMainOption(dump_usage_str)) {
                    return 1;
                }
            }
        }

        DynamicArray<const char *> filenames;
        opt_parser.ConsumeNonOptions(&filenames);

        if (filenames.len) {
            ClassifierStore store;
            if (!LoadClassifierStore(filenames, &store) && !store.sets.len)
                return 1;
            DumpClassifierStore(store, !headers);
        } else {
            const ClassifierStore *store = GetMainClassifierStore();
            if (!store)
                return 1;
            DumpClassifierStore(*store, !headers);
        }

        return 0;
    }

    COMMAND(list) {
        DynamicArray<const char *> spec_strings;
        bool verbose = false;
        {
            const char *opt;
            while ((opt = opt_parser.ConsumeOption())) {
                if (TestOption(opt, "--help")) {
                    PrintLn(stdout, list_usage_str);
                    return 0;
                } else if (TestOption(opt, "-v", "--verbose")) {
                    verbose = true;
                } else if (!HandleMainOption(list_usage_str)) {
                    return 1;
                }
            }

            opt_parser.ConsumeNonOptions(&spec_strings);
            if (!spec_strings.len) {
                PrintLn(stderr, "No specifier provided");
                PrintLn(stderr, list_usage_str);
                return 1;
            }
        }

        const ClassifierStore *store;
        const ClassifierSet *store_set;
        {
            store = GetMainClassifierStore();
            if (!store)
                return 1;
            store_set = store->FindSet(main_set_date);
            if (!store_set) {
                LogError("No classifier set available at '%1'", main_set_date);
                return 1;
            }
        }

        for (const char *spec_str: spec_strings) {
            ListSpecifier spec;
            if (!ParseListSpecifier(spec_str, &spec))
                continue;

            PrintLn("%1:", spec_str);
            switch (spec.table) {
                case ListSpecifier::Table::Diagnoses: {
                    for (const DiagnosisInfo &diag: store->diagnoses.Take(store_set->diagnoses)) {
                        if (diag.flags & (int)DiagnosisInfo::Flag::SexDifference) {
                            if (spec.Match(diag.sex[0].values)) {
                                PrintLn("  %1 (male)", diag.code);
                            }
                            if (spec.Match(diag.sex[1].values)) {
                                PrintLn("  %1 (female)", diag.code);
                            }
                        } else {
                            if (spec.Match(diag.sex[0].values)) {
                                PrintLn("  %1", diag.code);
                            }
                        }
                    }
                } break;

                case ListSpecifier::Table::Procedures: {
                    for (const ProcedureInfo &proc: store->procedures.Take(store_set->procedures)) {
                        if (spec.Match(proc.values)) {
                            PrintLn("  %1", proc.code);
                        }
                    }
                } break;
            }
            PrintLn();
        }

        return 0;
    }

    COMMAND(tables) {
        bool verbose = false;
        {
            const char *opt;
            while ((opt = opt_parser.ConsumeOption())) {
                if (TestOption(opt, "--help")) {
                    PrintLn(stdout, "%1", tables_usage_str);
                    return 0;
                } else if (TestOption(opt, "-v", "--verbose")) {
                    verbose = true;
                } else if (!HandleMainOption(tables_usage_str)) {
                    return 1;
                }
            }
        }

        const ClassifierStore *store = GetMainClassifierStore();
        if (!store)
            return 1;

        for (const ClassifierSet &set: store->sets) {
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

    COMMAND(pricing) {
        PrintLn(stderr, "Not implemented");
        return 1;
    }

    COMMAND(run) {
        DynamicArray<const char *> filenames;
        {
            const char *opt;
            while ((opt = opt_parser.ConsumeOption())) {
                if (TestOption(opt, "--help")) {
                    PrintLn(stdout, run_usage_str);
                    return 0;
                } else if (!HandleMainOption(run_usage_str)) {
                    return 1;
                }
            }

            opt_parser.ConsumeNonOptions(&filenames);
            if (!filenames.len) {
                PrintLn(stderr, "No filename provided");
                PrintLn(stderr, run_usage_str);
                return 1;
            }
        }

        StaySet stay_set;
        {
            StaySetBuilder stay_set_builder;
            for (size_t i = 0; i < 500; i++) {
                if (!stay_set_builder.LoadJson(filenames))
                    return 1;
            }
            if (!stay_set_builder.Validate(&stay_set))
                return 1;
        }

        PrintLn("%1 -- %2 -- %3",
                stay_set.stays.len, stay_set.diagnoses.len, stay_set.procedures.len);

        return 0;
    }

#undef REQUIRE_OPTION_VALUE
#undef COMMAND

    PrintLn(stderr, "Unknown command '%1'", cmd);
    PrintLn(stderr, "%1", main_usage_str);
    return 1;
}
