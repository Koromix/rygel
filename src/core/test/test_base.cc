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

#include <string>
#include <unordered_map>

// Comparative benchmarks
#if defined(_WIN32)
    extern "C" __declspec(dllimport) int __stdcall PathMatchSpecA(const char *pszFile, const char *pszSpec);
#endif
#include "musl/fnmatch.h"
#include "vendor/stb/stb_sprintf.h"
#include "vendor/fmt/include/fmt/format.h"
#include "vendor/fmt/include/fmt/compile.h"

namespace RG {

TEST_FUNCTION("base/FormatDouble")
{
    char buf[512];

    // Simple stuff
    TEST_STR(Fmt(buf, "%1", 0.0), "0");
    TEST_STR(Fmt(buf, "%1", 1e-4), "0.0001");
    TEST_STR(Fmt(buf, "%1", 1e-7), "1e-7");
    TEST_STR(Fmt(buf, "%1", 9.999e-7), "9.999e-7");
    TEST_STR(Fmt(buf, "%1", 1e10), "10000000000");
    TEST_STR(Fmt(buf, "%1", 1e11), "100000000000");
    TEST_STR(Fmt(buf, "%1", 1234e7), "12340000000");
    TEST_STR(Fmt(buf, "%1", 1234e-2), "12.34");
    TEST_STR(Fmt(buf, "%1", 1234e-6), "0.001234");

    // Float vs Double
    TEST_STR(Fmt(buf, "%1", 0.1f), "0.1");
    TEST_STR(Fmt(buf, "%1", double(0.1f)), "0.10000000149011612");

    // Typical Grisu/Grisu2/Grisu3 errors
    TEST_STR(Fmt(buf, "%1", 1e23), "1e+23");
    TEST_STR(Fmt(buf, "%1", 9e-265), "9e-265");
    TEST_STR(Fmt(buf, "%1", 5.423717798060526e+125), "5.423717798060526e+125");
    TEST_STR(Fmt(buf, "%1", 1.372371880954233e-288), "1.372371880954233e-288");
    TEST_STR(Fmt(buf, "%1", 55388492.622190244), "55388492.622190244");
    TEST_STR(Fmt(buf, "%1", 2.2506787569811123e-253), "2.2506787569811123e-253");
    TEST_STR(Fmt(buf, "%1", 2.9802322387695312e-8), "2.9802322387695312e-8");

    // Fixed precision
    TEST_STR(Fmt(buf, "%1", FmtDouble(12.243, 2, 2)), "12.24");
    TEST_STR(Fmt(buf, "%1", FmtDouble(0.1, 1, 1)), "0.1");
    TEST_STR(Fmt(buf, "%1", FmtDouble(0.8, 1, 1)), "0.8");
    TEST_STR(Fmt(buf, "%1", FmtDouble(0.01, 1, 1)), "0.0");
    TEST_STR(Fmt(buf, "%1", FmtDouble(0.08, 1, 1)), "0.1");
    TEST_STR(Fmt(buf, "%1", FmtDouble(0.001, 1, 1)), "0.0");
    TEST_STR(Fmt(buf, "%1", FmtDouble(0.008, 1, 1)), "0.0");
    TEST_STR(Fmt(buf, "%1", FmtDouble(9.999, 1, 1)), "10.0");
    TEST_STR(Fmt(buf, "%1", FmtDouble(9.55, 1, 1)), "9.6");
    TEST_STR(Fmt(buf, "%1", FmtDouble(9.95, 1, 1)), "10.0");
    TEST_STR(Fmt(buf, "%1", FmtDouble(0.02, 0, 1)), "0");
    TEST_STR(Fmt(buf, "%1", FmtDouble(0.2, 0, 0)), "0");
    TEST_STR(Fmt(buf, "%1", FmtDouble(0.6, 0, 0)), "0");
    TEST_STR(Fmt(buf, "%1", FmtDouble(1.6, 0, 0)), "2");
    TEST_STR(Fmt(buf, "%1", FmtDouble(10.6, 0, 0)), "11");
    TEST_STR(Fmt(buf, "%1", FmtDouble(10.2, 0, 0)), "10");
}

TEST_FUNCTION("base/FormatSize")
{
    char buf[512];

    // Memory sizes (binary / 1024)
    TEST_STR(Fmt(buf, "%1", FmtMemSize(999)), "999 B");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(1024)), "1.000 kiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(1025)), "1.001 kiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(10240)), "10.00 kiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(10243)), "10.00 kiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(10247)), "10.01 kiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(1048523)), "1023.9 kiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(1048524)), "1.000 MiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(1073688136)), "1023.9 MiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(1073688137)), "1.000 GiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(10736881370)), "10.00 GiB");
    TEST_STR(Fmt(buf, "%1", FmtMemSize(107368813700)), "100.0 GiB");

    // Disk sizes (SI / 1000)
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(999)), "999 B");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(1000)), "1.000 kB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(1001)), "1.001 kB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(10000)), "10.00 kB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(10001)), "10.00 kB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(10005)), "10.01 kB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(999900)), "999.9 kB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(999949)), "999.9 kB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(999999)), "1.000 MB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(1000000)), "1.000 MB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(1001499)), "1.001 MB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(1001500)), "1.002 MB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(1000000000)), "1.000 GB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(1001499000)), "1.001 GB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(1001500000)), "1.002 GB");
    TEST_STR(Fmt(buf, "%1", FmtDiskSize(10000000000000)), "10000.0 GB");
}

