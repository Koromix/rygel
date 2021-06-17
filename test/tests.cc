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

#include "../src/core/libcc/libcc.hh"
#include "tests.hh"

namespace RG {

static HeapArray<const TestInfo *> tests;
static HeapArray<const BenchmarkInfo *> benchmarks;

TestInfo::TestInfo(const char *path, const char *category, const char *label, void (*func)(Size *out_total, Size *out_failures))
    : path(path), category(category), label(label), func(func)
{
    tests.Append(this);
}

BenchmarkInfo::BenchmarkInfo(const char *path, const char *category, const char *label, void (*func)())
    : path(path), category(category), label(label), func(func)
{
    benchmarks.Append(this);
}

int RunTest(int argc, char **argv)
{
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
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        pattern = opt.ConsumeNonOption();
    }

    // We want to group the output, make sure everything is sorted correctly
    std::sort(tests.begin(), tests.end(), [](const TestInfo *test1, const TestInfo *test2) {
        return CmpStr(test1->path, test2->path) < 0;
    });
    std::sort(benchmarks.begin(), benchmarks.end(), [](const BenchmarkInfo *bench1, const BenchmarkInfo *bench2) {
        return CmpStr(bench1->path, bench2->path) < 0;
    });

    for (Size i = 0; i < tests.len; i++) {
        const TestInfo &test = *tests[i];

        if (!pattern || MatchPathSpec(test.path, pattern)) {
            if (!i || !TestStr(test.category, tests[i - 1]->category)) {
                PrintLn(stderr, "%1Tests: %!y..%2%!0", i ? "\n" : "", test.category);
            }
            Print(stderr, "  %!..+%1%!0", FmtArg(test.label).Pad(32));

            Size total = 0;
            Size failures = 0;
            test.func(&total, &failures);

            if (failures) {
                PrintLn(stderr, "\n    %!R..Failed%!0 (%1/%2)", failures, total);
            } else {
                PrintLn(stderr, " %!G..Success%!0 (%1)", total);
            }
        }
    }

    for (Size i = 0; i < benchmarks.len; i++) {
        const BenchmarkInfo &bench = *benchmarks[i];

        if (!pattern || MatchPathSpec(bench.path, pattern)) {
            if (!i || !TestStr(bench.category, tests[i - 1]->category)) {
                PrintLn(stderr, "\nBenchmarks: %!y..%1%!0", bench.category);
            }
            PrintLn(stderr, "  %!..+%1%!0", FmtArg(bench.label).Pad(20));

            bench.func();
        }
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunTest(argc, argv); }
