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

#pragma once

#include "src/core/libcc/libcc.hh"

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
#define TEST_FUNCTION(Path) TEST_FUNCTION_(RG_UNIQUE_NAME(func_), RG_UNIQUE_NAME(test_), (Path))

#define TEST_EX(Condition, ...) \
    do { \
        (*out_total)++; \
        if (!(Condition)) { \
            Print(stderr, "\n    %!D..[%1:%2]%!0 ", SplitStrReverseAny(__FILE__, RG_PATH_SEPARATORS), __LINE__); \
            Print(stderr, __VA_ARGS__); \
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
#define BENCHMARK_FUNCTION(Path) BENCHMARK_FUNCTION_(RG_UNIQUE_NAME(func_), RG_UNIQUE_NAME(bench_), (Path))

static inline void RunBenchmark(const char *name, Size iterations, FunctionRef<void()> func)
{
    Print("  %!..+%1%!0", FmtArg(name).Pad(32));

    int64_t time = GetMonotonicTime();
    int64_t clock = GetClockCounter();

    for (Size i = 0; i < iterations; i++) {
        func();
#ifndef MSC_VER
        __asm__ __volatile__("" : : : "memory");
#endif
    }

    time = GetMonotonicTime() - time;
    clock = GetClockCounter() - clock;

    PrintLn(" %!B..%1 ms%!0 (%2 cycles per iteration)", time, clock / iterations);
}

}
