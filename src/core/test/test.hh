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

#pragma once

#include "src/core/base/base.hh"

namespace RG {

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
#define TEST_FUNCTION(Path) TEST_FUNCTION_(RG_UNIQUE_NAME(func_), RG_UNIQUE_NAME(test_), "test/" Path)

#define TEST_EX(Condition, ...) \
    do { \
        (*out_total)++; \
        if (!(Condition)) { \
            Print("\n    %!D..[%1:%2]%!0 ", SplitStrReverseAny(__FILE__, RG_PATH_SEPARATORS), __LINE__); \
            Print(__VA_ARGS__); \
            (*out_failures)++; \
        } \
    } while (false)

#define TEST(Condition) \
    TEST_EX((Condition), "%1", RG_STRINGIFY(Condition))
#define TEST_EQ(Value1, Value2) \
    do { \
        auto value1 = (Value1); \
        auto value2 = (Value2); \
        \
        TEST_EX(value1 == value2, "%1: %2 == %3", RG_STRINGIFY(Value1), value1, value2); \
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
        TEST_EX(str1 == str2, "%1: '%2' == '%3'", RG_STRINGIFY(Str1), str1, str2); \
    } while (false)

#define BENCHMARK_FUNCTION_(FuncName, VarName, Path) \
    static void FuncName(); \
     \
    static const BenchmarkInfo VarName((Path), FuncName); \
     \
    static void FuncName()
#define BENCHMARK_FUNCTION(Path) BENCHMARK_FUNCTION_(RG_UNIQUE_NAME(func_), RG_UNIQUE_NAME(bench_), "bench/" Path)

static inline void RunBenchmark(const char *name, Size iterations, FunctionRef<void(Size)> func)
{
    Print("  %!..+%1%!0", FmtArg(name).Pad(34));
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
