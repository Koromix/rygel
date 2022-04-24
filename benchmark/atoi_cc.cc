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

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

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

    typedef int AtoiFunc(const char *ptr);

#ifdef _WIN32
    HMODULE module = LoadLibraryA("msvcrt.dll");
    RG_DEFER { FreeLibrary(module); };

    AtoiFunc *atoi_ptr = (AtoiFunc *)GetProcAddress(module, "atoi");
#else
    AtoiFunc *atoi_ptr = (AtoiFunc *)dlsym(RTLD_DEFAULT, "atoi");
#endif

    int64_t start = GetMonotonicTime();

    for (int i = 0; i < iterations; i++) {
        sum += (uint64_t)atoi_ptr(strings[i % RG_LEN(strings)]);
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
