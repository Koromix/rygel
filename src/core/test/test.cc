// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/base/base.hh"
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
            Print("%!y..%1%!0", FmtArg(test.path).Pad(36));

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

#if defined(RG_DEBUG)
    if (!pattern) {
        LogInfo("Benchmarks are disabled by default in debug builds");
    }
#endif

    // Run benchmarks
    for (Size i = 0; i < benchmarks.len; i++) {
        const BenchmarkInfo &bench = *benchmarks[i];

#if defined(RG_DEBUG)
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
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
