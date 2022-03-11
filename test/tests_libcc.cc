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

// Comparative benchmarks
#ifdef _WIN32
    extern "C" __declspec(dllimport) int __stdcall PathMatchSpecA(const char *pszFile, const char *pszSpec);
#endif
#include "vendor/musl_fnmatch.h"
#include "vendor/stb_sprintf.h"
#include "vendor/fmt/include/fmt/format.h"
#include "vendor/fmt/include/fmt/compile.h"

namespace RG {

TEST_FUNCTION("libcc/TestFormatDouble")
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

TEST_FUNCTION("libcc/TestFormatSize")
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

TEST_FUNCTION("libcc/TestMatchPathName")
{
#define CHECK_PATH_SPEC(Pattern, Path, Expected) \
        TEST_EQ(MatchPathName((Path), (Pattern)), (Expected))

    // Stolen from FreeBSD
    CHECK_PATH_SPEC("", "", true);
    CHECK_PATH_SPEC("a", "a", true);
    CHECK_PATH_SPEC("a", "b", false);
#ifdef _WIN32
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

TEST_FUNCTION("libcc/TestFastRandom")
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
}

TEST_FUNCTION("libcc/TestGetRandomIntSafe")
{
    static const int iterations = 100;
    static const int upper = 2000;
    static const int loop = 100000;

    bool varied = true;

    for (int i = 0; i < iterations; i++) {
        int max = GetRandomIntSafe(100, upper);

        TEST(max >= 100);
        TEST(max < upper);

        int distrib = 0;
        bool memory[upper] = {};

        for (int j = 0; j < loop; j++) {
            int rnd = GetRandomIntSafe(0, max);

            TEST(rnd >= 0);
            TEST(rnd < max);

            distrib += !memory[rnd];
            memory[rnd] = true;
        }

        varied &= (distrib > 95 * max / 100);
    }

    TEST_EX(varied, "GetRandomIntSafe() values look well distributed");
}

TEST_FUNCTION("libcc/TestOptionParser")
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
}

TEST_FUNCTION("libcc/TestPathCheck")
{
    TEST_EQ(PathIsAbsolute("foo"), false);
    TEST_EQ(PathIsAbsolute(""), false);
    TEST_EQ(PathIsAbsolute("/foo"), true);
    TEST_EQ(PathIsAbsolute("/"), true);
#ifdef _WIN32
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
#ifdef _WIN32
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

BENCHMARK_FUNCTION("libcc/BenchFmt")
{
    static const int iterations = 1600000;

#ifdef _WIN32
    FILE *fp = fopen("\\\\.\\NUL", "wb");
#else
    FILE *fp = OpenFile("/dev/null", (int)OpenFileFlag::Write);
#endif
    RG_ASSERT(fp);
    RG_DEFER { fclose(fp); };

    RunBenchmark("printf", iterations, [&]() {
        fprintf(fp, "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });

    RunBenchmark("snprintf", iterations, [&]() {
        char buf[1024];
        snprintf(buf, RG_SIZE(buf), "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });

#ifndef _WIN32
    RunBenchmark("asprintf", iterations, [&]() {
        char *ret = nullptr;
        asprintf(&ret, "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
        free(ret);
    });
#endif

    RunBenchmark("stbsp_snprintf", iterations, [&]() {
        char buf[1024];
        stbsp_snprintf(buf, RG_SIZE(buf), "%d:%d:%g:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });

    RunBenchmark("fmt::format_to", iterations, [&]() {
        char buf[1024];
        fmt::format_to(buf, "{}:{}:{}:{}:{}:{}%\n", 1234, 42, -313.3, "str", (void *)1000, 'X');
    });

    RunBenchmark("fmt::format_to (FMT_COMPILE)", iterations, [&]() {
        char buf[1024];
        fmt::format_to(buf, FMT_COMPILE("{}:{}:{}:{}:{}:{}%\n"), 1234, 42, -313.3, "str", (void *)1000, 'X');
    });

    RunBenchmark("libcc Fmt", iterations, [&]() {
        LocalArray<char, 1024> buf;
        buf.len = Fmt(buf.data, "%1:%2:%3:%4:%5:%6:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X').len;
    });

    RunBenchmark("libcc Fmt (allocator)", iterations, [&]() {
        LinkedAllocator temp_alloc;
        Fmt(&temp_alloc, "%1:%2:%3:%4:%5:%6:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });

    RunBenchmark("libcc Fmt (heap)", iterations, [&]() {
        HeapArray<char> buf;
        Fmt(&buf, "%1:%2:%3:%4:%5:%6:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
        buf.RemoveFrom(0);
    });

    RunBenchmark("libcc Print", iterations, [&]() {
        Print(fp, "%1:%2:%3:%4:%5:%6:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
    });
}

BENCHMARK_FUNCTION("libcc/BenchMatchPathName")
{
    static const int iterations = 3000000;

#ifdef _WIN32
    RunBenchmark("PathMatchSpecA", iterations, [&]() {
        PathMatchSpecA("aaa/bbb", "a*/*b");
    });
#endif

    RunBenchmark("fnmatch (musl)", iterations, [&]() {
        fnmatch("a*/*b", "aaa/bbb", FNM_PATHNAME);
    });

    RunBenchmark("MatchPathName", iterations, [&]() {
        MatchPathName("aaa/bbb", "a*/*b");
    });
}

BENCHMARK_FUNCTION("libcc/BenchRandom")
{
    static const int iterations = 10000000;

    FastRandom rng(42);
    RunBenchmark("FastRandom::GetInt", iterations, [&]() {
        rng.GetInt(1, 24097);
    });

    RunBenchmark("GetRandomIntSafe", iterations, [&]() {
        GetRandomIntSafe(1, 24097);
    });
}

}
