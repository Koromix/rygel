// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"
#include "dump.hh"

struct ListSpecifier {
    enum class Table {
        Diagnoses,
        Procedures
    };
    enum class Type {
        Mask,
        CmdJump
    };

    bool valid;

    Table table;
    Type type;
    union {
        struct {
            uint8_t offset;
            uint8_t mask;
        } mask;

        struct {
            uint8_t cmd;
            uint8_t jump;
        } cmd_jump;
    } u;

    static ListSpecifier FromString(const char *spec_str)
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

            case '-': {
                spec.type = ListSpecifier::Type::CmdJump;
                if (sscanf(spec_str + 2, "%02" SCNu8 "%02" SCNu8,
                           &spec.u.cmd_jump.cmd, &spec.u.cmd_jump.jump) != 2)
                    goto error;
            } break;

            default:
                goto error;
        }

        spec.valid = true;
        return spec;

error:
        LogError("Malformed list specifier '%1'", spec_str);
        return spec;
    }

    bool IsValid() const { return valid; }

    bool Match(Span<const uint8_t> values) const
    {
        switch (type) {
            case Type::Mask: {
                return LIKELY(u.mask.offset < values.len) &&
                       values[u.mask.offset] & u.mask.mask;
            } break;

            case Type::CmdJump: {
                return values[0] == u.cmd_jump.cmd &&
                       values[1] == u.cmd_jump.jump;
            } break;
        }

        Assert(false);
    }
};

static bool RunCatalogs(Span<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdc catalogs [options]
)");
        PrintLn(fp, main_options_usage);
    };

    OptionParser opt_parser(arguments);

    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return true;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return false;
            }
        }
    }

    const CatalogSet *catalog_set = GetMainCatalogSet();
    if (!catalog_set)
        return false;
    DumpCatalogSet(*catalog_set);

    return true;
}

static void PrintSummary(const ClassifySummary &summary)
{
    PrintLn("  Results: %1", summary.results_count);
    PrintLn("  Stays: %1", summary.stays_count);
    PrintLn("  Total GHS: %1 €",
            FmtDouble((double)summary.ghs_total_cents / 100.0, 2));
    PrintLn("  Supplements: REA %1, REASI %2, SI %3, SRC %4, NN1 %5, NN2 %6, NN3 %7, REP %8",
            summary.supplements.rea, summary.supplements.reasi,
            summary.supplements.si, summary.supplements.src,
            summary.supplements.nn1, summary.supplements.nn2,
            summary.supplements.nn3, summary.supplements.rep);
    PrintLn();
};

