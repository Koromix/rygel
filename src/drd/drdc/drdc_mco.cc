// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "drdc.hh"
#include "config.hh"

namespace RG {

enum class TestFlag {
    ClusterLen = 1 << 0,
    Ghm = 1 << 1,
    MainError = 1 << 2,
    Ghs = 1 << 3,
    Supplements = 1 << 4,
    ExbExh = 1 << 5
};
static const OptionDesc TestFlagOptions[] = {
    {"ClusterLen",  "Test cluster length"},
    {"GHM",         "Test GHM"},
    {"MainError",   "Test main error"},
    {"GHS",         "Test GHS"},
    {"Supplements", "Test supplements"},
    {"ExbExh",      "Test EXB/EXH counts"}
};

static void PrintSummary(const mco_Pricing &summary)
{
    PrintLn("  Results: %1", summary.results_count);
    PrintLn("  Stays: %1", summary.stays_count);
    PrintLn("  Failures: %1", summary.failures_count);
    PrintLn();
    PrintLn("  GHS-EXB+EXH: %1 €", FmtDouble((double)summary.price_cents / 100.0, 2));
    PrintLn("    GHS: %1 €", FmtDouble((double)summary.ghs_cents / 100.0, 2));
    PrintLn("  Supplements: %1 €",
            FmtDouble((double)(summary.total_cents - summary.price_cents) / 100.0, 2));
    for (Size i = 0; i < RG_LEN(mco_SupplementTypeNames); i++) {
        PrintLn("    %1: %2 € [%3]",
                mco_SupplementTypeNames[i],
                FmtDouble((double)summary.supplement_cents.values[i] / 100.0, 2),
                summary.supplement_days.values[i]);
    }
    PrintLn("  Total: %1 €", FmtDouble((double)summary.total_cents / 100.0, 2));
    PrintLn();
};

static void ExportResults(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                          Span<const mco_Pricing> pricings, Span<const mco_Pricing> mono_pricings,
                          int verbosity)
{
    const auto export_verbose_info = [&](const char *padding,
                                         const mco_Result &result, const mco_Pricing &pricing) {
        PrintLn("%1GHS-EXB+EXH: %2 € [%3, coefficient = %4]",
                padding, FmtDouble((double)pricing.price_cents / 100.0, 2),
                pricing.exb_exh, FmtDouble(pricing.ghs_coefficient, 4));
        if (pricing.price_cents != pricing.ghs_cents) {
            PrintLn("%1  GHS: %2 €",
                    padding, FmtDouble((double)pricing.ghs_cents / 100.0, 2));
        }
        if (pricing.total_cents > pricing.price_cents) {
            PrintLn("%1Supplements: %2 €", padding,
                    FmtDouble((double)(pricing.total_cents - pricing.price_cents) / 100.0, 2));
            for (Size j = 0; j < RG_LEN(mco_SupplementTypeNames); j++) {
                if (pricing.supplement_cents.values[j]) {
                    PrintLn("%1  %2: %3 € [%4]", padding, mco_SupplementTypeNames[j],
                            FmtDouble((double)pricing.supplement_cents.values[j] / 100.0, 2),
                            result.supplement_days.values[j]);
                }
            }
        }
        PrintLn("%1Total: %2 €",
                padding, FmtDouble((double)pricing.total_cents / 100.0, 2));
    };

    Size j = 0;
    for (Size i = 0; i < results.len; i++) {
        const mco_Result &result = results[i];
        const mco_Pricing &pricing = pricings[i];

        PrintLn("  %1 %2 [%3 -- %4] = GHM %5 [%6] / GHS %7",
                result.stays[0].admin_id, result.stays[0].bill_id,
                result.duration, result.stays[result.stays.len - 1].exit.date,
                result.ghm, result.main_error, result.ghs);
        if (verbosity) {
            export_verbose_info("    ", result, pricing);
        }

        if (mono_results.len && result.stays.len > 1) {
            if (verbosity) {
                PrintLn("    Individual results:");
            }
            for (Size k = 0; k < result.stays.len; k++) {
                const mco_Result &mono_result = mono_results[j + k];
                const mco_Pricing &mono_pricing = mono_pricings[j + k];
                RG_ASSERT_DEBUG(mono_result.stays[0].bill_id == result.stays[0].bill_id);

                PrintLn("    %1%2 [%3 -- %4] = GHM %5 [%6] / GHS %7",
                        verbosity ? "  " : "", FmtArg(k).Pad0(-2),
                        mono_result.duration, mono_result.stays[0].exit.date,
                        mono_result.ghm, mono_result.main_error, mono_result.ghs);
                if (verbosity >= 2) {
                    export_verbose_info("        ", mono_result, mono_pricing);
                }
            }
            j += result.stays.len;
        } else {
            j++;
        }
    }
    PrintLn();
}

static void ExportTests(Span<const mco_Result> results, Span<const mco_Pricing> pricings,
                        Span<const mco_Result> mono_results, const HashTable<int32_t, mco_Test> &tests,
                        unsigned int flags, bool verbose)
{
    Size tested_clusters = 0, failed_clusters = 0;
    Size tested_ghm = 0, failed_ghm = 0;
    Size tested_main_errors = 0, failed_main_errors = 0;
    Size tested_ghs = 0, failed_ghs = 0;
    Size tested_supplements = 0, failed_supplements = 0;
    Size tested_auth_supplements = 0, failed_auth_supplements = 0;
    Size tested_exb_exh = 0, failed_exb_exh = 0;

    Size j = 0;
    for (Size i = 0; i < results.len; i++) {
        const mco_Result &result = results[i];
        const mco_Pricing &pricing = pricings[i];

        Span<const mco_Result> sub_mono_results = {};
        if (mono_results.len) {
            sub_mono_results = mono_results.Take(j, result.stays.len);
            j += result.stays.len;
        }

        const mco_Test *test = tests.Find(result.stays[0].bill_id);
        if (!test)
            continue;

        if ((flags & (int)TestFlag::ClusterLen) && test->cluster_len) {
            tested_clusters++;
            if (result.stays.len != test->cluster_len) {
                failed_clusters++;
                if (verbose) {
                    PrintLn("    %1 [%2] has inadequate cluster %3 != %4",
                            test->bill_id, result.stays[result.stays.len - 1].exit.date,
                            result.stays.len, test->cluster_len);
                }
            }
        }

        if ((flags & (int)TestFlag::Ghm) && test->ghm.IsValid()) {
            tested_ghm++;
            if (test->ghm != result.ghm) {
                failed_ghm++;
                if (verbose) {
                    PrintLn("    %1 [%2] has inadequate GHM %3 != %4",
                            test->bill_id, result.stays[result.stays.len - 1].exit.date,
                            result.ghm, test->ghm);
                }
            }
        }

        if ((flags & (int)TestFlag::MainError) && test->ghm.IsValid()) {
            tested_main_errors++;
            if (test->error != result.main_error) {
                failed_main_errors++;
                if (verbose) {
                    PrintLn("    %1 [%2] has inadequate main error %3 != %4",
                            test->bill_id, result.stays[result.stays.len - 1].exit.date,
                            result.main_error, test->error);
                }
            }
        }

        if ((flags & (int)TestFlag::Ghs) && test->ghs.IsValid()) {
            tested_ghs++;
            if (test->ghs != result.ghs) {
                failed_ghs++;
                if (verbose) {
                    PrintLn("    %1 [%2] has inadequate GHS %3 != %4",
                            test->bill_id, result.stays[result.stays.len - 1].exit.date,
                            result.ghs, test->ghs);
                }
            }
        }

        if ((flags & (int)TestFlag::Supplements) && test->ghs.IsValid()) {
            tested_supplements++;
            if (test->supplement_days != result.supplement_days) {
                failed_supplements++;
                if (verbose) {
                    for (Size i = 0; i < RG_LEN(mco_SupplementTypeNames); i++) {
                        if (test->supplement_days.values[i] != result.supplement_days.values[i]) {
                            PrintLn("    %1 [%2] has inadequate %3 %4 != %5",
                                    test->bill_id, result.stays[result.stays.len - 1].exit.date,
                                    mco_SupplementTypeNames[i], result.supplement_days.values[i],
                                    test->supplement_days.values[i]);
                        }
                    }
                }
            }
        }

        if ((flags & (int)TestFlag::Supplements) && test->ghs.IsValid() && mono_results.len) {
            tested_auth_supplements += sub_mono_results.len;

            Size max_auth_tests = sub_mono_results.len;
            if (max_auth_tests > RG_LEN(mco_Test::auth_supplements)) {
                LogError("Testing only first %1 unit authorizations for stay %2",
                         RG_LEN(mco_Test::auth_supplements), result.stays[0].bill_id);
                max_auth_tests = RG_LEN(mco_Test::auth_supplements);
            }

            for (Size i = 0; i < max_auth_tests; i++) {
                const mco_Result &mono_result = sub_mono_results[i];

                int8_t type;
                int16_t days;
                if (mono_result.supplement_days.st.rea) {
                    type = (int)mco_SupplementType::Rea;
                } else if (mono_result.supplement_days.st.reasi) {
                    type = (int)mco_SupplementType::Reasi;
                } else if (mono_result.supplement_days.st.si) {
                    type = (int)mco_SupplementType::Si;
                } else if (mono_result.supplement_days.st.src) {
                    type = (int)mco_SupplementType::Src;
                } else if (mono_result.supplement_days.st.nn1) {
                    type = (int)mco_SupplementType::Nn1;
                } else if (mono_result.supplement_days.st.nn2) {
                    type = (int)mco_SupplementType::Nn2;
                } else if (mono_result.supplement_days.st.nn3) {
                    type = (int)mco_SupplementType::Nn3;
                } else if (mono_result.supplement_days.st.rep) {
                    type = (int)mco_SupplementType::Rep;
                } else {
                    type = 0;
                }
                days = mono_result.supplement_days.values[type];

                if (type != test->auth_supplements[i].type ||
                        days != test->auth_supplements[i].days) {
                    failed_auth_supplements++;
                    if (verbose) {
                        PrintLn("    %1/%2 has inadequate %3 %4 != %5 %6",
                                test->bill_id, i, mco_SupplementTypeNames[type], days,
                                mco_SupplementTypeNames[test->auth_supplements[i].type],
                                test->auth_supplements[i].days);
                    }
                }
            }
        }

        if ((flags & (int)TestFlag::ExbExh) && test->ghs.IsValid()) {
            tested_exb_exh++;
            if (test->exb_exh != pricing.exb_exh) {
                failed_exb_exh++;
                if (verbose) {
                    PrintLn("    %1 [%2] has inadequate EXB/EXH %3 != %4",
                            test->bill_id, result.stays[result.stays.len - 1].exit.date,
                            pricing.exb_exh, test->exb_exh);
                }
            }
        }
    }
    if (verbose && (failed_clusters || failed_ghm || failed_main_errors || failed_ghs ||
                    failed_supplements || failed_auth_supplements || failed_exb_exh)) {
        PrintLn();
    }

    if (flags & (int)TestFlag::ClusterLen) {
        PrintLn("    Failed cluster tests: %1 / %2 (missing %3)",
                failed_clusters, tested_clusters, results.len - tested_clusters);
    }
    if (flags & (int)TestFlag::Ghm) {
    PrintLn("    Failed GHM tests: %1 / %2 (missing %3)",
            failed_ghm, tested_ghm, results.len - tested_ghm);
    }
    if (flags & (int)TestFlag::MainError) {
    PrintLn("    Failed main errors tests: %1 / %2 (missing %3)",
            failed_main_errors, tested_main_errors, results.len - tested_main_errors);
    }
    if (flags & (int)TestFlag::Ghs) {
    PrintLn("    Failed GHS tests: %1 / %2 (missing %3)",
            failed_ghs, tested_ghs, results.len - tested_ghs);
    }
    if (flags & (int)TestFlag::Supplements) {
        PrintLn("    Failed supplements tests: %1 / %2 (missing %3)",
                failed_supplements, tested_supplements, results.len - tested_supplements);
        if (mono_results.len) {
            PrintLn("    Failed auth supplements tests: %1 / %2 (missing %3)",
                    failed_auth_supplements, tested_auth_supplements,
                    mono_results.len - tested_auth_supplements);
        } else {
            PrintLn("    Auth supplements tests not performed, needs --mono");
        }
    }
    if (flags & (int)TestFlag::ExbExh) {
        PrintLn("    Failed EXB/EXH tests: %1 / %2 (missing %3)",
                failed_exb_exh, tested_exb_exh, results.len - tested_exb_exh);
    }
    PrintLn();
}

int RunMcoClassify(Span<const char *> arguments)
{
    // Options
    unsigned int classifier_flags = 0;
    int dispense_mode = -1;
    bool apply_coefficient = false;
    const char *filter = nullptr;
    const char *filter_path = nullptr;
    int verbosity = 0;
    unsigned int test_flags = 0;
    int torture = 0;
    HeapArray<const char *> filenames;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: drdc mco_classify [options] stay_file ...
)");
        PrintLn(fp, CommonOptions);
        PrintLn(fp, R"(
Classify options:
    -o, --option <options>       Classifier options (see below)
    -d, --dispense <mode>        Run dispensation algorithm (see below)
        --coeff                  Apply GHS coefficients

    -f, --filter <expr>          Run Wren filter
    -F, --filter_file <file>     Run Wren filter in file

    -v, --verbose                Show more classification details (cumulative)

        --test [options]         Enable testing against GenRSA values (see below)
        --torture [N]            Benchmark classifier with N runs

Classifier options:)");
        for (const OptionDesc &desc: mco_ClassifyFlagOptions) {
            PrintLn(fp, "    %1  %2", FmtArg(desc.name).Pad(27), desc.help);
        }
        PrintLn(fp, R"(
Dispensation modes:)");
        for (const OptionDesc &desc: mco_DispenseModeOptions) {
            PrintLn(fp, "    %1  Algorithm %2", FmtArg(desc.name).Pad(27), desc.help);
        }
        PrintLn(fp, R"(
Test options:)");
        for (const OptionDesc &desc: TestFlagOptions) {
            PrintLn(fp, "    %1  %2", FmtArg(desc.name).Pad(27), desc.help);
        }
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-o", "--option", OptionType::Value)) {
                const char *flags_str = opt.current_value;