TEST_FUNCTION("base/MatchPathName")
{
#define CHECK_PATH_SPEC(Pattern, Path, Expected) \
        TEST_EQ(MatchPathName((Path), (Pattern)), (Expected))

    // Stolen from FreeBSD
    CHECK_PATH_SPEC("", "", true);
    CHECK_PATH_SPEC("a", "a", true);
    CHECK_PATH_SPEC("a", "b", false);
#if defined(_WIN32)
    CHECK_PATH_SPEC("a", "A", true);
#else
    CHECK_PATH_SPEC("a", "A", false);
#endif
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

TEST_FUNCTION("base/FastRandom")
{
    for (int i = 0; i < 2; i++) {
        FastRandom rng(42);

        TEST_EQ(rng.GetInt(1, 24097), 18776);
        TEST_EQ(rng.GetInt(1, 24097), 20580);
        TEST_EQ(rng.GetInt(1, 24097), 12480);
        TEST_EQ(rng.GetInt(1, 24097), 13705);
        TEST_EQ(rng.GetInt(1, 24097), 23606);
        TEST_EQ(rng.GetInt(1, 24097), 18997);
        TEST_EQ(rng.GetInt(1, 24097), 3751);
        TEST_EQ(rng.GetInt(1, 24097), 2556);
        TEST_EQ(rng.GetInt(1, 24097), 20979);
        TEST_EQ(rng.GetInt(1, 24097), 9832);
        TEST_EQ(rng.GetInt(1, 24097), 5825);
        TEST_EQ(rng.GetInt(1, 24097), 1645);
        TEST_EQ(rng.GetInt(1, 24097), 3272);
        TEST_EQ(rng.GetInt(1, 24097), 3614);
        TEST_EQ(rng.GetInt(1, 24097), 21157);
        TEST_EQ(rng.GetInt(1, 24097), 19320);
        TEST_EQ(rng.GetInt(1, 24097), 6459);
        TEST_EQ(rng.GetInt(1, 24097), 12383);
        TEST_EQ(rng.GetInt(1, 24097), 2714);
        TEST_EQ(rng.GetInt(1, 24097), 791);
        TEST_EQ(rng.GetInt(1, 24097), 3227);
    }

    for (int i = 0; i < 2; i++) {
        FastRandom rng(24);

        TEST_EQ(rng.GetInt(1, 24097), 931);
        TEST_EQ(rng.GetInt(1, 24097), 10937);
        TEST_EQ(rng.GetInt(1, 24097), 23722);
        TEST_EQ(rng.GetInt(1, 24097), 4287);
        TEST_EQ(rng.GetInt(1, 24097), 3511);
        TEST_EQ(rng.GetInt(1, 24097), 4221);
        TEST_EQ(rng.GetInt(1, 24097), 24011);
        TEST_EQ(rng.GetInt(1, 24097), 12267);
        TEST_EQ(rng.GetInt(1, 24097), 19237);
        TEST_EQ(rng.GetInt(1, 24097), 17957);
        TEST_EQ(rng.GetInt(1, 24097), 12928);
        TEST_EQ(rng.GetInt(1, 24097), 7037);
        TEST_EQ(rng.GetInt(1, 24097), 4299);
        TEST_EQ(rng.GetInt(1, 24097), 14853);
        TEST_EQ(rng.GetInt(1, 24097), 4323);
        TEST_EQ(rng.GetInt(1, 24097), 4861);
        TEST_EQ(rng.GetInt(1, 24097), 19231);
        TEST_EQ(rng.GetInt(1, 24097), 12924);
        TEST_EQ(rng.GetInt(1, 24097), 9126);
        TEST_EQ(rng.GetInt(1, 24097), 20133);
        TEST_EQ(rng.GetInt(1, 24097), 20881);
    }

    for (int i = 4; i < 1000; i++) {
        FastRandom rng;

        for (int j = 0; j < 100000; j++) {
            int value = rng.GetInt(0, i);
            TEST_EX(value >= 0 && value < i, "GetInt(0, %2): %1 >= 0 && %1 < %2", value, i);
        }

        for (int j = 0; j < 100000; j++) {
            int64_t value = rng.GetInt64(0, i);
            TEST_EX(value >= 0 && value < i, "GetInt(0, %2): %1 >= 0 && %1 < %2", value, i);
        }
    }
}

TEST_FUNCTION("base/ParseBool")
{
    PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
    RG_DEFER { PopLogFilter(); };

#define VALID(Str, Flags, Value, Remain) \
        do { \
            bool value; \
            Span<const char> remain; \
            bool valid = ParseBool((Str), &value, (Flags), &remain); \
             \
            TEST_EX(valid && value == (Value) && remain.len == (Remain), \
                    "%1: Valid %2 [%3] == %4 %5 [%6]", (Str), (Value), (Remain), valid ? "Valid" : "Invalid", value, remain.len); \
        } while (false)
#define INVALID(Str, Flags) \
        do { \
            bool value; \
            bool valid = ParseBool((Str), &value, (Flags)); \
             \
            TEST_EX(!valid, "%1: Invalid == %2 %3", (Str), valid ? "Valid" : "Invalid", value); \
        } while (false)

    VALID("1", RG_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("on", RG_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("y", RG_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("yes", RG_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("true", RG_DEFAULT_PARSE_FLAGS, true, 0);

    VALID("0", RG_DEFAULT_PARSE_FLAGS, false, 0);
    VALID("off", RG_DEFAULT_PARSE_FLAGS, false, 0);
    VALID("n", RG_DEFAULT_PARSE_FLAGS, false, 0);
    VALID("no", RG_DEFAULT_PARSE_FLAGS, false, 0);
    VALID("false", RG_DEFAULT_PARSE_FLAGS, false, 0);

    VALID("true", RG_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("TrUe", RG_DEFAULT_PARSE_FLAGS, true, 0);
    INVALID("trues", RG_DEFAULT_PARSE_FLAGS);
    VALID("FALSE!", 0, false, 1);
    VALID("Y", RG_DEFAULT_PARSE_FLAGS, true, 0);
    INVALID("YE", RG_DEFAULT_PARSE_FLAGS);
    VALID("yes", 0, true, 0);
    VALID("yes!!!", 0, true, 3);
    VALID("n+", 0, false, 1);
    VALID("no+", 0, false, 1);
    INVALID("no+", RG_DEFAULT_PARSE_FLAGS);

#undef INVALID
#undef VALID
}

TEST_FUNCTION("base/ParseSize")
{
    PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
    RG_DEFER { PopLogFilter(); };

#define VALID(Str, Flags, Value, Remain) \
        do { \
            int64_t value; \
            Span<const char> remain; \
            bool valid = ParseSize((Str), &value, (Flags), &remain); \
             \
            TEST_EX(valid && value == (Value) && remain.len == (Remain), \
                    "%1: Valid %2 [%3] == %4 %5 [%6]", (Str), (Value), (Remain), valid ? "Valid" : "Invalid", value, remain.len); \
        } while (false)
#define INVALID(Str, Flags) \
        do { \
            int64_t value; \
            bool valid = ParseSize((Str), &value, (Flags)); \
             \
            TEST_EX(!valid, "%1: Invalid == %2 %3", (Str), valid ? "Valid" : "Invalid", value); \
        } while (false)

    VALID("1", RG_DEFAULT_PARSE_FLAGS, 1, 0);
    VALID("2147483648", RG_DEFAULT_PARSE_FLAGS, 2147483648, 0);
    VALID("4294967295", RG_DEFAULT_PARSE_FLAGS, 4294967295, 0);
    INVALID("1S", RG_DEFAULT_PARSE_FLAGS);

    VALID("4B", RG_DEFAULT_PARSE_FLAGS, 4, 0);
    VALID("4k", RG_DEFAULT_PARSE_FLAGS, 4000, 0);
    VALID("4M", RG_DEFAULT_PARSE_FLAGS, 4000000, 0);
    VALID("4G", RG_DEFAULT_PARSE_FLAGS, 4000000000, 0);
    VALID("4T", RG_DEFAULT_PARSE_FLAGS, 4000000000000ll, 0);
    VALID("4s", 0, 4, 1);
    INVALID("4s", RG_DEFAULT_PARSE_FLAGS);

    VALID("4G", RG_DEFAULT_PARSE_FLAGS, 4000000000, 0);
    VALID("4Gi", 0, 4000000000, 1);
    INVALID("4Gi", RG_DEFAULT_PARSE_FLAGS);

#undef INVALID
#undef VALID
}

TEST_FUNCTION("base/ParseDuration")
{
    PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
    RG_DEFER { PopLogFilter(); };

#define VALID(Str, Flags, Value, Remain) \
        do { \
            int64_t value; \
            Span<const char> remain; \
            bool valid = ParseDuration((Str), &value, (Flags), &remain); \
             \
            TEST_EX(valid && value == (Value) && remain.len == (Remain), \
                    "%1: Valid %2 [%3] == %4 %5 [%6]", (Str), (Value), (Remain), valid ? "Valid" : "Invalid", value, remain.len); \
        } while (false)
#define INVALID(Str, Flags) \
        do { \
            int64_t value; \
            bool valid = ParseDuration((Str), &value, (Flags)); \
             \
            TEST_EX(!valid, "%1: Invalid == %2 %3", (Str), valid ? "Valid" : "Invalid", value); \
        } while (false)

    VALID("1", RG_DEFAULT_PARSE_FLAGS, 1000, 0);
    VALID("300", RG_DEFAULT_PARSE_FLAGS, 300000, 0);
    INVALID("1p", RG_DEFAULT_PARSE_FLAGS);

    VALID("4s", RG_DEFAULT_PARSE_FLAGS, 4000, 0);
    VALID("4m", RG_DEFAULT_PARSE_FLAGS, 4000 * 60, 0);
    VALID("4h", RG_DEFAULT_PARSE_FLAGS, 4000 * 3600, 0);
    VALID("4d", RG_DEFAULT_PARSE_FLAGS, 4000 * 86400, 0);
    VALID("4w", 0, 4000, 1);
    INVALID("4w", RG_DEFAULT_PARSE_FLAGS);

    VALID("4d", RG_DEFAULT_PARSE_FLAGS, 4000 * 86400, 0);
    VALID("4dt", 0, 4000 * 86400, 1);
    INVALID("4dt", RG_DEFAULT_PARSE_FLAGS);

#undef INVALID
#undef VALID
}

TEST_FUNCTION("base/GetRandomInt")
{
    static const int iterations = 100;
    static const int upper = 2000;
    static const int loop = 100000;

    bool varied = true;

    for (int i = 0; i < iterations; i++) {
        int max = GetRandomInt(100, upper);

        TEST(max >= 100);
        TEST(max < upper);

        int distrib = 0;
        bool memory[upper] = {};

        for (int j = 0; j < loop; j++) {
            int rnd = GetRandomInt(0, max);

            TEST(rnd >= 0);
            TEST(rnd < max);

            distrib += !memory[rnd];
            memory[rnd] = true;
        }

        varied &= (distrib > 95 * max / 100);
    }

    TEST_EX(varied, "GetRandomInt() values look well distributed");
}

TEST_FUNCTION("base/OptionParser")
{
    // Empty

    {
        OptionParser opt({});

        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    // Short options

    {
        const char *args[] = {"-f"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "-f");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"-foo", "-b"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "-b");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    // Long options

    {
        const char *args[] = {"--foobar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foobar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"--foo", "--bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST_STR(opt.Next(), "--bar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    // Mixed tests

    {
        const char *args[] = {"--foo", "-bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST_STR(opt.Next(), "-b");
        TEST_STR(opt.Next(), "-a");
        TEST_STR(opt.Next(), "-r");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"-foo", "--bar", "-FOO"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "--bar");
        TEST_STR(opt.Next(), "-F");
        TEST_STR(opt.Next(), "-O");
        TEST_STR(opt.Next(), "-O");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    // Values

    {
        const char *args[] = {"-f", "bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"-fbar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"--foo=bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"--foo", "bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"bar", "--foo"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST_EQ(opt.ConsumeValue(), nullptr);
        TEST_EQ(opt.Next(), nullptr);
        TEST_STR(opt.ConsumeNonOption(), "bar");
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    // Positional tests

    {
        const char *args[] = {"foo", "bar"};
        OptionParser opt(args);

        TEST_STR(opt.ConsumeNonOption(), "foo");
        TEST_STR(opt.ConsumeNonOption(), "bar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"foo", "--foobar", "bar"};
        OptionParser opt(args);

        opt.Next();
        opt.Next();
        TEST_STR(opt.ConsumeNonOption(), "foo");
        TEST_STR(opt.ConsumeNonOption(), "bar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"foobar", "--", "foo", "--bar"};
        OptionParser opt(args);

        opt.Next();
        TEST_STR(opt.ConsumeNonOption(), "foobar");
        TEST_STR(opt.ConsumeNonOption(), "foo");
        TEST_STR(opt.ConsumeNonOption(), "--bar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    {
        const char *args[] = {"foo", "FOO", "foobar", "--", "bar", "BAR", "barfoo", "BARFOO"};
        OptionParser opt(args);

        opt.Next();
        TEST_STR(opt.ConsumeNonOption(), "foo");
        TEST_STR(opt.ConsumeNonOption(), "FOO");
        TEST_STR(opt.ConsumeNonOption(), "foobar");
        TEST_STR(opt.ConsumeNonOption(), "bar");
        TEST_STR(opt.ConsumeNonOption(), "BAR");
        TEST_STR(opt.ConsumeNonOption(), "barfoo");
        TEST_STR(opt.ConsumeNonOption(), "BARFOO");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    // ConsumeNonOption

    {
        const char *args[] = {"foo", "-f", "bar"};
        OptionParser opt(args);

        TEST_STR(opt.ConsumeNonOption(), "foo");
        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.ConsumeNonOption(), "bar");
        TEST_EQ(opt.Next(), nullptr);
    }

    {
        const char *args[] = {"bar1", "-foo", "bar2"};
        OptionParser opt(args);

        TEST_STR(opt.ConsumeNonOption(), "bar1");
        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.ConsumeNonOption(), "bar2");
        TEST_EQ(opt.Next(), nullptr);
    }

    // Complex tests

    {
        const char *args[] = {"--foo1", "bar", "fooBAR", "-foo2", "--foo3=BAR", "-fbar",
                              "--", "FOOBAR", "--", "--FOOBAR"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo1");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.Next(), "-o");
        TEST_EQ(opt.ConsumeValue(), nullptr);
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "-2");
        TEST_STR(opt.Next(), "--foo3");
        TEST_STR(opt.ConsumeValue(), "BAR");
        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST_EQ(opt.Next(), nullptr);
        TEST_STR(opt.ConsumeNonOption(), "fooBAR");
        TEST_STR(opt.ConsumeNonOption(), "FOOBAR");
        TEST_STR(opt.ConsumeNonOption(), "--");
        TEST_EQ(opt.Next(), nullptr);
        TEST_STR(opt.ConsumeNonOption(), "--FOOBAR");
        TEST_EQ(opt.Next(), nullptr);
        TEST_EQ(opt.ConsumeNonOption(), nullptr);
    }

    // Skip mode

    {
        const char *args[] = {"-f", "FOO", "--bar", "--foo", "BAR"};
        OptionParser opt(args, OptionMode::Skip);

        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.Next(), "--bar");
        TEST_STR(opt.Next(), "--foo");
        TEST_STR(opt.ConsumeNonOption(), "BAR");
        TEST_STR(opt.Next(), nullptr);
        TEST_STR(opt.ConsumeNonOption(), nullptr);
    }

    // Stop mode

    {
        const char *args[] = {"-f", "--bar", "FOO", "--foo", "BAR"};
        OptionParser opt(args, OptionMode::Stop);

        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.Next(), "--bar");
        TEST_STR(opt.Next(), nullptr);
        TEST_STR(opt.ConsumeNonOption(), "FOO");
        TEST_STR(opt.ConsumeNonOption(), "--foo");
        TEST_STR(opt.ConsumeNonOption(), "BAR");
        TEST_STR(opt.ConsumeNonOption(), nullptr);
    }
}

TEST_FUNCTION("base/PathCheck")
{
    TEST_EQ(PathIsAbsolute("foo"), false);
    TEST_EQ(PathIsAbsolute(""), false);
    TEST_EQ(PathIsAbsolute("/foo"), true);
    TEST_EQ(PathIsAbsolute("/"), true);
#if defined(_WIN32)
    TEST_EQ(PathIsAbsolute("\\foo"), true);
    TEST_EQ(PathIsAbsolute("\\"), true);
    TEST_EQ(PathIsAbsolute("C:foo"), true); // Technically not absolute but it seems safer to deal with it this way
    TEST_EQ(PathIsAbsolute("C:/foo"), true);
    TEST_EQ(PathIsAbsolute("C:/"), true);
    TEST_EQ(PathIsAbsolute("C:\\foo"), true);
    TEST_EQ(PathIsAbsolute("C:\\"), true);
#endif

    TEST_EQ(PathContainsDotDot(".."), true);
    TEST_EQ(PathContainsDotDot("/.."), true);
    TEST_EQ(PathContainsDotDot("/../"), true);
    TEST_EQ(PathContainsDotDot("a.."), false);
    TEST_EQ(PathContainsDotDot("..b"), false);
    TEST_EQ(PathContainsDotDot("..b"), false);
    TEST_EQ(PathContainsDotDot("foo/bar/.."), true);
    TEST_EQ(PathContainsDotDot("foo/../bar"), true);
    TEST_EQ(PathContainsDotDot("foo../bar"), false);
    TEST_EQ(PathContainsDotDot("foo/./bar"), false);
#if defined(_WIN32)
    TEST_EQ(PathContainsDotDot(".."), true);
    TEST_EQ(PathContainsDotDot("\\.."), true);
    TEST_EQ(PathContainsDotDot("\\..\\"), true);
    TEST_EQ(PathContainsDotDot("a.."), false);
    TEST_EQ(PathContainsDotDot("..b"), false);
    TEST_EQ(PathContainsDotDot("..b"), false);
    TEST_EQ(PathContainsDotDot("foo\\bar\\.."), true);
    TEST_EQ(PathContainsDotDot("foo\\..\\bar"), true);
    TEST_EQ(PathContainsDotDot("foo..\\bar"), false);
    TEST_EQ(PathContainsDotDot("foo\\.\\bar"), false);
#endif
}

struct IntBucket {
    int key;
    int value;

    RG_HASHTABLE_HANDLER(IntBucket, key);
};

struct StrBucket {
    const char *key;
    int value;

    RG_HASHTABLE_HANDLER(StrBucket, key);
};

TEST_FUNCTION("base/HashTable")
{
    BlockAllocator temp_alloc;

    // Integer keys

    for (int i = 0; i < 16; i++) {
        std::unordered_map<int, int> ref;

        HashTable<int, IntBucket> table;
        HashMap<int, int> map;
        HashSet<int> set;

        for (int j = 0; j < 1000; j++) {
            int key = 0;
            do {
                key = GetRandomInt(0, INT_MAX);
            } while (ref.count(key));

            TEST(!map.Find(key));
            TEST(!set.Find(key));

            int value = GetRandomInt(0, INT_MAX);
            ref[key] = value;

            table.Set({ key, value });
            map.Set(key, value);
            set.Set(key);
        }

        for (const auto &it: ref) {
            if (it.first % 3) {
                Size prev = table.count;

                table.Remove(it.first);
                map.Remove(it.first);
                set.Remove(it.first);

                TEST_EQ(table.count, prev - 1);
                TEST_EQ(map.table.count, prev - 1);
                TEST_EQ(set.table.count, prev - 1);
            }
        }

        for (const auto &it: ref) {
            if (it.first % 3) {
                TEST(!table.Find(it.first));
                TEST(!map.Find(it.first));
                TEST(!set.Find(it.first));
            } else {
                TEST_EQ(table.FindValue(it.first, {}).value, it.second);
                TEST_EQ(map.FindValue(it.first, 0), it.second);
                TEST(set.Find(it.first));
            }
        }
    }

    // String keys

    for (int i = 0; i < 16; i++) {
        std::unordered_map<std::string, int> ref;

        HashTable<const char *, StrBucket> table;
        HashMap<const char *, int> map;
        HashSet<const char *> set;

        for (int j = 0; j < 1000; j++) {
            std::string key;
            do {
                char buf[16];
                Fmt(buf, "%1", FmtRandom(8));
                key = std::string(buf);
            } while (ref.count(key));

            TEST(!table.Find(key.c_str()));
            TEST(!map.Find(key.c_str()));
            TEST(!set.Find(key.c_str()));

            int value = GetRandomInt(0, INT_MAX);
            ref[key] = value;

            const char *copy = DuplicateString(key.c_str(), &temp_alloc).ptr;

            table.Set({ copy, value });
            map.Set(copy, value);
            set.Set(copy);
        }

        for (const auto &it: ref) {
            char c = *it.first.c_str();

            if (c % 3) {
                Size prev = table.count;

                table.Remove(it.first.c_str());
                map.Remove(it.first.c_str());
                set.Remove(it.first.c_str());

                TEST_EQ(table.count, prev - 1);
                TEST_EQ(map.table.count, prev - 1);
                TEST_EQ(set.table.count, prev - 1);
            }
        }

        for (const auto &it: ref) {
            char c = *it.first.c_str();

            if (c % 3) {
                TEST(!table.Find(it.first.c_str()));
                TEST(!map.Find(it.first.c_str()));
                TEST(!set.Find(it.first.c_str()));
            } else {
                TEST_EQ(table.FindValue(it.first.c_str(), {}).value, it.second);
                TEST_EQ(map.FindValue(it.first.c_str(), 0), it.second);
                TEST(set.Find(it.first.c_str()));
            }
        }
    }
}

BENCHMARK_FUNCTION("base/Fmt")
{
    static const int iterations = 1600000;

#if defined(_WIN32)
    FILE *fp = fopen("\\\\.\\NUL", "wb");
    int fd = fileno(fp);
#else
    int fd = OpenFile("/dev/null", (int)OpenFlag::Write);
    FILE *fp = fdopen(fd, "wb");
#endif
    RG_ASSERT(fp);
    RG_DEFER { fclose(fp); };

    StreamWriter writer(fd, "/dev/null");
    RG_ASSERT(writer.IsValid());

    RunBenchmark("printf", iterations, [&](Size) {
        fprintf(fp, "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });

    RunBenchmark("snprintf", iterations, [&](Size) {
        char buf[1024];
        snprintf(buf, RG_SIZE(buf), "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });

#if !defined(_WIN32)
    RunBenchmark("asprintf", iterations, [&](Size) {
        char *str = nullptr;
        int ret = asprintf(&str, "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
        (void)ret;
        free(str);
    });
#endif

    RunBenchmark("stbsp_snprintf", iterations, [&](Size) {
        char buf[1024];
        stbsp_snprintf(buf, RG_SIZE(buf), "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });

    RunBenchmark("fmt::format_to", iterations, [&](Size) {
        char buf[1024];
        fmt::format_to(buf, "{}:{}:{}:{}:{}:{}%\n", 1234, 42, -313.3, "str", (void *)1000, 'X');
    });

    RunBenchmark("fmt::format_to (FMT_COMPILE)", iterations, [&](Size) {
        char buf[1024];
        fmt::format_to(buf, FMT_COMPILE("{}:{}:{}:{}:{}:{}%\n"), 1234, 42, -313.3, "str", (void *)1000, 'X');
    });

    RunBenchmark("base Fmt", iterations, [&](Size) {
        LocalArray<char, 1024> buf;
        buf.len = Fmt(buf.data, "%1:%2:%3:%4:%5:%6:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X').len;
    });

    RunBenchmark("base Fmt (allocator)", iterations, [&](Size) {
        BlockAllocator temp_alloc;
        Fmt(&temp_alloc, "%1:%2:%3:%4:%5:%6:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });

    RunBenchmark("base Fmt (heap)", iterations, [&](Size) {
        HeapArray<char> buf;
        Fmt(&buf, "%1:%2:%3:%4:%5:%6:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
        buf.RemoveFrom(0);
    });

    RunBenchmark("base Print", iterations, [&](Size) {
        Print(&writer, "%1:%2:%3:%4:%5:%6:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });
}

BENCHMARK_FUNCTION("base/MatchPathName")
{
    static const int iterations = 3000000;

#if defined(_WIN32)
    RunBenchmark("PathMatchSpecA", iterations, [&](Size) {
        PathMatchSpecA("aaa/bbb", "a*/*b");
    });
#endif

    RunBenchmark("fnmatch (musl)", iterations, [&](Size) {
        fnmatch("a*/*b", "aaa/bbb", FNM_PATHNAME);
    });

    RunBenchmark("MatchPathName", iterations, [&](Size) {
        MatchPathName("aaa/bbb", "a*/*b");
    });
}

BENCHMARK_FUNCTION("base/Random")
{
    static const int iterations = 5000000;

    srand(42);
    RunBenchmark("rand", iterations, [&](Size) {
        int x;

        do {
            x = rand();
        } while (x >= (RAND_MAX - RAND_MAX % 24096));

        x %= 24096;
    });

    FastRandom rng(42);
    RunBenchmark("FastRandom::GetInt", iterations, [&](Size) {
        rng.GetInt(0, 24096);
    });

    RunBenchmark("GetRandomInt", iterations, [&](Size) {
        GetRandomInt(0, 24096);
    });
}

BENCHMARK_FUNCTION("base/HashTable")
{
    static const int iterations = 4000000;

    HeapArray<std::string> keys;
    HeapArray<int> values;
    HeapArray<std::string> known;
    HeapArray<std::string> unknown;

    std::unordered_map<std::string, int> map1;
    HashMap<Span<const char>, int> map2;
    HashMap<const char *, int> map3;
    unsigned int sum = 0;

    for (int i = 0; i < iterations; i++) {
        char buf[32];
        Fmt(buf, "%1", FmtRandom(16));

        std::string key(buf);
        int value = GetRandomInt(0, 16);

        keys.Append(key);
        known.Append(key);
        values.Append(value);

        // Try to make sure the C string version is available
        (void)key.c_str();
    }

    for (int i = 0; i < iterations; i++) {
        char key[32]; Fmt(key, "%1", FmtRandom(16));
        unknown.Append(std::string(key));
    }

    FastRandomRNG<size_t> rng;
    std::shuffle(known.begin(), known.end(), rng);

    RunBenchmark("std::unordered_map (set)", iterations, [&](Size i) {
        map1[keys[i]] = values[i];
    });

    RunBenchmark("HashMap<Span> (set)", iterations, [&](Size i) {
        Span<const char> key = { keys[i].c_str(), (Size)keys[i].size() };
        map2.Set(key, values[i]);
    });

    RunBenchmark("HashMap<const char *> (set)", iterations, [&](Size i) {
        const char *key = keys[i].c_str();
        map3.Set(key, values[i]);
    });

    RunBenchmark("std::unordered_map (known)", iterations, [&](Size i) {
        const auto it = map1.find(known[i]);
        sum += (it != map1.end()) ? (unsigned int)it->second : 0;
    });

    RunBenchmark("HashMap<Span> (known)", iterations, [&](Size i) {
        Span<const char> key = { known[i].c_str(), (Size)known[i].size() };
        int *ptr = map2.Find(key);

        sum += ptr ? (unsigned int)*ptr : 0;
    });

    RunBenchmark("HashMap<const char *> (known)", iterations, [&](Size i) {
        const char *key = known[i].c_str();
        int *ptr = map3.Find(key);

        sum += ptr ? (unsigned int)*ptr : 0;
    });

    RunBenchmark("std::unordered_map (unknown)", iterations, [&](Size i) {
        const auto it = map1.find(unknown[i]);
        sum += (it != map1.end()) ? (unsigned int)it->second : 0;
    });

    RunBenchmark("HashMap<Span> (unknown)", iterations, [&](Size i) {
        Span<const char> key = { unknown[i].c_str(), (Size)unknown[i].size() };
        int *ptr = map2.Find(key);

        sum += ptr ? (unsigned int)*ptr : 0;
    });

    RunBenchmark("HashMap<const char *> (unknown)", iterations, [&](Size i) {
        const char *key = unknown[i].c_str();
        int *ptr = map3.Find(key);

        sum += ptr ? (unsigned int)*ptr : 0;
    });

    RunBenchmark("std::unordered_map (remove)", iterations, [&](Size i) {
        map1.erase(known[i]);
    });

    RunBenchmark("HashMap<Span> (remove)", iterations, [&](Size i) {
        Span<const char> key = { known[i].c_str(), (Size)known[i].size() };
        map2.Remove(key);
    });

    RunBenchmark("HashMap<const char *> (remove)", iterations, [&](Size i) {
        const char *key = known[i].c_str();
        map3.Remove(key);
    });
}

BENCHMARK_FUNCTION("base/ParseBool")
{
    static const int iterations = 4000000;

    bool yes = true;
    bool no = false;
    bool valid = true;

#define VALID(Str, Flags, Value, Remain) \
        do { \
            bool value; \
            Span<const char> remain; \
            valid &= ParseBool((Str), &value, (Flags), &remain); \
             \
            yes &= value; \
            no |= value; \
        } while (false)
#define INVALID(Str, Flags) \
        do { \
            bool value; \
            valid &= ParseBool((Str), &value, (Flags)); \
        } while (false)

    RunBenchmark("ParseBool", iterations, [&](Size) {
        VALID("1", (int)ParseFlag::End, true, 0);
        VALID("on", (int)ParseFlag::End, true, 0);
        VALID("y", (int)ParseFlag::End, true, 0);
        VALID("Yes", (int)ParseFlag::End, true, 0);
        VALID("true", (int)ParseFlag::End, true, 0);

        VALID("0", (int)ParseFlag::End, false, 0);
        VALID("off", (int)ParseFlag::End, false, 0);
        VALID("n", (int)ParseFlag::End, false, 0);
        VALID("no", (int)ParseFlag::End, false, 0);
        VALID("False", (int)ParseFlag::End, false, 0);

        VALID("true", (int)ParseFlag::End, true, 0);
        VALID("TrUe", (int)ParseFlag::End, true, 0);
        INVALID("trues", (int)ParseFlag::End);
        VALID("FALSE!", 0, false, 1);
        VALID("Y", (int)ParseFlag::End, true, 0);
        INVALID("YE", (int)ParseFlag::End);
        VALID("yes", 0, true, 0);
        VALID("yes!!!", 0, true, 3);
        VALID("n+", 0, false, 1);
        VALID("no+", 0, false, 1);
        INVALID("no+", (int)ParseFlag::End);
    });

#undef INVALID
#undef VALID
}

TEST_FUNCTION("crc/CRC32")
{
#define TEST_CRC(Str, Expected) \
        do { \
            Span<const char> span = (Str); \
            TEST_EQ(CRC32(0, span.As<const uint8_t>()), (Expected)); \
        } while (false)

    TEST_CRC("", 0u);
    TEST_CRC("123456789", 0xCBF43926u);
    TEST_CRC("Lorem ipsum dolor sit amet, consectetur adipiscing elit. In suscipit lacinia odio, ut maximus lorem aliquet vel. "
             "Fusce lacus sapien, interdum nec laoreet at, pretium vel tortor. Nunc id urna eget augue maximus pharetra vitae et quam. "
             "Suspendisse potenti. Praesent vitae maximus magna. Nunc tempor metus ipsum, eu venenatis metus cursus in. "
             "Donec rutrum sem a arcu pulvinar tristique. Nulla facilisi. Sed eu fringilla augue. Mauris tempus bibendum massa, eu euismod justo convallis eget. "
             "Morbi sit amet facilisis nunc, et pharetra nunc. Nullam gravida mi vitae mauris viverra, non accumsan ante egestas. "
             "Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.", 0x310BA7A4u);

#undef TEST_CRC
}

TEST_FUNCTION("crc/CRC32C")
{
#define TEST_CRC(Str, Expected) \
        do { \
            Span<const char> span = (Str); \
            TEST_EQ(CRC32C(0, span.As<const uint8_t>()), (Expected)); \
        } while (false)

    TEST_CRC("", 0u);
    TEST_CRC("123456789", 0xE3069283);
    TEST_CRC("Lorem ipsum dolor sit amet, consectetur adipiscing elit. In suscipit lacinia odio, ut maximus lorem aliquet vel. "
             "Fusce lacus sapien, interdum nec laoreet at, pretium vel tortor. Nunc id urna eget augue maximus pharetra vitae et quam. "
             "Suspendisse potenti. Praesent vitae maximus magna. Nunc tempor metus ipsum, eu venenatis metus cursus in. "
             "Donec rutrum sem a arcu pulvinar tristique. Nulla facilisi. Sed eu fringilla augue. Mauris tempus bibendum massa, eu euismod justo convallis eget. "
             "Morbi sit amet facilisis nunc, et pharetra nunc. Nullam gravida mi vitae mauris viverra, non accumsan ante egestas. "
             "Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.", 0x8B4AC0B7);

#undef TEST_CRC
}

TEST_FUNCTION("crc/CRC64xz")
{
#define TEST_CRC(Str, Expected) \
        do { \
            Span<const char> span = (Str); \
            TEST_EQ(CRC64xz(0, span.As<const uint8_t>()), (Expected)); \
        } while (false)

    TEST_CRC("", 0ull);
    TEST_CRC("123456789", 0x995DC9BBDF1939FAull);
    TEST_CRC("Lorem ipsum dolor sit amet, consectetur adipiscing elit. In suscipit lacinia odio, ut maximus lorem aliquet vel. "
             "Fusce lacus sapien, interdum nec laoreet at, pretium vel tortor. Nunc id urna eget augue maximus pharetra vitae et quam. "
             "Suspendisse potenti. Praesent vitae maximus magna. Nunc tempor metus ipsum, eu venenatis metus cursus in. "
             "Donec rutrum sem a arcu pulvinar tristique. Nulla facilisi. Sed eu fringilla augue. Mauris tempus bibendum massa, eu euismod justo convallis eget. "
             "Morbi sit amet facilisis nunc, et pharetra nunc. Nullam gravida mi vitae mauris viverra, non accumsan ante egestas. "
             "Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.", 0x20C36CB9E094C3A8ull);

#undef TEST_CRC
}

TEST_FUNCTION("crc/CRC64nvme")
{
#define TEST_CRC(Str, Expected) \
        do { \
            Span<const char> span = (Str); \
            TEST_EQ(CRC64nvme(0, span.As<const uint8_t>()), (Expected)); \
        } while (false)

    TEST_CRC("", 0ull);
    TEST_CRC("123456789", 0xAE8B14860A799888ull);
    TEST_CRC("Lorem ipsum dolor sit amet, consectetur adipiscing elit. In suscipit lacinia odio, ut maximus lorem aliquet vel. "
             "Fusce lacus sapien, interdum nec laoreet at, pretium vel tortor. Nunc id urna eget augue maximus pharetra vitae et quam. "
             "Suspendisse potenti. Praesent vitae maximus magna. Nunc tempor metus ipsum, eu venenatis metus cursus in. "
             "Donec rutrum sem a arcu pulvinar tristique. Nulla facilisi. Sed eu fringilla augue. Mauris tempus bibendum massa, eu euismod justo convallis eget. "
             "Morbi sit amet facilisis nunc, et pharetra nunc. Nullam gravida mi vitae mauris viverra, non accumsan ante egestas. "
             "Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.", 0xDA3CA874A87E0AC1ull);

#undef TEST_CRC
}

}
