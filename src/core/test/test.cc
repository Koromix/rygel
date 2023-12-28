// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "test.hh"

namespace RG {

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
    RG_CRITICAL(argc >= 1, "First argument is missing");

    // Options
    const char *pattern = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %1 [pattern])", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
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
            Print(stderr, "Test: %!y..%1%!0", FmtArg(test.path).Pad(28));

            Size total = 0;
            Size failures = 0;
            test.func(&total, &failures);

            if (failures) {
                PrintLn(stderr, "\n    %!R..Failed%!0 (%1/%2)\n", failures, total);
            } else {
                PrintLn(stderr, " %!G..Success%!0 (%1)", total);
            }

            matches++;
        }
    }
    if (matches) {
        PrintLn(stderr);
    }

#ifdef RG_DEBUG
    if (!pattern) {
        LogInfo("Benchmarks are disabled by default in debug builds");
    }
#endif

    // Run benchmarks
    for (Size i = 0; i < benchmarks.len; i++) {
        const BenchmarkInfo &bench = *benchmarks[i];

#ifdef RG_DEBUG
        bool enable = pattern && MatchPathSpec(bench.path, pattern);
#else
        bool enable = !pattern || MatchPathSpec(bench.path, pattern);
#endif

        if (enable) {
            PrintLn(stderr, "Benchmark: %!y..%1%!0", bench.path);
            bench.func();
            PrintLn(stderr);

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
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