                while (flags_str[0]) {
                    Span<const char> flag = TrimStr(SplitStr(flags_str, ',', &flags_str), " ");
                    const OptionDesc *desc = FindIf(mco_ClassifyFlagOptions,
                                                    [&](const OptionDesc &desc) { return TestStr(desc.name, flag); });
                    if (!desc) {
                        LogError("Unknown classifier flag '%1'", flag);
                        return 1;
                    }
                    classifier_flags |= 1u << (desc - mco_ClassifyFlagOptions);
                }
            } else if (opt.Test("-d", "--dispense", OptionType::Value)) {
                const char *mode_str = opt.current_value;

                const OptionDesc *desc = FindIf(mco_DispenseModeOptions,
                                                [&](const OptionDesc &desc) { return TestStr(desc.name, mode_str); });
                if (!desc) {
                    LogError("Unknown dispensation mode '%1'", mode_str);
                    return 1;
                }
                dispense_mode = (int)(desc - mco_DispenseModeOptions);
            } else if (opt.Test("--coeff")) {
                apply_coefficient = true;
            } else if (opt.Test("-f", "--filter", OptionType::Value)) {
                filter = opt.current_value;
            } else if (opt.Test("-F", "--filter_file", OptionType::Value)) {
                filter_path = opt.current_value;
            } else if (opt.Test("-v", "--verbose")) {
                verbosity++;
            } else if (opt.Test("--test", OptionType::OptionalValue)) {
                const char *flags_str = opt.current_value;

                if (flags_str) {
                    while (flags_str[0]) {
                        Span<const char> flag = TrimStr(SplitStr(flags_str, ',', &flags_str), " ");
                        const OptionDesc *desc = FindIf(TestFlagOptions,
                                                        [&](const OptionDesc &desc) { return TestStr(desc.name, flag); });
                        if (!desc) {
                            LogError("Unknown test flag '%1'", flag);
                            return 1;
                        }
                        test_flags |= 1u << (desc - TestFlagOptions);
                    }
                } else {
                    test_flags = UINT_MAX;
                }
            } else if (opt.Test("--torture", OptionType::Value)) {
                if (!ParseDec(opt.current_value, &torture))
                    return 1;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.ConsumeNonOptions(&filenames);
        if (!filenames.len) {
            LogError("No filename provided");
            return 1;
        }
    }

    LogInfo("Load tables");
    mco_TableSet table_set;
    if (!mco_LoadTableSet(drdc_config.table_directories, {}, &table_set) || !table_set.indexes.len)
        return 1;

    LogInfo("Load authorizations");
    mco_AuthorizationSet authorization_set;
    if (!mco_LoadAuthorizationSet(drdc_config.profile_directory, drdc_config.mco_authorization_filename,
                                  &authorization_set))
        return 1;

    HeapArray<char> filter_buf;
    if (filter) {
        filter_buf.Append(filter);
        filter_buf.Append(0);
    } else if (filter_path) {
        if (ReadFile(filter_path, Megabytes(1), &filter_buf) < 0)
            return 1;
        filter_buf.Append(0);
    }

    mco_StaySet stay_set;
    HashTable<int32_t, mco_Test> tests;
    {
        mco_StaySetBuilder stay_set_builder;
        for (const char *filename: filenames) {
            LogInfo("Load '%1'", filename);
            if (!stay_set_builder.LoadFiles(filename, test_flags ? &tests : nullptr))
                return 1;
        }
        if (!stay_set_builder.Finish(&stay_set))
            return 1;
    }

    // Performance counter
    int64_t *perf_counter = nullptr;
    int64_t perf_start;
    const auto switch_perf_counter = [&](int64_t *counter) {
        int64_t now = GetMonotonicTime();

        if (perf_counter) {
            *perf_counter += now - perf_start;
        }
        perf_start = now;
        perf_counter = counter;
    };

    LogInfo("Classify");
    HeapArray<mco_Result> results;
    HeapArray<mco_Result> mono_results;
    mco_StaySet filter_stay_set;
    HeapArray<mco_Pricing> pricings;
    HeapArray<mco_Pricing> mono_pricings;
    mco_Pricing summary = {};
    int64_t classify_time = 0;
    int64_t filter_time = 0;
    int64_t pricing_time = 0;
    for (int j = 0; j < std::max(torture, 1); j++) {
        results.RemoveFrom(0);
        mono_results.RemoveFrom(0);
        filter_stay_set.stays.RemoveFrom(0);
        filter_stay_set.array_alloc.ReleaseAll();
        pricings.RemoveFrom(0);
        mono_pricings.RemoveFrom(0);
        summary = {};

        switch_perf_counter(&classify_time);
        mco_Classify(table_set, authorization_set, drdc_config.sector, stay_set.stays,
                     classifier_flags, &results, dispense_mode >= 0 ? &mono_results : nullptr);

        if (filter_buf.len) {
            switch_perf_counter(&filter_time);

            HeapArray<const mco_Result *> filter_results;
            HeapArray<const mco_Result *> filter_mono_results;
            if (!mco_Filter(filter_buf.ptr, results, mono_results, &filter_results,
                            dispense_mode >= 0 ? &filter_mono_results : nullptr, &filter_stay_set))
                return 1;

            for (Size k = 0; k < filter_results.len; k++) {
                results[k] = *filter_results[k];
            }
            for (Size k = 0; k < filter_mono_results.len; k++) {
                mono_results[k] = *filter_mono_results[k];
            }
            results.RemoveFrom(filter_results.len);
            mono_results.RemoveFrom(filter_mono_results.len);

            mco_Classify(table_set, authorization_set, drdc_config.sector, filter_stay_set.stays,
                         classifier_flags, &results, dispense_mode >= 0 ? &mono_results : nullptr);
        }

        switch_perf_counter(&pricing_time);
        if (verbosity || test_flags) {
            mco_Price(results, apply_coefficient, &pricings);

            if (dispense_mode >= 0) {
                mco_Dispense(pricings, mono_results, (mco_DispenseMode)dispense_mode,
                             &mono_pricings);
            }
            mco_Summarize(pricings, &summary);
        } else {
            mco_PriceTotal(results, apply_coefficient, &summary);
        }
    }
    switch_perf_counter(nullptr);

    LogInfo("Export");
    if (verbosity - !!test_flags >= 1) {
        PrintLn("Results:");
        ExportResults(results, mono_results, pricings, mono_pricings, verbosity - !!test_flags - 1);
    }
    PrintLn("Summary:");
    PrintSummary(summary);
    if (test_flags) {
        PrintLn("Tests:");
        ExportTests(results, pricings, mono_results, tests, test_flags, verbosity >= 1);
    }

    PrintLn("GHS coefficients have%1 been applied!", apply_coefficient ? "" : " NOT");

    if (torture) {
        int64_t total_time = classify_time + filter_time + pricing_time;
        int64_t perf = (int64_t)summary.results_count * torture * 1000 / total_time;
        int64_t mono_perf = (int64_t)summary.stays_count * torture * 1000 / total_time;

        PrintLn();
        PrintLn("Performance (with %1 runs):", torture);
        PrintLn("  Results: %1/sec (%2 μs/result)", perf, 1000000.0 / perf);
        PrintLn("  Stays: %1/secc (%2 μs/stay)", mono_perf, 1000000.0 / mono_perf);
        PrintLn();
        PrintLn("  Time: %1 sec/run",
                FmtDouble((double)((classify_time + pricing_time) / torture) / 1000.0, 3));
        PrintLn("  Classify: %1 sec/run (%2%%)",
                FmtDouble((double)(classify_time / torture) / 1000.0, 3),
                FmtDouble(100.0 * classify_time / total_time, 2));
        if (filter_buf.len) {
            PrintLn("  Filter: %1 sec/run (%2%%)",
                    FmtDouble((double)(filter_time / torture) / 1000.0, 3),
                    FmtDouble(100.0 * filter_time / total_time, 2));
        }
        PrintLn("  Pricing: %1 sec/run (%2%%)",
                FmtDouble((double)(pricing_time / torture) / 1000.0, 3),
                FmtDouble(100.0 * pricing_time / total_time, 2));
    }

    return 0;
}

