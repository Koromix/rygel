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
