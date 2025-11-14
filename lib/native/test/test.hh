// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

struct TestInfo {
    const char *path;
    void (*func)(Size *out_total, Size *out_failures);

    TestInfo(const char *path, void (*func)(Size *out_total, Size *out_failures));
};

struct BenchmarkInfo {
    const char *path;
    void (*func)();

    BenchmarkInfo(const char *path, void (*func)());
};

#define TEST_FUNCTION_(FuncName, VarName, Path) \
    static void FuncName(Size *out_total, Size *out_failures); \
     \
    static const TestInfo VarName((Path), FuncName); \
     \
    static void FuncName(Size *out_total, Size *out_failures)
#define TEST_FUNCTION(Path) TEST_FUNCTION_(K_UNIQUE_NAME(func_), K_UNIQUE_NAME(test_), "test/" Path)

#define TEST_EX(Condition, ...) \
    do { \
        (*out_total)++; \
        if (!(Condition)) { \
            Print("\n    %!D..[%1:%2]%!0 ", SplitStrReverseAny(__FILE__, K_PATH_SEPARATORS), __LINE__); \
            Print(__VA_ARGS__); \
            (*out_failures)++; \
        } \
    } while (false)

#define TEST(Condition) \
    TEST_EX((Condition), "%1", K_STRINGIFY(Condition))
#define TEST_EQ(Value1, Value2) \
    do { \
        auto value1 = (Value1); \
        auto value2 = (Value2); \
        \
        TEST_EX(value1 == value2, "%1: %2 == %3", K_STRINGIFY(Value1), value1, value2); \
    } while (false)
#define TEST_GT(Value1, Value2) \
    do { \
        auto value1 = (Value1); \
        auto value2 = (Value2); \
        \
        TEST_EX(value1 > value2, "%1: %2 > %3", K_STRINGIFY(Value1), value1, value2); \
    } while (false)
#define TEST_LT(Value1, Value2) \
    do { \
        auto value1 = (Value1); \
        auto value2 = (Value2); \
        \
        TEST_EX(value1 < value2, "%1: %2 < %3", K_STRINGIFY(Value1), value1, value2); \
    } while (false)
#define TEST_STR(Str1, Str2) \
    do { \
        Span<const char> str1 = (Str1); \
        Span<const char> str2 = (Str2); \
        \
        if (!str1.ptr) { \
            str1 = "(null)"; \
        } \
        if (!str2.ptr) { \
            str2 = "(null)"; \
        } \
        \
        TEST_EX(str1 == str2, "%1: '%2' == '%3'", K_STRINGIFY(Str1), str1, str2); \
    } while (false)

#define BENCHMARK_FUNCTION_(FuncName, VarName, Path) \
    static void FuncName(); \
     \
    static const BenchmarkInfo VarName((Path), FuncName); \
     \
    static void FuncName()
#define BENCHMARK_FUNCTION(Path) BENCHMARK_FUNCTION_(K_UNIQUE_NAME(func_), K_UNIQUE_NAME(bench_), "bench/" Path)

static inline void RunBenchmark(const char *name, Size iterations, FunctionRef<void(Size)> func)
{
    Print("  %!..+%1%!0", FmtPad(name, 34));
    StdOut->Flush();

    int64_t time = GetMonotonicTime();
    int64_t clock = GetClockCounter();

    for (Size i = 0; i < iterations; i++) {
        func(i);
#if defined(__clang__) || !defined(_MSC_VER)
        __asm__ __volatile__("" : : : "memory");
#endif
    }

    time = GetMonotonicTime() - time;
    clock = GetClockCounter() - clock;

    PrintLn(" %!c..%1 ms%!0 (%2 cycles per iteration)", time, clock / iterations);
}

}
