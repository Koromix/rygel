// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
#include "test.hh"

#include <string>
#include <unordered_map>
#if !defined(_WIN32)
    #include <fnmatch.h>
#endif

// Comparative benchmarks
#if defined(_WIN32)
    extern "C" __declspec(dllimport) int __stdcall PathMatchSpecA(const char *pszFile, const char *pszSpec);
#endif
#include "musl/fnmatch.h"
#include "vendor/stb/stb_sprintf.h"
#include "vendor/fmt/include/fmt/format.h"
#include "vendor/fmt/include/fmt/compile.h"

namespace K {

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
TEST_FUNCTION("base/CmpStr")
{
    TEST_GT(CmpStr("b", "a"), 0);
    TEST_LT(CmpStr("a", "b"), 0);
    TEST_LT(CmpStr("aa", "b"), 0);
    TEST_GT(CmpStr("abc", "ab"), 0);
    TEST_LT(CmpStr("ab", "abc"), 0);

    TEST_GT(CmpStr("10", "1"), 0);
    TEST_LT(CmpStr("10", "2"), 0);
    TEST_LT(CmpStr("ab-10", "ab-2"), 0);
    TEST_LT(CmpStr("ab-10", "ac-10"), 0);

    TEST_LT(CmpStr("1", "10"), 0);
    TEST_GT(CmpStr("2", "10"), 0);
    TEST_GT(CmpStr("ab-2", "ab-10"), 0);
    TEST_GT(CmpStr("ac-10", "ab-10"), 0);

    TEST_LT(CmpStr("00001", "02"), 0);
    TEST_LT(CmpStr("00002", "02"), 0);
    TEST_LT(CmpStr("00003", "02"), 0);
    TEST_LT(CmpStr("P00001", "P02"), 0);
    TEST_LT(CmpStr("P00002", "P02"), 0);
    TEST_LT(CmpStr("P00003", "P02"), 0);

    TEST_EQ(CmpStr("01", "01"), 0);
    TEST_EQ(CmpStr("02", "02"), 0);
    TEST_EQ(CmpStr("01ab", "01ab"), 0);
    TEST_LT(CmpStr("01ab", "01ac"), 0);
    TEST_GT(CmpStr("01ac", "01ab"), 0);

    TEST_GT(CmpStr("20", "10"), 0);
    TEST_LT(CmpStr("10", "20"), 0);
    TEST_GT(CmpStr("X20", "X10"), 0);
    TEST_LT(CmpStr("X10", "X20"), 0);
}

TEST_FUNCTION("base/CmpNatural")
{
    TEST_GT(CmpNatural("b", "a"), 0);
    TEST_LT(CmpNatural("a", "b"), 0);
    TEST_LT(CmpNatural("aa", "b"), 0);
    TEST_GT(CmpNatural("abc", "ab"), 0);
    TEST_LT(CmpNatural("ab", "abc"), 0);

    TEST_GT(CmpNatural("10", "1"), 0);
    TEST_GT(CmpNatural("10", "2"), 0);
    TEST_GT(CmpNatural("ab-10", "ab-2"), 0);
    TEST_LT(CmpNatural("ab-10", "ac-10"), 0);

    TEST_LT(CmpNatural("1", "10"), 0);
    TEST_LT(CmpNatural("2", "10"), 0);
    TEST_LT(CmpNatural("ab-2", "ab-10"), 0);
    TEST_GT(CmpNatural("ac-10", "ab-10"), 0);

    TEST_LT(CmpNatural("00001", "02"), 0);
    TEST_EQ(CmpNatural("00002", "02"), 0);
    TEST_GT(CmpNatural("00003", "02"), 0);
    TEST_LT(CmpNatural("P00001", "P02"), 0);
    TEST_EQ(CmpNatural("P00002", "P02"), 0);
    TEST_GT(CmpNatural("P00003", "P02"), 0);
    TEST_EQ(CmpNatural("02", "00002"), 0);

    TEST_EQ(CmpNatural("01", "01"), 0);
    TEST_EQ(CmpNatural("02", "02"), 0);
    TEST_EQ(CmpNatural("01ab", "01ab"), 0);
    TEST_LT(CmpNatural("01ab", "01ac"), 0);
    TEST_GT(CmpNatural("01ac", "01ab"), 0);

    TEST_GT(CmpNatural("20", "10"), 0);
    TEST_GT(CmpNatural("20", "11"), 0);
    TEST_GT(CmpNatural("20", "12"), 0);
    TEST_GT(CmpNatural("22", "12"), 0);
    TEST_GT(CmpNatural("23", "12"), 0);
    TEST_LT(CmpNatural("10", "20"), 0);
    TEST_LT(CmpNatural("11", "20"), 0);
    TEST_LT(CmpNatural("12", "20"), 0);
    TEST_LT(CmpNatural("12", "22"), 0);
    TEST_LT(CmpNatural("12", "23"), 0);
    TEST_GT(CmpNatural("X20", "X10"), 0);
    TEST_GT(CmpNatural("X20", "X12"), 0);
    TEST_LT(CmpNatural("X10", "X20"), 0);
    TEST_LT(CmpNatural("X12", "X20"), 0);

    TEST_GT(CmpNatural("00002t", "02s"), 0);
    TEST_LT(CmpNatural("00002s", "02t"), 0);
    TEST_LT(CmpNatural("02s", "00002t"), 0);
    TEST_GT(CmpNatural("02t", "00002s"), 0);
}

TEST_FUNCTION("base/ParseBool")
{
    PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
    K_DEFER { PopLogFilter(); };

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

    VALID("1", K_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("on", K_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("y", K_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("yes", K_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("true", K_DEFAULT_PARSE_FLAGS, true, 0);

    VALID("0", K_DEFAULT_PARSE_FLAGS, false, 0);
    VALID("off", K_DEFAULT_PARSE_FLAGS, false, 0);
    VALID("n", K_DEFAULT_PARSE_FLAGS, false, 0);
    VALID("no", K_DEFAULT_PARSE_FLAGS, false, 0);
    VALID("false", K_DEFAULT_PARSE_FLAGS, false, 0);

    VALID("true", K_DEFAULT_PARSE_FLAGS, true, 0);
    VALID("TrUe", K_DEFAULT_PARSE_FLAGS, true, 0);
    INVALID("trues", K_DEFAULT_PARSE_FLAGS);
    VALID("FALSE!", 0, false, 1);
    VALID("Y", K_DEFAULT_PARSE_FLAGS, true, 0);
    INVALID("YE", K_DEFAULT_PARSE_FLAGS);
    VALID("yes", 0, true, 0);
    VALID("yes!!!", 0, true, 3);
    VALID("n+", 0, false, 1);
    VALID("no+", 0, false, 1);
    INVALID("no+", K_DEFAULT_PARSE_FLAGS);

#undef INVALID
#undef VALID
}

TEST_FUNCTION("base/ParseSize")
{
    PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
    K_DEFER { PopLogFilter(); };

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

    VALID("1", K_DEFAULT_PARSE_FLAGS, 1, 0);
    VALID("2147483648", K_DEFAULT_PARSE_FLAGS, 2147483648, 0);
    VALID("4294967295", K_DEFAULT_PARSE_FLAGS, 4294967295, 0);
    INVALID("1S", K_DEFAULT_PARSE_FLAGS);

    VALID("4B", K_DEFAULT_PARSE_FLAGS, 4, 0);
    VALID("4k", K_DEFAULT_PARSE_FLAGS, 4000, 0);
    VALID("4M", K_DEFAULT_PARSE_FLAGS, 4000000, 0);
    VALID("4G", K_DEFAULT_PARSE_FLAGS, 4000000000, 0);
    VALID("4T", K_DEFAULT_PARSE_FLAGS, 4000000000000ll, 0);
    VALID("4s", 0, 4, 1);
    INVALID("4s", K_DEFAULT_PARSE_FLAGS);

    VALID("4G", K_DEFAULT_PARSE_FLAGS, 4000000000, 0);
    VALID("4Gi", 0, 4000000000, 1);
    INVALID("4Gi", K_DEFAULT_PARSE_FLAGS);

#undef INVALID
#undef VALID
}

TEST_FUNCTION("base/ParseDuration")
{
    PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
    K_DEFER { PopLogFilter(); };

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

    VALID("1", K_DEFAULT_PARSE_FLAGS, 1000, 0);
    VALID("300", K_DEFAULT_PARSE_FLAGS, 300000, 0);
    INVALID("1p", K_DEFAULT_PARSE_FLAGS);

    VALID("4s", K_DEFAULT_PARSE_FLAGS, 4000, 0);
    VALID("4m", K_DEFAULT_PARSE_FLAGS, 4000 * 60, 0);
    VALID("4h", K_DEFAULT_PARSE_FLAGS, 4000 * 3600, 0);
    VALID("4d", K_DEFAULT_PARSE_FLAGS, 4000 * 86400, 0);
    VALID("4w", 0, 4000, 1);
    INVALID("4w", K_DEFAULT_PARSE_FLAGS);

    VALID("4d", K_DEFAULT_PARSE_FLAGS, 4000 * 86400, 0);
    VALID("4dt", 0, 4000 * 86400, 1);
    INVALID("4dt", K_DEFAULT_PARSE_FLAGS);

#undef INVALID
#undef VALID
}

TEST_FUNCTION("base/ChaCha20")
{
    struct TestCase {
        uint8_t key[32];
        uint8_t iv[8];
        uint64_t counter;
        int len;
        uint8_t stream[1024];
    };

    // Got these from RFC 7539: https://datatracker.ietf.org/doc/rfc7539/
    static const TestCase tests[] = {
        {
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            0, 64,
            { 0x76, 0xB8, 0xE0, 0xAD, 0xA0, 0xF1, 0x3D, 0x90, 0x40, 0x5D, 0x6A, 0xE5, 0x53, 0x86, 0xBD, 0x28, 0xBD, 0xD2, 0x19, 0xB8, 0xA0, 0x8D, 0xED, 0x1A, 0xA8, 0x36, 0xEF, 0xCC, 0x8B, 0x77, 0x0D, 0xC7, 0xDA, 0x41, 0x59, 0x7C, 0x51, 0x57, 0x48, 0x8D, 0x77, 0x24, 0xE0, 0x3F, 0xB8, 0xD8, 0x4A, 0x37, 0x6A, 0x43, 0xB8, 0xF4, 0x15, 0x18, 0xA1, 0x1C, 0xC3, 0x87, 0xB6, 0x69, 0xB2, 0xEE, 0x65, 0x86 }
        },
        {
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            0, 64,
            { 0x45, 0x40, 0xF0, 0x5A, 0x9F, 0x1F, 0xB2, 0x96, 0xD7, 0x73, 0x6E, 0x7B, 0x20, 0x8E, 0x3C, 0x96, 0xEB, 0x4F, 0xE1, 0x83, 0x46, 0x88, 0xD2, 0x60, 0x4F, 0x45, 0x09, 0x52, 0xED, 0x43, 0x2D, 0x41, 0xBB, 0xE2, 0xA0, 0xB6, 0xEA, 0x75, 0x66, 0xD2, 0xA5, 0xD1, 0xE7, 0xE2, 0x0D, 0x42, 0xAF, 0x2C, 0x53, 0xD7, 0x92, 0xB1, 0xC4, 0x3F, 0xEA, 0x81, 0x7E, 0x9A, 0xD2, 0x75, 0xAE, 0x54, 0x69, 0x63 }
        },
        {
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            1, 64,
            { 0x9F, 0x07, 0xE7, 0xBE, 0x55, 0x51, 0x38, 0x7A, 0x98, 0xBA, 0x97, 0x7C, 0x73, 0x2D, 0x08, 0x0D, 0xCB, 0x0F, 0x29, 0xA0, 0x48, 0xE3, 0x65, 0x69, 0x12, 0xC6, 0x53, 0x3E, 0x32, 0xEE, 0x7A, 0xED, 0x29, 0xB7, 0x21, 0x76, 0x9C, 0xE6, 0x4E, 0x43, 0xD5, 0x71, 0x33, 0xB0, 0x74, 0xD8, 0x39, 0xD5, 0x31, 0xED, 0x1F, 0x28, 0x51, 0x0A, 0xFB, 0x45, 0xAC, 0xE1, 0x0A, 0x1F, 0x4B, 0x79, 0x4D, 0x6F }
        },
        {
            { 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            2, 64,
            { 0x72, 0xD5, 0x4D, 0xFB, 0xF1, 0x2E, 0xC4, 0x4B, 0x36, 0x26, 0x92, 0xDF, 0x94, 0x13, 0x7F, 0x32, 0x8F, 0xEA, 0x8D, 0xA7, 0x39, 0x90, 0x26, 0x5E, 0xC1, 0xBB, 0xBE, 0xA1, 0xAE, 0x9A, 0xF0, 0xCA, 0x13, 0xB2, 0x5A, 0xA2, 0x6C, 0xB4, 0xA6, 0x48, 0xCB, 0x9B, 0x9D, 0x1B, 0xE6, 0x5B, 0x2C, 0x09, 0x24, 0xA6, 0x6C, 0x54, 0xD5, 0x45, 0xEC, 0x1B, 0x73, 0x74, 0xF4, 0x87, 0x2E, 0x99, 0xF0, 0x96 }
        },
        {
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
            0, 60,
            { 0xDE, 0x9C, 0xBA, 0x7B, 0xF3, 0xD6, 0x9E, 0xF5, 0xE7, 0x86, 0xDC, 0x63, 0x97, 0x3F, 0x65, 0x3A, 0x0B, 0x49, 0xE0, 0x15, 0xAD, 0xBF, 0xF7, 0x13, 0x4F, 0xCB, 0x7D, 0xF1, 0x37, 0x82, 0x10, 0x31, 0xE8, 0x5A, 0x05, 0x02, 0x78, 0xA7, 0x08, 0x45, 0x27, 0x21, 0x4F, 0x73, 0xEF, 0xC7, 0xFA, 0x5B, 0x52, 0x77, 0x06, 0x2E, 0xB7, 0xA0, 0x43, 0x3E, 0x44, 0x5F, 0x41, 0xE3 }
        },
        {
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            0, 64,
            { 0xEF, 0x3F, 0xDF, 0xD6, 0xC6, 0x15, 0x78, 0xFB, 0xF5, 0xCF, 0x35, 0xBD, 0x3D, 0xD3, 0x3B, 0x80, 0x09, 0x63, 0x16, 0x34, 0xD2, 0x1E, 0x42, 0xAC, 0x33, 0x96, 0x0B, 0xD1, 0x38, 0xE5, 0x0D, 0x32, 0x11, 0x1E, 0x4C, 0xAF, 0x23, 0x7E, 0xE5, 0x3C, 0xA8, 0xAD, 0x64, 0x26, 0x19, 0x4A, 0x88, 0x54, 0x5D, 0xDC, 0x49, 0x7A, 0x0B, 0x46, 0x6E, 0x7D, 0x6B, 0xBD, 0xB0, 0x04, 0x1B, 0x2F, 0x58, 0x6B }
        },
        {
            { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F },
            { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 },
            0, 256,
            { 0xF7, 0x98, 0xA1, 0x89, 0xF1, 0x95, 0xE6, 0x69, 0x82, 0x10, 0x5F, 0xFB, 0x64, 0x0B, 0xB7, 0x75, 0x7F, 0x57, 0x9D, 0xA3, 0x16, 0x02, 0xFC, 0x93, 0xEC, 0x01, 0xAC, 0x56, 0xF8, 0x5A, 0xC3, 0xC1, 0x34, 0xA4, 0x54, 0x7B, 0x73, 0x3B, 0x46, 0x41, 0x30, 0x42, 0xC9, 0x44, 0x00, 0x49, 0x17, 0x69, 0x05, 0xD3, 0xBE, 0x59, 0xEA, 0x1C, 0x53, 0xF1, 0x59, 0x16, 0x15, 0x5C, 0x2B, 0xE8, 0x24, 0x1A, 0x38, 0x00, 0x8B, 0x9A, 0x26, 0xBC, 0x35, 0x94, 0x1E, 0x24, 0x44, 0x17, 0x7C, 0x8A, 0xDE, 0x66, 0x89, 0xDE, 0x95, 0x26, 0x49, 0x86, 0xD9, 0x58, 0x89, 0xFB, 0x60, 0xE8, 0x46, 0x29, 0xC9, 0xBD, 0x9A, 0x5A, 0xCB, 0x1C, 0xC1, 0x18, 0xBE, 0x56, 0x3E, 0xB9, 0xB3, 0xA4, 0xA4, 0x72, 0xF8, 0x2E, 0x09, 0xA7, 0xE7, 0x78, 0x49, 0x2B, 0x56, 0x2E, 0xF7, 0x13, 0x0E, 0x88, 0xDF, 0xE0, 0x31, 0xC7, 0x9D, 0xB9, 0xD4, 0xF7, 0xC7, 0xA8, 0x99, 0x15, 0x1B, 0x9A, 0x47, 0x50, 0x32, 0xB6, 0x3F, 0xC3, 0x85, 0x24, 0x5F, 0xE0, 0x54, 0xE3, 0xDD, 0x5A, 0x97, 0xA5, 0xF5, 0x76, 0xFE, 0x06, 0x40, 0x25, 0xD3, 0xCE, 0x04, 0x2C, 0x56, 0x6A, 0xB2, 0xC5, 0x07, 0xB1, 0x38, 0xDB, 0x85, 0x3E, 0x3D, 0x69, 0x59, 0x66, 0x09, 0x96, 0x54, 0x6C, 0xC9, 0xC4, 0xA6, 0xEA, 0xFD, 0xC7, 0x77, 0xC0, 0x40, 0xD7, 0x0E, 0xAF, 0x46, 0xF7, 0x6D, 0xAD, 0x39, 0x79, 0xE5, 0xC5, 0x36, 0x0C, 0x33, 0x17, 0x16, 0x6A, 0x1C, 0x89, 0x4C, 0x94, 0xA3, 0x71, 0x87, 0x6A, 0x94, 0xDF, 0x76, 0x28, 0xFE, 0x4E, 0xAA, 0xF2, 0xCC, 0xB2, 0x7D, 0x5A, 0xAA, 0xE0, 0xAD, 0x7A, 0xD0, 0xF9, 0xD4, 0xB6, 0xAD, 0x3B, 0x54, 0x09, 0x87, 0x46, 0xD4, 0x52, 0x4D, 0x38, 0x40, 0x7A, 0x6D, 0xEB, 0x3A, 0xB7, 0x8F, 0xAB, 0x78, 0xC9 }
        }
    };

    for (const TestCase &test: tests) {
        uint32_t state[16];
        uint8_t stream[1024];

        uint64_t counter = LittleEndian(test.counter);
        InitChaCha20(state, test.key, test.iv, (const uint8_t *)&counter);

        for (Size i = 0; i < test.len; i += 64) {
            RunChaCha20(state, stream + i);
        }

        TEST(!memcmp(stream, test.stream, test.len));
    }
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

    K_HASHTABLE_HANDLER(IntBucket, key);
};

struct StrBucket {
    const char *key;
    int value;

    K_HASHTABLE_HANDLER(StrBucket, key);
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
    K_ASSERT(fp);
    K_DEFER { fclose(fp); };

    StreamWriter writer(fd, "/dev/null");
    K_ASSERT(writer.IsValid());

    RunBenchmark("printf", iterations, [&](Size) {
        fprintf(fp, "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });

    RunBenchmark("snprintf", iterations, [&](Size) {
        char buf[1024];
        snprintf(buf, K_SIZE(buf), "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
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
        stbsp_snprintf(buf, K_SIZE(buf), "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
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
#else
    RunBenchmark("fnmatch", iterations, [&](Size) {
        fnmatch("a*/*b", "aaa/bbb", FNM_PATHNAME);
    });
#endif

    RunBenchmark("fnmatch (musl)", iterations, [&](Size) {
        musl_fnmatch("a*/*b", "aaa/bbb", MUSL_FNM_PATHNAME);
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

TEST_FUNCTION("base/CRC32")
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

TEST_FUNCTION("base/CRC32C")
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

TEST_FUNCTION("base/CRC64xz")
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

TEST_FUNCTION("base/CRC64nvme")
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
