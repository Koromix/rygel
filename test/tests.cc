// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../src/core/libcc/libcc.hh"
#include "tests.hh"

namespace RG {

void TestFormatDouble();
void TestMatchPathName();
void TestOptionParser();

void BenchFmt();
void BenchMatchPathName();

int RunTest(int argc, char **argv)
{
    bool test = false;
    bool bench = false;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %1 [options]

Options:
        --test                   Run tests
        --bench                  Run benchmarks)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("--test")) {
                test = true;
            } else if (opt.Test("--bench")) {
                bench = true;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    if (!test && !bench) {
        LogError("You must specify --test or --bench");
        return 1;
    }

    if (test) {
        PrintLn(stderr, "Testing libcc");
        TestFormatDouble();
        TestMatchPathName();
        TestOptionParser();

        PrintLn();
    }
    if (bench) {
        PrintLn(stderr, "Benchmarking libcc");
        BenchFmt();
        BenchMatchPathName();

        PrintLn();
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunTest(argc, argv); }
