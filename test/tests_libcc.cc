// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../src/core/libcc/libcc.hh"
#include "tests.hh"

#ifdef _WIN32
    extern "C" __declspec(dllimport) int __stdcall PathMatchSpecA(const char *pszFile, const char *pszSpec);
#else
    #include <fnmatch.h>
#endif

// Comparative benchmarks
#include "vendor/stb_sprintf.h"
#include "vendor/fmt/format.h"

namespace RG {

void TestMatchPathName()
{
    TEST_FUNCTION("Testing MatchPathName");

#define CHECK_PATH_SPEC(Pattern, Path, Expected) \
        TEST(MatchPathName(Path, Pattern) == Expected)

    // Stolen from FreeBSD
    CHECK_PATH_SPEC("", "", true);
    CHECK_PATH_SPEC("a", "a", true);
    CHECK_PATH_SPEC("a", "b", false);
    CHECK_PATH_SPEC("a", "A", false);
    CHECK_PATH_SPEC("*", "a", true);
    CHECK_PATH_SPEC("*", "aa", true);
    CHECK_PATH_SPEC("*a", "a", true);
    CHECK_PATH_SPEC("*a", "b", false);
    CHECK_PATH_SPEC("*a*", "b", false);
    CHECK_PATH_SPEC("*a*b*", "ab", true);
    CHECK_PATH_SPEC("*a*b*", "qaqbq", true);
    CHECK_PATH_SPEC("*a*bb*", "qaqbqbbq", true);
    CHECK_PATH_SPEC("*a*bc*", "qaqbqbcq", true);
    CHECK_PATH_SPEC("*a*bb*", "qaqbqbb", true);
    CHECK_PATH_SPEC("*a*bc*", "qaqbqbc", true);
    CHECK_PATH_SPEC("*a*bb", "qaqbqbb", true);
    CHECK_PATH_SPEC("*a*bc", "qaqbqbc", true);
    CHECK_PATH_SPEC("*a*bb", "qaqbqbbq", false);
    CHECK_PATH_SPEC("*a*bc", "qaqbqbcq", false);
    CHECK_PATH_SPEC("*a*a*a*a*a*a*a*a*a*a*", "aaaaaaaaa", false);
    CHECK_PATH_SPEC("*a*a*a*a*a*a*a*a*a*a*", "aaaaaaaaaa", true);
    CHECK_PATH_SPEC("*a*a*a*a*a*a*a*a*a*a*", "aaaaaaaaaaa", true);
    CHECK_PATH_SPEC(".*.*.*.*.*.*.*.*.*.*", ".........", false);
    CHECK_PATH_SPEC(".*.*.*.*.*.*.*.*.*.*", "..........", true);
    CHECK_PATH_SPEC(".*.*.*.*.*.*.*.*.*.*", "...........", true);
    CHECK_PATH_SPEC("*?*?*?*?*?*?*?*?*?*?*", "123456789", false);
    CHECK_PATH_SPEC("??????????*", "123456789", false);
    CHECK_PATH_SPEC("*??????????", "123456789", false);
    CHECK_PATH_SPEC("*?*?*?*?*?*?*?*?*?*?*", "1234567890", true);
    CHECK_PATH_SPEC("??????????*", "1234567890", true);
    CHECK_PATH_SPEC("*??????????", "1234567890", true);
    CHECK_PATH_SPEC("*?*?*?*?*?*?*?*?*?*?*", "12345678901", true);
    CHECK_PATH_SPEC("??????????*", "12345678901", true);
    CHECK_PATH_SPEC("*??????????", "12345678901", true);
    CHECK_PATH_SPEC(".*", ".", true);
    CHECK_PATH_SPEC(".*", "..", true);
    CHECK_PATH_SPEC(".*", ".a", true);
    CHECK_PATH_SPEC("a*", "a.", true);
    CHECK_PATH_SPEC("a/a", "a/a", true);
    CHECK_PATH_SPEC("a/*", "a/a", true);
    CHECK_PATH_SPEC("*/a", "a/a", true);
    CHECK_PATH_SPEC("*/*", "a/a", true);
    CHECK_PATH_SPEC("a*b/*", "abbb/x", true);
    CHECK_PATH_SPEC("a*b/*", "abbb/.x", true);
    CHECK_PATH_SPEC("*", "a/a", false);
    CHECK_PATH_SPEC("*/*", "a/a/a", false);
    CHECK_PATH_SPEC("a", "a/b", false);
    CHECK_PATH_SPEC("*", "a/b", false);
    CHECK_PATH_SPEC("*b", "a/b", false);

    // Stolen from glibc
    CHECK_PATH_SPEC("*.c", "foo.c", true);
    CHECK_PATH_SPEC("*.c", ".c", true);
    CHECK_PATH_SPEC("*.a", "foo.c", false);
    CHECK_PATH_SPEC("*.c", ".foo.c", true);
    CHECK_PATH_SPEC("a/*.c", "a/x.c", true);
    CHECK_PATH_SPEC("a*.c", "a/x.c", false);
    CHECK_PATH_SPEC("*/foo", "/foo", true);
    CHECK_PATH_SPEC("*", "a/b", false);
    CHECK_PATH_SPEC("??""/b", "aa/b", true);
    CHECK_PATH_SPEC("???b", "aa/b", false);

    // Those are mine
    CHECK_PATH_SPEC("xxx", "xxx", true);
    CHECK_PATH_SPEC("x?x", "xxx", true);
    CHECK_PATH_SPEC("xxxx", "xxx", false);
    CHECK_PATH_SPEC("x*x", "xxx", true);
    CHECK_PATH_SPEC("*c", "abc", true);
    CHECK_PATH_SPEC("*b", "abc", false);
    CHECK_PATH_SPEC("a*", "abc", true);
    CHECK_PATH_SPEC("*d*", "abc", false);
    CHECK_PATH_SPEC("*b*", "abc", true);
    CHECK_PATH_SPEC("a*d*/f", "abcqzdde/f", true);
    CHECK_PATH_SPEC("a*d**f", "abcqzdde/f", true);
    CHECK_PATH_SPEC("a*d*f", "abcqzdde/f", false);

#undef CHECK_PATH_SPEC
}

void BenchFmt()
{
    PrintLn("Benchmarking Fmt");

    static const int iterations = 800000;

#ifdef _WIN32
    FILE *fp = OpenFile("NUL", OpenFileMode::Write);
#else
    FILE *fp = OpenFile("/dev/null", OpenFileMode::Write);
#endif
    RG_ASSERT(fp);
    RG_DEFER { fclose(fp); };

    RunBenchmark("printf", iterations, [&]() {
        fprintf(fp, "%d:%d:%d:%s:%p:%c:%%\n",
                1234, 42, -313, "str", (void*)1000, 'X');
    });

    RunBenchmark("snprintf", iterations, [&]() {
        char buf[1024];
        snprintf(buf, RG_SIZE(buf), "%d:%d:%d:%s:%p:%c:%%\n",
                1234, 42, -313, "str", (void*)1000, 'X');
    });

#ifndef _WIN32
    RunBenchmark("asprintf", iterations, [&]() {
        char *ret = nullptr;
        asprintf(&ret, "%d:%d:%d:%s:%p:%c:%%\n",
                 1234, 42, -313, "str", (void*)1000, 'X');
        free(ret);
    });
#endif

    RunBenchmark("stbsp_snprintf", iterations, [&]() {
        char buf[1024];
        stbsp_snprintf(buf, RG_SIZE(buf), "%d:%d:%d:%s:%p:%c:%%\n",
                       1234, 42, -313, "str", (void*)1000, 'X');
    });

    RunBenchmark("fmt::format", iterations, [&]() {
        fmt::format("{}:{}:{}:{}:{}:{}%\n", 1234, 42, -313, "str", (void *)1000, 'X');
    });

    RunBenchmark("fmt::format_to", iterations, [&]() {
        fmt::memory_buffer buf;
        fmt::format_to(buf, "{}:{}:{}:{}:{}:{}%\n", 1234, 42, -313, "str", (void *)1000, 'X');
    });

    RunBenchmark("libcc Print", iterations, [&]() {
        Print(fp, "%1:%2:%3:%4:%5:%6:%%\n",
              1234, 42, -313, "str", (void*)1000, 'X');
    });

    RunBenchmark("libcc Fmt (allocator)", iterations, [&]() {
        LinkedAllocator temp_alloc;
        Fmt(&temp_alloc, "%1:%2:%3:%4:%5:%6:%%\n",
            1234, 42, -313, "str", (void*)1000, 'X');
    });

    RunBenchmark("libcc Fmt (heap)", iterations, [&]() {
        HeapArray<char> buf;
        Fmt(&buf, "%1:%2:%3:%4:%5:%6:%%\n",
            1234, 42, -313, "str", (void*)1000, 'X');
        buf.RemoveFrom(0);
    });

    RunBenchmark("libcc Fmt (buffer)", iterations, [&]() {
        LocalArray<char, 1024> buf;
        buf.len = Fmt(buf.data, "%1:%2:%3:%4:%5:%6:%%\n",
                      1234, 42, -313, "str", (void*)1000, 'X').len;
    });
}

void BenchMatchPathName()
{
    PrintLn("Benchmarking MatchPathName");

    static const int iterations = 3000000;

#ifdef _WIN32
    RunBenchmark("PathMatchSpecA", iterations, [&]() {
        PathMatchSpecA("aaa/bbb", "a*/*b");
    });
#else
    RunBenchmark("fnmatch", iterations, [&]() {
        fnmatch("a*/*b", "aaa/bbb", FNM_PATHNAME);
    });
#endif

    RunBenchmark("MatchPathName", iterations, [&]() {
        MatchPathName("aaa/bbb", "a*/*b");
    });
}

}
