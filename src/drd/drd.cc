/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../moya/libmoya.hh"
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

    bool Match(ArrayRef<const uint8_t> values) const
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

static bool RunBundle(ArrayRef<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: drd bundle [options] stay_file ... dest_file
)");
        PrintLn(fp, "%1", main_options_usage);
    };

    OptionParser opt_parser(arguments);

    HeapArray<const char *> filenames;
    const char *dest_filename;
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

        opt_parser.ConsumeNonOptions(&filenames);
        if (filenames.len < 2) {
            if (filenames.len) {
                PrintLn(stderr, "A destination filename must be provided");
            } else {
                PrintLn(stderr, "No filename provided");
            }
            PrintUsage(stderr);
            return false;
        }
        dest_filename = filenames[filenames.len - 1];
        filenames.RemoveLast(1);
    }

    LogDebug("Load");
    StaySet stay_set;
    {
        StaySetBuilder stay_set_builder;

        if (!stay_set_builder.LoadFile(filenames))
            return false;
        if (!stay_set_builder.Finish(&stay_set))
            return false;
    }

    LogDebug("Bundle");
    {
        FILE *fp = fopen(dest_filename, "wb" FOPEN_COMMON_FLAGS);
        if (!fp) {
            LogError("Cannot open '%1': %2", dest_filename, strerror(errno));
            return false;
        }
        DEFER { fclose(fp); };

        if (!stay_set.SaveBundle(fp, dest_filename))
            return false;
    }

    return true;
}

