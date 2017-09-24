/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../core/kutil.hh"
#include "../core/algorithm.hh"
#include "dump.hh"
#include "../core/pricing.hh"
#include "../core/stays.hh"
#include "../core/tables.hh"

static const char *const MainUsageText =
R"(Usage: moya <command> [<args>]

Commands:
    dump                         Dump available tables and lists
    info                         Print information about individual elements
                                 (diagnoses, procedures, GHM roots, etc.)
    indexes                      Show table and price indexes
    list                         Export diagnosis and procedure lists
    pricings                     Print GHS pricing tables
    summarize                    Summarize stays

Global options:
    -O, --output <filename>      Dump information to file (default: stdout)

    -t, --table-file <filename>  Load table file
    -T, --table-dir <dir>        Load table directory)";

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

static HeapArray<const char *> main_table_filenames;
static TableSet main_table_set = {};

static const TableSet *GetMainTableSet()
{
    if (!main_table_set.indexes.len) {
        if (!main_table_filenames.len) {
            LogError("No table provided");
            return nullptr;
        }
        LoadTableSet(main_table_filenames, &main_table_set);
        if (!main_table_set.indexes.len)
            return nullptr;
    }

    return &main_table_set;
}

static bool HandleMainOption(OptionParser &opt_parser, Allocator &temp_alloc,
                             const char *usage_str)
{
    if (opt_parser.TestOption("-O", "--output")) {
        const char *filename = opt_parser.RequireOptionValue(MainUsageText);
        if (!filename)
            return false;

        if (!freopen(filename, "w", stdout)) {
            LogError("Cannot open '%1': %2", filename, strerror(errno));
            return false;
        }

        return true;
    } else if (opt_parser.TestOption("-T", "--table-dir")) {
        if (!opt_parser.RequireOptionValue(MainUsageText))
            return false;

        return EnumerateDirectoryFiles(opt_parser.current_value, "*.tab", temp_alloc,
                                       &main_table_filenames, 1024);
    } else if (opt_parser.TestOption("-t", "--table-file")) {
        if (!opt_parser.RequireOptionValue(MainUsageText))
            return false;

        main_table_filenames.Append(opt_parser.current_value);
        return true;
    } else {
        PrintLn(stderr, "Unknown option '%1'", opt_parser.current_option);
        PrintLn(stderr, "%1", usage_str);
        return false;
    }
}

