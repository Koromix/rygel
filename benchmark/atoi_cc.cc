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

#include "../vendor/libcc/libcc.hh"

namespace RG {

static const char * strings[] = {
    "424242",
    "foobar",
    "123456789",
};

volatile uint64_t sum = 0;

int Main(int argc, char **argv)
{
    if (argc < 2) {
        LogError("Missing number of iterations");
        LogInfo("Usage: atoi_cc <iterations>");
        return 1;
    }

    int iterations = 0;
    if (!ParseInt(argv[1], &iterations))
        return 1;
    LogInfo("Iterations: %1", iterations);

    int64_t start = GetMonotonicTime();

    for (int i = 0; i < iterations; i++) {
        sum += (uint64_t)atoi(strings[i % RG_LEN(strings)]);
    }

    // Help prevent optimisation of loop
    {
        PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
        RG_DEFER { PopLogFilter(); };

        LogInfo("Sum = %1", sum);
    }

    int64_t time = GetMonotonicTime() - start;
    LogInfo("Time: %1s", FmtDouble((double)time / 1000.0, 2));

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
