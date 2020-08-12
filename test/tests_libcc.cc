// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

void TestMatchPathName()
{
    TEST_FUNCTION("MatchPathName");

#define CHECK_PATH_SPEC(Pattern, Path, Expected) \
        TEST(MatchPathName(Path, Pattern) == Expected)

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

void TestOptionParser()
{
    TEST_FUNCTION("OptionParser");

    // Empty

    {
        OptionParser opt({});

        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    // Short options

    {
        const char *args[] = {"-f"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "-f");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    {
        const char *args[] = {"-foo", "-b"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "-b");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    // Long options

    {
        const char *args[] = {"--foobar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foobar");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    {
        const char *args[] = {"--foo", "--bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST_STR(opt.Next(), "--bar");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    // Mixed tests

    {
        const char *args[] = {"--foo", "-bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST_STR(opt.Next(), "-b");
        TEST_STR(opt.Next(), "-a");
        TEST_STR(opt.Next(), "-r");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
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
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    // Values

    {
        const char *args[] = {"-f", "bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    {
        const char *args[] = {"-fbar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    {
        const char *args[] = {"--foo=bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    {
        const char *args[] = {"--foo", "bar"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    {
        const char *args[] = {"bar", "--foo"};
        OptionParser opt(args);

        TEST_STR(opt.Next(), "--foo");
        TEST(!opt.ConsumeValue());
        TEST(!opt.Next());
        TEST_STR(opt.ConsumeNonOption(), "bar");
        TEST(!opt.ConsumeNonOption());
    }

    // Positional tests

    {
        const char *args[] = {"foo", "bar"};
        OptionParser opt(args);

        TEST_STR(opt.ConsumeNonOption(), "foo");
        TEST_STR(opt.ConsumeNonOption(), "bar");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    {
        const char *args[] = {"foo", "--foobar", "bar"};
        OptionParser opt(args);

        opt.Next();
        opt.Next();
        TEST_STR(opt.ConsumeNonOption(), "foo");
        TEST_STR(opt.ConsumeNonOption(), "bar");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    {
        const char *args[] = {"foobar", "--", "foo", "--bar"};
        OptionParser opt(args);

        opt.Next();
        TEST_STR(opt.ConsumeNonOption(), "foobar");
        TEST_STR(opt.ConsumeNonOption(), "foo");
        TEST_STR(opt.ConsumeNonOption(), "--bar");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
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
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }

    // ConsumeNonOption

    {
        const char *args[] = {"foo", "-f", "bar"};
        OptionParser opt(args);

        TEST_STR(opt.ConsumeNonOption(), "foo");
        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.ConsumeNonOption(), "bar");
        TEST(!opt.Next());
    }

    {
        const char *args[] = {"bar1", "-foo", "bar2"};
        OptionParser opt(args);

        TEST_STR(opt.ConsumeNonOption(), "bar1");
        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.ConsumeNonOption(), "bar2");
        TEST(!opt.Next());
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
        TEST(!opt.ConsumeValue());
        TEST_STR(opt.Next(), "-o");
        TEST_STR(opt.Next(), "-2");
        TEST_STR(opt.Next(), "--foo3");
        TEST_STR(opt.ConsumeValue(), "BAR");
        TEST_STR(opt.Next(), "-f");
        TEST_STR(opt.ConsumeValue(), "bar");
        TEST(!opt.Next());
        TEST_STR(opt.ConsumeNonOption(), "fooBAR");
        TEST_STR(opt.ConsumeNonOption(), "FOOBAR");
        TEST_STR(opt.ConsumeNonOption(), "--");
        TEST(!opt.Next());
        TEST_STR(opt.ConsumeNonOption(), "--FOOBAR");
        TEST(!opt.Next());
        TEST(!opt.ConsumeNonOption());
    }
}

void BenchFmt()
{
    PrintLn("  Fmt");

    static const int iterations = 1600000;

#ifdef _WIN32
    FILE *fp = OpenFile("NUL", OpenFileMode::Write);
#else
    FILE *fp = OpenFile("/dev/null", OpenFileMode::Write);
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
        stbsp_snprintf(buf, RG_SIZE(buf), "%d:%d:%d:%s:%p:%c:%%\n", 1234, 42, -313.3, "str", (void*)1000, 'X');
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

void BenchMatchPathName()
{
    PrintLn("  MatchPathName");

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

}