static bool RunDump(ArrayRef<const char *> arguments)
{
    static const char *const UsageText =
R"(Usage: moya dump [options] [filename] ...

Specific options:
    -h, --headers                Print only table headers)";

    Allocator temp_alloc;
    OptionParser opt_parser(arguments);

    bool headers = false;
    HeapArray<const char *> filenames;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintLn("%1", UsageText);
                return true;
            } else if (TestOption(opt, "-h", "--headers")) {
                headers = true;
            } else if (!HandleMainOption(opt_parser, temp_alloc, UsageText)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&filenames);
    }

    if (filenames.len) {
        TableSet table_set;
        if (!LoadTableSet(filenames, &table_set) && !table_set.indexes.len)
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
    static const char *const UsageText =
R"(Usage: moya info [options] name ...)";

    Allocator temp_alloc;
    OptionParser opt_parser(arguments);

    Date index_date = {};
    HeapArray<const char *> names;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintLn("%1", UsageText);
                return true;
            } else if (TestOption(opt_parser.current_option, "-d", "--date")) {
                if (!opt_parser.RequireOptionValue(MainUsageText))
                    return false;
                index_date = Date::FromString(opt_parser.current_value);
                if (!index_date.value)
                    return false;
            } else if (!HandleMainOption(opt_parser, temp_alloc, UsageText)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&names);
        if (!names.len) {
            PrintLn(stderr, "No element name provided");
            PrintLn(stderr, UsageText);
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
            DiagnosisCode diag_code = DiagnosisCode::FromString(name, false);
            const DiagnosisInfo *diag_info = index->FindDiagnosis(diag_code);
            if (diag_info) {
                DumpDiagnosisTable(*diag_info, index->exclusions);
                continue;
            }
        }

        {
            ProcedureCode proc_code = ProcedureCode::FromString(name, false);
            ArrayRef<const ProcedureInfo> proc_info = index->FindProcedure(proc_code);
            if (proc_info.len) {
                DumpProcedureTable(proc_info);
                continue;
            }
        }

        {
            GhmRootCode ghm_root_code = GhmRootCode::FromString(name, false);
            const GhmRootInfo *ghm_root_info = index->FindGhmRoot(ghm_root_code);
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
    static const char *const UsageText =
R"(Usage: moya indexes [options]

Options:
    -v, --verbose                Show more detailed information)";

    Allocator temp_alloc;
    OptionParser opt_parser(arguments);

    bool verbose = false;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintLn("%1", UsageText);
                return true;
            } else if (TestOption(opt, "-v", "--verbose")) {
                verbose = true;
            } else if (!HandleMainOption(opt_parser, temp_alloc, UsageText)) {
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
    static const char *const UsageText =
R"(Usage: moya list [options] list_name ...)";

    Allocator temp_alloc;
    OptionParser opt_parser(arguments);

    Date index_date = {};
    HeapArray<const char *> spec_strings;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintLn("%1", UsageText);
                return true;
            } else if (TestOption(opt_parser.current_option, "-d", "--date")) {
                if (!opt_parser.RequireOptionValue(MainUsageText))
                    return false;
                index_date = Date::FromString(opt_parser.current_value);
                if (!index_date.value)
                    return false;
            } else if (!HandleMainOption(opt_parser, temp_alloc, UsageText)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&spec_strings);
        if (!spec_strings.len) {
            PrintLn(stderr, "No specifier provided");
            PrintLn(stderr, UsageText);
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
                            PrintLn("  %1 (male)", diag.code);
                        }
                        if (spec.Match(diag.Attributes(Sex::Female).raw)) {
                            PrintLn("  %1 (female)", diag.code);
                        }
                    } else {
                        if (spec.Match(diag.Attributes(Sex::Male).raw)) {
                            PrintLn("  %1", diag.code);
                        }
                    }
                }
            } break;

            case ListSpecifier::Table::Procedures: {
                for (const ProcedureInfo &proc: index->procedures) {
                    if (spec.Match(proc.bytes)) {
                        PrintLn("  %1", proc.code);
                    }
                }
            } break;
        }
        PrintLn();
    }

    return true;
}

static bool RunPricings(ArrayRef<const char *>)
{
    Allocator temp_alloc;

    ArrayRef<uint8_t> file_data;
    if (!ReadFile(&temp_alloc, "data/ghs.nx", Megabytes(30), &file_data))
        return false;

    HeapArray<GhsPricing> ghs_pricings;
    ParseGhsPricings(file_data, "data/ghs.nx", &ghs_pricings);

    for (const GhsPricing &pricing: ghs_pricings) {
        PrintLn("GHS %1 [%2 -- %3]",
                pricing.ghs_code, pricing.limit_dates[0], pricing.limit_dates[1]);
        PrintLn("  Public: %1 [exh = %2, exb = %3]",
                FmtDouble(pricing.sectors[0].price_cents / 100.0, 2),
                FmtDouble(pricing.sectors[0].exh_cents / 100.0, 2),
                FmtDouble(pricing.sectors[0].exb_cents / 100.0, 2));
        PrintLn("  Private: %1 [exh = %2, exb = %3]",
                FmtDouble(pricing.sectors[1].price_cents / 100.0, 2),
                FmtDouble(pricing.sectors[1].exh_cents / 100.0, 2),
                FmtDouble(pricing.sectors[1].exb_cents / 100.0, 2));
    }

    return true;
}