int RunMcoDump(Span<const char *> arguments)
{
    // Options
    bool dump = false;
    HeapArray<const char *> filenames;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: drdc mco_dump [options] [filename] ...
)");
        PrintLn(fp, CommonOptions);
        PrintLn(fp, R"(
Dump options:
    -d, --dump                   Dump content of (readable) tables)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-d", "--dump")) {
                dump = true;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.ConsumeNonOptions(&filenames);
    }

    mco_TableSet table_set;
    if (!mco_LoadTableSet(drdc_config.table_directories, filenames, &table_set) ||
            !table_set.indexes.len)
        return 1;
    mco_DumpTableSetHeaders(table_set, &stdout_st);
    if (dump) {
        mco_DumpTableSetContent(table_set, &stdout_st);
    }

    return 0;
}

int RunMcoList(Span<const char *> arguments)
{
    // Options
    Date index_date = {};
    HeapArray<const char *> spec_strings;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: drdc mco_list [options] list_name ...
)");
        PrintLn(fp, CommonOptions);
        PrintLn(fp, R"(
List options:
    -d, --date <date>            Use tables valid on specified date
                                 (default: most recent tables))");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-d", "--date", OptionType::Value)) {
                index_date = Date::FromString(opt.current_value);
                if (!index_date.value)
                    return 1;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.ConsumeNonOptions(&spec_strings);
        if (!spec_strings.len) {
            LogError("No specifier string provided");
            return 1;
        }
    }

    mco_TableSet table_set;
    const mco_TableIndex *index;
    {
        if (!mco_LoadTableSet(drdc_config.table_directories, {}, &table_set))
            return 1;
        index = table_set.FindIndex(index_date);
        if (!index) {
            LogError("No table index available at '%1'", index_date);
            return 1;
        }
    }

    for (const char *spec_str: spec_strings) {
        mco_ListSpecifier spec = mco_ListSpecifier::FromString(spec_str);
        if (!spec.IsValid())
            continue;

        PrintLn("%1:", spec_str);
        switch (spec.table) {
            case mco_ListSpecifier::Table::Invalid: { /* Handled above */ } break;

            case mco_ListSpecifier::Table::Diagnoses: {
                for (const mco_DiagnosisInfo &diag: index->diagnoses) {
                    if (diag.flags & (int)mco_DiagnosisInfo::Flag::SexDifference) {
                        if (spec.Match(diag.Attributes(1).raw)) {
                            PrintLn("  %1 (male)", diag.diag);
                        }
                        if (spec.Match(diag.Attributes(2).raw)) {
                            PrintLn("  %1 (female)", diag.diag);
                        }
                    } else {
                        if (spec.Match(diag.Attributes(1).raw)) {
                            PrintLn("  %1", diag.diag);
                        }
                    }
                }
            } break;

            case mco_ListSpecifier::Table::Procedures: {
                for (const mco_ProcedureInfo &proc: index->procedures) {
                    if (spec.Match(proc.bytes)) {
                        PrintLn("  %1", proc.proc);
                    }
                }
            } break;
        }
        PrintLn();
    }

    return 0;
}