static bool RunClassify(Span<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdc classify [options] stay_file ...

Classify options:
        --cluster_mode <mode>    Change stay cluster mode
                                 (bill_id*, stay_modes, disable)
    -v, --verbose                Show more classification details (cumulative)

        --test                   Enable testing against GenRSA values
)");
        PrintLn(fp, main_options_usage);
    };

    OptionParser opt_parser(arguments);

    HeapArray<const char *> filenames;
    ClusterMode cluster_mode = ClusterMode::BillId;
    int verbosity = 0;
    bool test = false;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return true;
            } else if (TestOption(opt, "--cluster_mode")) {
                const char *mode_str = opt_parser.RequireOptionValue(PrintUsage);
                if (!mode_str)
                    return false;

                if (TestStr(mode_str, "bill_id")) {
                    cluster_mode = ClusterMode::BillId;
                } else if (TestStr(mode_str, "stay_modes")) {
                    cluster_mode = ClusterMode::StayModes;
                } else if (TestStr(mode_str, "disable")) {
                    cluster_mode = ClusterMode::Disable;
                } else {
                    LogError("Unknown cluster mode '%1'", mode_str);
                    return false;
                }
            } else if (TestOption(opt, "-v", "--verbose")) {
                verbosity++;
            } else if (TestOption(opt, "--test")) {
                test = true;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&filenames);
        if (!filenames.len) {
            PrintLn(stderr, "No filename provided");
            PrintUsage(stderr);
            return false;
        }
    }

    const TableSet *table_set = GetMainTableSet();
    if (!table_set || !table_set->indexes.len)
        return false;
    const AuthorizationSet *authorization_set = GetMainAuthorizationSet();
    if (!authorization_set)
        return false;

    struct ClassifySet {
        StaySet stay_set;
        HashTable<int32_t, StayTest> tests;

        HeapArray<ClassifyResult> results;
        ClassifySummary summary;
    };
    HeapArray<ClassifySet> classify_sets;
    classify_sets.AppendDefault(filenames.len);

    Async async;
    for (Size i = 0; i < filenames.len; i++) {
        async.AddTask([&, i]() {
            ClassifySet *classify_set = &classify_sets[i];

            LogInfo("Load '%1'", filenames[i]);
            StaySetBuilder stay_set_builder;
            if (!stay_set_builder.LoadFiles(filenames[i], test ? &classify_set->tests : nullptr))
                return false;
            if (!stay_set_builder.Finish(&classify_set->stay_set))
                return false;

            LogInfo("Classify '%1'", filenames[i]);
            Classify(*table_set, *authorization_set, classify_set->stay_set.stays, cluster_mode,
                     &classify_set->results);

            LogInfo("Summarize '%1'", filenames[i]);
            Summarize(classify_set->results, &classify_set->summary);

            return true;
        });
    }
    if (!async.Sync())
        return false;

    LogInfo("Export");
    ClassifySummary main_summary = {};
    for (Size i = 0; i < filenames.len; i++) {
        const ClassifySet &classify_set = classify_sets[i];

        PrintLn("%1:", filenames[i]);

        if (verbosity >= 1) {
            PrintLn("  Detailed results:");
            for (const ClassifyResult &result: classify_set.results) {
                PrintLn("    %1 [%2 -- %3 (%4)] = GHM %5 / GHS %6", result.stays[0].bill_id,
                        result.stays[0].entry.date, result.stays[result.stays.len - 1].exit.date,
                        result.stays.len, result.ghm, result.ghs);

                if (verbosity >= 2) {
                    if (result.main_error) {
                        PrintLn("      Error: %1", result.main_error);
                    }

                    PrintLn("      GHS price: %1 €", FmtDouble((double)result.ghs_price_cents / 100.0, 2));
                    if (result.supplements.rea || result.supplements.reasi || result.supplements.si ||
                            result.supplements.src || result.supplements.nn1 || result.supplements.nn2 ||
                            result.supplements.nn3 || result.supplements.rep) {
                        PrintLn("      Supplements: REA %1, REASI %2, SI %3, SRC %4, NN1 %5, NN2 %6, NN3 %7, REP %8",
                                result.supplements.rea, result.supplements.reasi, result.supplements.si,
                                result.supplements.src, result.supplements.nn1, result.supplements.nn2,
                                result.supplements.nn3, result.supplements.rep);
                    }
                }
            }
            PrintLn();
        }

        PrintSummary(classify_set.summary);
        main_summary += classify_set.summary;

        if (test) {
            PrintLn("  Tests:");

            Size tested_clusters = 0, failed_clusters = 0;
            Size tested_ghm = 0, failed_ghm = 0;
            Size tested_ghs = 0, failed_ghs = 0;
            for (const ClassifyResult &result: classify_set.results) {
                const StayTest *stay_test = classify_set.tests.Find(result.stays[0].bill_id);
                if (!stay_test)
                    continue;

                if (stay_test->cluster_len) {
                    tested_clusters++;
                    if (result.stays.len != stay_test->cluster_len) {
                        failed_clusters++;
                        if (verbosity >= 1) {
                            PrintLn("    %1 has inadequate cluster (%2, expected %3)",
                                    stay_test->bill_id, result.stays.len, stay_test->cluster_len);
                        }
                    }
                }

                if (stay_test->ghm.value) {
                    tested_ghm++;
                    if (stay_test->ghm != result.ghm) {
                        failed_ghm++;
                        if (verbosity >= 1) {
                            PrintLn("    %1 has inadequate GHM (%2 [%3], expected %4 [%5])",
                                    stay_test->bill_id, result.ghm, result.main_error,
                                    stay_test->ghm, stay_test->error);
                        }
                    }
                }

                if (stay_test->ghs.number) {
                    tested_ghs++;
                    if (stay_test->ghs != result.ghs ||
                            stay_test->supplements.rea != result.supplements.rea ||
                            stay_test->supplements.reasi != result.supplements.reasi ||
                            stay_test->supplements.si != result.supplements.si ||
                            stay_test->supplements.src != result.supplements.src ||
                            stay_test->supplements.nn1 != result.supplements.nn1 ||
                            stay_test->supplements.nn2 != result.supplements.nn2 ||
                            stay_test->supplements.nn3 != result.supplements.nn3 ||
                            stay_test->supplements.rep != result.supplements.rep) {
                        failed_ghs++;
                        if (verbosity >= 1) {
                            if (result.ghs != stay_test->ghs) {
                                PrintLn("    %1 has inadequate GHS (%2, expected %3)",
                                        stay_test->bill_id, result.ghs, stay_test->ghs);
                            }
                            if (result.supplements.rea != stay_test->supplements.rea) {
                                PrintLn("    %1 has inadequate REA (%2, expected %3)",
                                        stay_test->bill_id, result.supplements.rea, stay_test->supplements.rea);
                            }
                            if (result.supplements.reasi != stay_test->supplements.reasi) {
                                PrintLn("    %1 has inadequate REASI (%2, expected %3)",
                                        stay_test->bill_id, result.supplements.reasi, stay_test->supplements.reasi);
                            }
                            if (result.supplements.si != stay_test->supplements.si) {
                                PrintLn("    %1 has inadequate SI (%2, expected %3)",
                                        stay_test->bill_id, result.supplements.si, stay_test->supplements.si);
                            }
                            if (result.supplements.src != stay_test->supplements.src) {
                                PrintLn("    %1 has inadequate SRC (%2, expected %3)",
                                        stay_test->bill_id, result.supplements.src, stay_test->supplements.src);
                            }
                            if (result.supplements.nn1 != stay_test->supplements.nn1) {
                                PrintLn("    %1 has inadequate NN1 (%2, expected %3)",
                                        stay_test->bill_id, result.supplements.nn1, stay_test->supplements.nn1);
                            }
                            if (result.supplements.nn2 != stay_test->supplements.nn2) {
                                PrintLn("    %1 has inadequate NN2 (%2, expected %3)",
                                        stay_test->bill_id, result.supplements.nn2, stay_test->supplements.nn2);
                            }
                            if (result.supplements.nn3 != stay_test->supplements.nn3) {
                                PrintLn("    %1 has inadequate NN3 (%2, expected %3)",
                                        stay_test->bill_id, result.supplements.nn3, stay_test->supplements.nn3);
                            }
                            if (result.supplements.rep != stay_test->supplements.rep) {
                                PrintLn("    %1 has inadequate REP (%2, expected %3)",
                                        stay_test->bill_id, result.supplements.rep, stay_test->supplements.rep);
                            }
                        }
                    }
                }
            }
            if (verbosity >= 1 && (failed_clusters || failed_ghm || failed_ghs)) {
                PrintLn();
            }

            PrintLn("    Failed cluster tests: %1 / %2 (missing %3)",
                    failed_clusters, tested_clusters, classify_set.results.len - tested_clusters);
            PrintLn("    Failed GHM tests: %1 / %2 (missing %3)",
                    failed_ghm, tested_ghm, classify_set.results.len - tested_ghm);
            PrintLn("    Failed GHS (and supplements) tests: %1 / %2 (missing %3)",
                    failed_ghs, tested_ghs, classify_set.results.len - tested_ghs);
            PrintLn();
        }
    }

    if (filenames.len > 1) {
        PrintLn("Global summary:");
        PrintSummary(main_summary);
    }

    return true;
}

