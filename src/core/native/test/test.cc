// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
#include "test.hh"

namespace K {

static HeapArray<const TestInfo *> tests;
static HeapArray<const BenchmarkInfo *> benchmarks;

TestInfo::TestInfo(const char *path, void (*func)(Size *out_total, Size *out_failures))
    : path(path), func(func)
{
    tests.Append(this);
}

BenchmarkInfo::BenchmarkInfo(const char *path, void (*func)())
    : path(path), func(func)
{
    benchmarks.Append(this);
}

int Main(int argc, char **argv)
{
    // Options
    const char *pattern = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st, R"(Usage: %1 [pattern])", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        pattern = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    // We want to group the output, make sure everything is sorted correctly
    std::sort(tests.begin(), tests.end(), [](const TestInfo *test1, const TestInfo *test2) {
        return CmpStr(test1->path, test2->path) < 0;
    });
    std::sort(benchmarks.begin(), benchmarks.end(), [](const BenchmarkInfo *bench1, const BenchmarkInfo *bench2) {
        return CmpStr(bench1->path, bench2->path) < 0;
    });

    Size matches = 0;

    // Run tests
    for (Size i = 0; i < tests.len; i++) {
        const TestInfo &test = *tests[i];

        if (!pattern || MatchPathSpec(test.path, pattern)) {
            Print("%!y..%1%!0", FmtPad(test.path, 36));
            StdOut->Flush();

            Size total = 0;
            Size failures = 0;
            test.func(&total, &failures);

            if (failures) {
                PrintLn("\n    %!R..Failed%!0 (%1/%2)\n", failures, total);
            } else {
                PrintLn(" %!G..Success%!0 (%1)", total);
            }

            matches++;
        }
    }
    if (matches) {
        PrintLn();
    }

#if defined(K_DEBUG)
    if (!pattern) {
        LogInfo("Benchmarks are disabled by default in debug builds");
    }
#endif

    // Run benchmarks
    for (Size i = 0; i < benchmarks.len; i++) {
        const BenchmarkInfo &bench = *benchmarks[i];

#if defined(K_DEBUG)
        bool enable = pattern && MatchPathSpec(bench.path, pattern);
#else
        bool enable = !pattern || MatchPathSpec(bench.path, pattern);
#endif

        if (enable) {
            PrintLn("%!m..%1%!0", bench.path);
            bench.func();
            PrintLn();

            matches++;
        }
    }

    if (pattern && !matches) {
        LogError("Pattern '%1' does not match any test", pattern);
        return 1;
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