int RunMcoMap(Span<const char *> arguments)
{
    // Options
    Date index_date = {};

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: drdc mco_map [options]
)");
        PrintLn(fp, CommonOptions);
        PrintLn(fp, R"(
Map options:
    -d, --date <date>            Use tables valid on specified date
                                 (default: most recent tables))");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-d", "--date", OptionType::Value)) {
                index_date = Date::FromString(opt.current_value);
                if (!index_date.value)
                    return 1;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }
    }

    mco_TableSet table_set;
    const mco_TableIndex *index;
    {
        if (!mco_LoadTableSet(drdc_config.table_directories, {}, &table_set))
            return 1;
        index = table_set.FindIndex(index_date);
        if (!index) {
            LogError("No table index available at '%1'", index_date);
            return 1;
        }
    }

    LogInfo("Computing");
    HashTable<mco_GhmCode, mco_GhmConstraint> ghm_constraints;
    if (!mco_ComputeGhmConstraints(*index, &ghm_constraints))
        return 1;

    LogInfo("Export");
    for (const mco_GhmToGhsInfo &ghm_to_ghs_info: index->ghs)  {
        const mco_GhmConstraint *constraint = ghm_constraints.Find(ghm_to_ghs_info.ghm);
        if (constraint) {
            PrintLn("Constraint for %1", ghm_to_ghs_info.ghm);
            PrintLn("  Duration = 0x%1",
                    FmtHex(constraint->durations).Pad0(-2 * RG_SIZE(constraint->durations)));
            PrintLn("  Warnings = 0x%1",
                    FmtHex(constraint->warnings).Pad0(-2 * RG_SIZE(constraint->warnings)));
        } else {
            PrintLn("%1 unreached!", ghm_to_ghs_info.ghm);
        }
    }

    return 0;
}