static bool RunConstraints(Span<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdc constraints [options]

Constraints options:
    -d, --date <date>            Use tables valid on specified date
                                 (default: most recent tables)
)");
        PrintLn(fp, main_options_usage);
    };

    OptionParser opt_parser(arguments);

    Date index_date = {};
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return true;
            } else if (TestOption(opt_parser.current_option, "-d", "--date")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return false;
                index_date = Date::FromString(opt_parser.current_value);
                if (!index_date.value)
                    return false;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return false;
            }
        }
    }

    const TableSet *table_set;
    const TableIndex *index;
    {
        table_set = GetMainTableSet();
        if (!table_set)
            return false;
        index = table_set->FindIndex(index_date);
        if (!index) {
            LogError("No table index available at '%1'", index_date);
            return false;
        }
    }

    LogInfo("Computing");
    HashTable<GhmCode, GhmConstraint> ghm_constraints;
    if (!ComputeGhmConstraints(*index, &ghm_constraints))
        return false;

    LogInfo("Export");
    for (const GhsAccessInfo &ghs_access_info: index->ghs)  {
        const GhmConstraint *constraint = ghm_constraints.Find(ghs_access_info.ghm);
        if (constraint) {
            PrintLn("Constraint for %1", ghs_access_info.ghm);
            PrintLn("  Duration = %1", FmtHex(constraint->duration_mask));
        } else {
            PrintLn("%1 unreached!", ghs_access_info.ghm);
        }
    }

    return true;
}