static bool RunClassify(ArrayRef<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: drd classify [options] stay_file ...

Classify options:
        --cluster_mode <mode>    Change stay cluster mode
                                 (bill_id*, stay_modes, disable)
    -v, --verbose                Show more classification details (cumulative)

        --test                   Enable testing against GenRSA values
)");
        PrintLn(fp, "%1", main_options_usage);
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

                if (StrTest(mode_str, "bill_id")) {
                    cluster_mode = ClusterMode::BillId;
                } else if (StrTest(mode_str, "stay_modes")) {
                    cluster_mode = ClusterMode::StayModes;
                } else if (StrTest(mode_str, "disable")) {
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

#ifdef DISABLE_TESTS
    if (test) {
        LogError("Test is not available in this build");
        test = false;
    }
#endif

    const TableSet *table_set = GetMainTableSet();
    if (!table_set)
        return false;
    const AuthorizationSet *authorization_set = GetMainAuthorizationSet();
    if (!authorization_set)
        return false;
    const PricingSet *pricing_set = GetMainPricingSet();
    if (!pricing_set) {
        LogError("No pricing information will be available");
    }

    LogDebug("Load");
    StaySet stay_set;
    {
        StaySetBuilder stay_set_builder;

        if (!stay_set_builder.LoadFile(filenames))
            return false;
        if (!stay_set_builder.Finish(&stay_set))
            return false;
    }

    LogDebug("Classify");
    ClassifyResultSet result_set = {};
    Classify(*table_set, *authorization_set, pricing_set, stay_set.stays, cluster_mode,
             &result_set);

    LogDebug("Summary");
    PrintLn("Summary:");
    PrintLn("  Total GHS: %1 €", FmtDouble(result_set.ghs_total_cents / 100.0, 2));
    PrintLn("  Supplements: REA %1, REASI %2, SI %3, SRC %4, NN1 %5, NN2 %6, NN3 %7, REP %8",
            result_set.supplements.rea, result_set.supplements.reasi, result_set.supplements.si,
            result_set.supplements.src, result_set.supplements.nn1, result_set.supplements.nn2,
            result_set.supplements.nn3, result_set.supplements.rep);
    PrintLn();

    if (verbosity >= 1 || test) {
        LogDebug("Export");
        PrintLn("Details:");
        for (const ClassifyResult &result: result_set.results) {
            PrintLn("  %1 [%2 -- %3 (%4)] = GHM %5", result.stays[0].stay_id,
                    result.stays[0].dates[0], result.stays[result.stays.len - 1].dates[1],
                    result.stays.len, result.ghm);

            if (verbosity >= 2) {
                if (result.errors.len) {
                    Print("    Errors:");
                    for (int16_t error: result.errors) {
                        Print(" %1", error);
                    }
                    PrintLn();
                }

                PrintLn("    GHS: %1 => %2 €",
                        result.ghs, FmtDouble(result.ghs_price_cents / 100.0, 2));
                if (result.supplements.rea || result.supplements.reasi || result.supplements.si ||
                        result.supplements.src || result.supplements.nn1 || result.supplements.nn2 ||
                        result.supplements.nn3 || result.supplements.rep) {
                    PrintLn("    Supplements: REA %1, REASI %2, SI %3, SRC %4, NN1 %5, NN2 %6, NN3 %7, REP %8",
                            result.supplements.rea, result.supplements.reasi, result.supplements.si,
                            result.supplements.src, result.supplements.nn1, result.supplements.nn2,
                            result.supplements.nn3, result.supplements.rep);
                }
            }

#ifndef DISABLE_TESTS
            if (test) {
                if (result.stays.len != result.stays[0].test.cluster_len) {
                    PrintLn("    Test_Error / Inadequate Cluster (%1, expected %2)",
                            result.stays.len, result.stays[0].test.cluster_len);
                }
                if (result.stays[0].test.ghm.IsValid()
                        && result.ghm != result.stays[0].test.ghm) {
                    PrintLn("    Test_Error / Wrong GHM (%1, expected %2)",
                            result.ghm, result.stays[0].test.ghm);
                }
                if (result.stays[0].test.ghs.IsValid()) {
                    if (result.ghs != result.stays[0].test.ghs) {
                        PrintLn("    Test_Error / Wrong GHS (%1, expected %2)",
                                result.ghs, result.stays[0].test.ghs);
                    }
                    if (result.stays[0].test.supplements.rea != result.supplements.rea) {
                        PrintLn("    Test_Error / Wrong Supplement REA (%1, expected %2)",
                                result.supplements.rea, result.stays[0].test.supplements.rea);
                    }
                    if (result.stays[0].test.supplements.reasi != result.supplements.reasi) {
                        PrintLn("    Test_Error / Wrong Supplement REASI (%1, expected %2)",
                                result.supplements.reasi, result.stays[0].test.supplements.reasi);
                    }
                    if (result.stays[0].test.supplements.si != result.supplements.si) {
                        PrintLn("    Test_Error / Wrong Supplement SI (%1, expected %2)",
                                result.supplements.si, result.stays[0].test.supplements.si);
                    }
                    if (result.stays[0].test.supplements.src != result.supplements.src) {
                        PrintLn("    Test_Error / Wrong Supplement SRC (%1, expected %2)",
                                result.supplements.src, result.stays[0].test.supplements.src);
                    }
                    if (result.stays[0].test.supplements.nn1 != result.supplements.nn1) {
                        PrintLn("    Test_Error / Wrong Supplement NN1 (%1, expected %2)",
                                result.supplements.nn1, result.stays[0].test.supplements.nn1);
                    }
                    if (result.stays[0].test.supplements.nn2 != result.supplements.nn2) {
                        PrintLn("    Test_Error / Wrong Supplement NN2 (%1, expected %2)",
                                result.supplements.nn2, result.stays[0].test.supplements.nn2);
                    }
                    if (result.stays[0].test.supplements.nn3 != result.supplements.nn3) {
                        PrintLn("    Test_Error / Wrong Supplement NN3 (%1, expected %2)",
                                result.supplements.nn3, result.stays[0].test.supplements.nn3);
                    }
                    if (result.stays[0].test.supplements.rep != result.supplements.rep) {
                        PrintLn("    Test_Error / Wrong Supplement REP (%1, expected %2)",
                                result.supplements.rep, result.stays[0].test.supplements.rep);
                    }
                }
            }
#endif
        }
    }

    return true;
}

static bool RunConstraints(ArrayRef<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: drd constraints [options]

Constraints options:
    -d, --date <date>            Use tables valid on specified date
                                 (default: most recent tables)
)");
        PrintLn(fp, "%1", main_options_usage);
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

    LogDebug("Computing");
    HashSet<GhmCode, GhmConstraint> ghm_constraints;
    if (!ComputeGhmConstraints(*index, &ghm_constraints))
        return false;

    LogDebug("Export");
    for (const GhsInfo &ghs_info: index->ghs)  {
        const GhmConstraint *constraint = ghm_constraints.Find(ghs_info.ghm);
        if (constraint) {
            PrintLn("Constraint for %1", ghs_info.ghm);
            PrintLn("  Duration = %1", FmtHex(constraint->duration_mask));
        } else {
            PrintLn("%1 unreached!", ghs_info.ghm);
        }
    }

    return true;
}