int RunMcoPack(Span<const char *> arguments)
{
    // Options
    const char *dest_filename = nullptr;
    HeapArray<const char *> filenames;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: drdc mco_pack [options] stay_file ... -O output_file
)");
        PrintLn(fp, CommonOptions);
        PrintLn(fp, R"(
Pack options:
    -O, --output_file <file>     Set output file)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                dest_filename = opt.current_value;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.ConsumeNonOptions(&filenames);
        if (!dest_filename) {
            LogError("A destination file must be provided (--output)");
            return 1;
        }
        if (!filenames.len) {
            LogError("No stay file provided");
            return 1;
        }
    }

    LogInfo("Load stays");
    mco_StaySet stay_set;
    {
        mco_StaySetBuilder stay_set_builder;

        if (!stay_set_builder.LoadFiles(filenames))
            return 1;
        if (!stay_set_builder.Finish(&stay_set))
            return 1;
    }

    LogInfo("Pack stays");
    if (!stay_set.SavePack(dest_filename))
        return 1;

    return 0;
}

int RunMcoShow(Span<const char *> arguments)
{
    // Options
    Date index_date = {};
    HeapArray<const char *> names;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: drdc mco_show [options] name ...
)");
        PrintLn(fp, CommonOptions);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-d", "--date", OptionType::Value)) {
                index_date = Date::FromString(opt.current_value);
                if (!index_date.value)
                    return 1;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.ConsumeNonOptions(&names);
        if (!names.len) {
            LogError("No element name provided");
            return 1;
        }
    }

    mco_TableSet table_set;
    const mco_TableIndex *index;
    {
        if (!mco_LoadTableSet(drdc_config.table_directories, {}, &table_set))
            return 1;
        index = table_set.FindIndex(index_date);
        if (!index) {
            LogError("No table index available at '%1'", index_date);
            return 1;
        }
    }

    for (const char *name: names) {
        // Diagnosis?
        {
            drd_DiagnosisCode diag =
                drd_DiagnosisCode::FromString(name, RG_DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log);
            if (diag.IsValid()) {
                const mco_DiagnosisInfo *diag_info = index->FindDiagnosis(diag);
                if (diag_info) {
                    mco_DumpDiagnosisTable(*diag_info, index->exclusions, &stdout_st);
                    continue;
                }
            }
        }

        // Procedure?
        {
            drd_ProcedureCode proc =
                drd_ProcedureCode::FromString(name, RG_DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log);
            if (proc.IsValid()) {
                Span<const mco_ProcedureInfo> proc_info = index->FindProcedure(proc);
                if (proc_info.len) {
                    mco_DumpProcedureTable(proc_info, &stdout_st);
                    continue;
                }
            }
        }

        // GHM root?
        {
            mco_GhmRootCode ghm_root =
                mco_GhmRootCode::FromString(name, RG_DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log);
            if (ghm_root.IsValid()) {
                const mco_GhmRootInfo *ghm_root_info = index->FindGhmRoot(ghm_root);
                if (ghm_root_info) {
                    mco_DumpGhmRootTable(*ghm_root_info, &stdout_st);
                    PrintLn();

                    Span<const mco_GhmToGhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root);
                    mco_DumpGhmToGhsTable(compatible_ghs, &stdout_st);

                    continue;
                }
            }
        }

        // GHS?
        {
            mco_GhsCode ghs = mco_GhsCode::FromString(name, RG_DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log);
            if (ghs.IsValid()) {
                const mco_GhsPriceInfo *pub_price_info = index->FindGhsPrice(ghs, drd_Sector::Public);
                const mco_GhsPriceInfo *priv_price_info = index->FindGhsPrice(ghs, drd_Sector::Private);
                if (pub_price_info || priv_price_info) {
                    for (const mco_GhmToGhsInfo &ghm_to_ghs_info: index->ghs) {
                        if (ghm_to_ghs_info.Ghs(drd_Sector::Public) == ghs ||
                                ghm_to_ghs_info.Ghs(drd_Sector::Private) == ghs) {
                            mco_DumpGhmToGhsTable(ghm_to_ghs_info, &stdout_st);
                        }
                    }
                    PrintLn();

                    if (pub_price_info) {
                        PrintLn("      Public:");
                        mco_DumpGhsPriceTable(*pub_price_info, &stdout_st);
                    }
                    if (priv_price_info) {
                        PrintLn("      Private:");
                        mco_DumpGhsPriceTable(*priv_price_info, &stdout_st);
                    }

                    continue;
                }
            }
        }

        LogError("Unknown element '%1'", name);
    }

    return 0;
}

}