static bool RunInfo(Span<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdc info [options] name ...
)");
        PrintLn(fp, main_options_usage);
    };

    OptionParser opt_parser(arguments);

    Date index_date = {};
    HeapArray<const char *> names;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return true;
            } else if (TestOption(opt_parser.current_option, "-d", "--date")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return false;
                index_date = Date::FromString(opt_parser.current_value);
                if (!index_date.value)
                    return false;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&names);
        if (!names.len) {
            PrintLn(stderr, "No element name provided");
            PrintUsage(stderr);
            return false;
        }
    }

    const TableSet *table_set;
    const TableIndex *index;
    {
        table_set = GetMainTableSet();
        if (!table_set)
            return false;
        index = table_set->FindIndex(index_date);
        if (!index) {
            LogError("No table index available at '%1'", index_date);
            return false;
        }
    }

    for (const char *name: names) {
        {
            DiagnosisCode diag = DiagnosisCode::FromString(name, false);
            const DiagnosisInfo *diag_info = index->FindDiagnosis(diag);
            if (diag_info) {
                DumpDiagnosisTable(*diag_info, index->exclusions);
                continue;
            }
        }

        {
            ProcedureCode proc = ProcedureCode::FromString(name, false);
            Span<const ProcedureInfo> proc_info = index->FindProcedure(proc);
            if (proc_info.len) {
                DumpProcedureTable(proc_info);
                continue;
            }
        }

        {
            GhmRootCode ghm_root = GhmRootCode::FromString(name, false);
            const GhmRootInfo *ghm_root_info = index->FindGhmRoot(ghm_root);
            if (ghm_root_info) {
                DumpGhmRootTable(*ghm_root_info);
                continue;
            }
        }

        PrintLn(stderr, "Unknown element '%1'", name);
    }

    return true;
}

static bool RunList(Span<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdc list [options] list_name ...

List options:
    -d, --date <date>            Use tables valid on specified date
                                 (default: most recent tables)
)");
        PrintLn(fp, main_options_usage);
    };

    OptionParser opt_parser(arguments);

    Date index_date = {};
    HeapArray<const char *> spec_strings;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return true;
            } else if (TestOption(opt_parser.current_option, "-d", "--date")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return false;
                index_date = Date::FromString(opt_parser.current_value);
                if (!index_date.value)
                    return false;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&spec_strings);
        if (!spec_strings.len) {
            PrintLn(stderr, "No specifier provided");
            PrintUsage(stderr);
            return false;
        }
    }

    const TableSet *table_set;
    const TableIndex *index;
    {
        table_set = GetMainTableSet();
        if (!table_set)
            return false;
        index = table_set->FindIndex(index_date);
        if (!index) {
            LogError("No table index available at '%1'", index_date);
            return false;
        }
    }

    for (const char *spec_str: spec_strings) {
        ListSpecifier spec = ListSpecifier::FromString(spec_str);
        if (!spec.IsValid())
            continue;

        PrintLn("%1:", spec_str);
        switch (spec.table) {
            case ListSpecifier::Table::Diagnoses: {
                for (const DiagnosisInfo &diag: index->diagnoses) {
                    if (diag.flags & (int)DiagnosisInfo::Flag::SexDifference) {
                        if (spec.Match(diag.Attributes(Sex::Male).raw)) {
                            PrintLn("  %1 (male)", diag.diag);
                        }
                        if (spec.Match(diag.Attributes(Sex::Female).raw)) {
                            PrintLn("  %1 (female)", diag.diag);
                        }
                    } else {
                        if (spec.Match(diag.Attributes(Sex::Male).raw)) {
                            PrintLn("  %1", diag.diag);
                        }
                    }
                }
            } break;

            case ListSpecifier::Table::Procedures: {
                for (const ProcedureInfo &proc: index->procedures) {
                    if (spec.Match(proc.bytes)) {
                        PrintLn("  %1", proc.proc);
                    }
                }
            } break;
        }
        PrintLn();
    }

    return true;
}