static bool RunSummarize(ArrayRef<const char *> arguments)
{
    static const char *const UsageText =
R"(Usage: moya sumarize [options] stay_file ...

Options:
    --cluster_mode <mode>      Change stay cluster mode
                               (stay_modes*, bill_id, disable))";

    Allocator temp_alloc;
    OptionParser opt_parser(arguments);

    HeapArray<const char *> filenames;
    ClusterMode cluster_mode = ClusterMode::StayModes;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintLn("%1", UsageText);
                return true;
            } else if (TestOption(opt, "--cluster_mode")) {
                const char *mode_str = opt_parser.RequireOptionValue(UsageText);
                if (!mode_str)
                    return false;

                if (!strcmp(mode_str, "stay_modes")) {
                    cluster_mode = ClusterMode::StayModes;
                } else if (!strcmp(mode_str, "bill_id")) {
                    cluster_mode = ClusterMode::BillId;
                } else if (!strcmp(mode_str, "disable")) {
                    cluster_mode = ClusterMode::Disable;
                } else {
                    LogError("Unknown cluster mode '%1'", mode_str);
                    return false;
                }
            } else if (!HandleMainOption(opt_parser, temp_alloc, UsageText)) {
                return false;
            }
        }

        opt_parser.ConsumeNonOptions(&filenames);
        if (!filenames.len) {
            PrintLn(stderr, "No filename provided");
            PrintLn(stderr, UsageText);
            return false;
        }
    }

    const TableSet *table_set = GetMainTableSet();
    if (!table_set)
        return false;

    LogDebug("Load");
    StaySet stay_set;
    {
        StaySetBuilder stay_set_builder;

        if (!stay_set_builder.LoadJson(filenames))
            return false;
        if (!stay_set_builder.Finish(&stay_set))
            return false;
    }

    LogDebug("Summarize");
    SummarizeResultSet result_set;
    Summarize(*table_set, stay_set.stays, cluster_mode, &result_set);

    LogDebug("Export");
    for (const SummarizeResult &result: result_set.results) {
        PrintLn("%1 [%2 / %3 stays] = %4", result.agg.stay.stay_id,
                result.agg.stay.dates[1], result.cluster.len, result.ghm);
        for (int16_t error: result.errors) {
            PrintLn("  Error %1", error);
        }

#ifndef DISABLE_TESTS
        if (result.ghm != result.agg.stay.test.ghm) {
            PrintLn("  Test_Error / Wrong GHM (%1, expected %2)",
                    result.ghm, result.agg.stay.test.ghm);
        }
        if (result.cluster.len != result.agg.stay.test.cluster_len) {
            PrintLn("  Test_Error / Inadequate Cluster (%1, expected %2)",
                    result.cluster.len, result.agg.stay.test.cluster_len);
        }
#endif
    }

    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        PrintLn(stderr, "%1", MainUsageText);
        return 1;
    }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "help")) {
        if (argc > 2 && argv[2][0] != '-') {
            std::swap(argv[1], argv[2]);
            argv[2] = (char *)"--help";
        } else {
            PrintLn("%1", MainUsageText);
            return 1;
        }
    }

#define HANDLE_COMMAND(Cmd, Func) \
        do { \
            if (!(strcmp(cmd, STRINGIFY(Cmd)))) { \
                return !Func(arguments); \
            } \
        } while (false)

    const char *cmd = argv[1];
    ArrayRef<const char *> arguments((const char **)argv + 2, argc - 2);

    HANDLE_COMMAND(dump, RunDump);
    HANDLE_COMMAND(info, RunInfo);
    HANDLE_COMMAND(indexes, RunIndexes);
    HANDLE_COMMAND(list, RunList);
    HANDLE_COMMAND(pricings, RunPricings);
    HANDLE_COMMAND(summarize, RunSummarize);

#undef HANDLE_COMMAND

    PrintLn(stderr, "Unknown command '%1'", cmd);
    PrintLn(stderr, "%1", MainUsageText);
    return 1;
}