static bool RunDump(ArrayRef<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: drd dump [options] [filename] ...

Dump options:
    -h, --headers                Print only table headers
)");
        PrintLn(fp, "%1", main_options_usage);
    };

    OptionParser opt_parser(arguments);

    bool headers = false;
    HeapArray<const char *> filenames;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return true;
            } else if (TestOption(opt, "-h", "--headers")) {
                headers = true;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&filenames);
    }

    if (filenames.len) {
        TableSet table_set;
        if (!LoadTableFiles(filenames, &table_set) && !table_set.indexes.len)
            return false;
        DumpTableSet(table_set, !headers);
    } else {
        const TableSet *table_set = GetMainTableSet();
        if (!table_set)
            return false;
        DumpTableSet(*table_set, !headers);
    }

    return true;
}

static bool RunInfo(ArrayRef<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: drd info [options] name ...
)");
        PrintLn(fp, "%1", main_options_usage);
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
            ArrayRef<const ProcedureInfo> proc_info = index->FindProcedure(proc);
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

static bool RunIndexes(ArrayRef<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: drd indexes [options]

Indexes options:
    -v, --verbose                Show more detailed information
)");
        PrintLn(fp, "%1", main_options_usage);
    };

    OptionParser opt_parser(arguments);

    bool verbose = false;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return true;
            } else if (TestOption(opt, "-v", "--verbose")) {
                verbose = true;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return false;
            }
        }
    }

    const TableSet *table_set = GetMainTableSet();
    if (!table_set)
        return false;

    for (const TableIndex &index: table_set->indexes) {
        PrintLn("%1 to %2:", index.limit_dates[0], index.limit_dates[1]);
        for (const TableInfo *table: index.tables) {
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

    return true;
}

static bool RunList(ArrayRef<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: drd list [options] list_name ...

List options:
    -d, --date <date>            Use tables valid on specified date
                                 (default: most recent tables)
)");
        PrintLn(fp, "%1", main_options_usage);
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

static bool RunPricing(ArrayRef<const char *> arguments)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: drd pricing [options]
)");
        PrintLn(fp, "%1", main_options_usage);
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

    const PricingSet *pricing_set = GetMainPricingSet();
    if (!pricing_set)
        return false;

    DumpPricingSet(*pricing_set);
    return true;
}

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: drd <command> [<args>]

Commands:
    bundle                       Bundle stays for quicker loads
    classify                     Classify stays
    constraints                  Compute GHM accessibility constraints
    dump                         Dump available tables and lists
    info                         Print information about individual elements
                                 (diagnoses, procedures, GHM roots, etc.)
    indexes                      Show table and price indexes
    list                         Export diagnosis and procedure lists
    pricing                      Dump GHS pricings
)");
        PrintLn(fp, "%1", main_options_usage);
    };

    Allocator temp_alloc;

    if (argc < 2) {
        PrintUsage(stderr);
        return 1;
    }
    if (StrTest(argv[1], "--help") || StrTest(argv[1], "help")) {
        if (argc > 2 && argv[2][0] != '-') {
            std::swap(argv[1], argv[2]);
            argv[2] = (char *)"--help";
        } else {
            PrintUsage(stdout);
            return 0;
        }
    }

    // Add default data directory
    {
        const char *default_data_dir = Fmt(&temp_alloc, "%1%/data", GetExecutableDirectory()).ptr;
        main_data_directories.Append(default_data_dir);
    }

#define HANDLE_COMMAND(Cmd, Func) \
        do { \
            if (StrTest(cmd, STRINGIFY(Cmd))) { \
                return !Func(arguments); \
            } \
        } while (false)

    const char *cmd = argv[1];
    ArrayRef<const char *> arguments((const char **)argv + 2, argc - 2);

    HANDLE_COMMAND(bundle, RunBundle);
    HANDLE_COMMAND(classify, RunClassify);
    HANDLE_COMMAND(constraints, RunConstraints);
    HANDLE_COMMAND(dump, RunDump);
    HANDLE_COMMAND(info, RunInfo);
    HANDLE_COMMAND(indexes, RunIndexes);
    HANDLE_COMMAND(list, RunList);
    HANDLE_COMMAND(pricing, RunPricing);

#undef HANDLE_COMMAND

    PrintLn(stderr, "Unknown command '%1'", cmd);
    PrintUsage(stderr);
    return 1;
}