static bool RunPack(Span<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdc pack [options] stay_file ... -O output_file
)");
        PrintLn(fp, main_options_usage);
    };

    OptionParser opt_parser(arguments);

    HeapArray<const char *> filenames;
    const char *dest_filename = nullptr;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return true;
            } else if (TestOption(opt, "-O", "--output")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return false;
                dest_filename = opt_parser.current_value;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&filenames);
        if (!dest_filename) {
            PrintLn(stderr, "A destination file must be provided (--output)");
            PrintUsage(stderr);
            return false;
        }
        if (!filenames.len) {
            PrintLn(stderr, "No stay file provided");
            PrintUsage(stderr);
            return false;
        }
    }

    LogInfo("Load");
    StaySet stay_set;
    {
        StaySetBuilder stay_set_builder;

        if (!stay_set_builder.LoadFiles(filenames))
            return false;
        if (!stay_set_builder.Finish(&stay_set))
            return false;
    }

    LogInfo("Pack");
    if (!stay_set.SavePack(dest_filename))
        return false;

    return true;
}

static bool RunTables(Span<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdc tables [options] [filename] ...

Dump options:
    -d, --dump                   Dump content of (readable) tables
)");
        PrintLn(fp, main_options_usage);
    };

    OptionParser opt_parser(arguments);

    bool dump = false;
    HeapArray<const char *> filenames;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return true;
            } else if (TestOption(opt, "-d", "--dump")) {
                dump = true;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&filenames);
    }

    const TableSet *table_set = GetMainTableSet();
    if (!table_set || !table_set->indexes.len)
        return false;
    DumpTableSetHeaders(*table_set);
    if (dump) {
        DumpTableSetContent(*table_set);
    }

    return true;
}

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdc <command> [<args>]

Commands:
    classify                     Classify stays
    constraints                  Compute GHM accessibility constraints
    info                         Print information about individual elements
                                 (diagnoses, procedures, GHM roots, etc.)
    list                         Export diagnosis and procedure lists
    pack                         Pack stays for quicker loads
    tables                       Dump available tables and lists
)");
        PrintLn(fp, main_options_usage);
    };

    LinkedAllocator temp_alloc;

    if (argc < 2) {
        PrintUsage(stderr);
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            cmd = arguments[0];
            arguments[0] = "--help";
        } else {
            PrintUsage(stdout);
            return 0;
        }
    }

    // Add default data directory
    {
        const char *app_dir = GetApplicationDirectory();
        if (app_dir) {
            const char *default_data_dir = Fmt(&temp_alloc, "%1%/data", app_dir).ptr;
            main_data_directories.Append(default_data_dir);
        }
    }

#define HANDLE_COMMAND(Cmd, Func) \
        do { \
            if (TestStr(cmd, STRINGIFY(Cmd))) { \
                return !Func(arguments); \
            } \
        } while (false)

    HANDLE_COMMAND(catalogs, RunCatalogs);
    HANDLE_COMMAND(classify, RunClassify);
    HANDLE_COMMAND(constraints, RunConstraints);
    HANDLE_COMMAND(info, RunInfo);
    HANDLE_COMMAND(list, RunList);
    HANDLE_COMMAND(pack, RunPack);
    HANDLE_COMMAND(tables, RunTables);

#undef HANDLE_COMMAND

    PrintLn(stderr, "Unknown command '%1'", cmd);
    PrintUsage(stderr);
    return 1;
}
