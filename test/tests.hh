// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../src/core/libcc/libcc.hh"

namespace RG {

#define TEST_FUNCTION(Label) \
    static Size tests = 0; \
    static Size failures = 0; \
    Print("  %1", (Label)); \
    RG_DEFER { ReportTestResults(tests, failures); };

static inline void ReportTestResults(Size tests, Size failures)
{
    if (failures) {
        PrintLn(stderr, "\n    %!R..Failed%!0 (%1/%2)", failures, tests);
    } else {
        PrintLn(stderr, " %!G..Success%!0 (%1)", tests);
    }
}

#define TEST_EX(Condition, ...) \
    do { \
        tests++; \
        if (!(Condition)) { \
            Print(stderr, "\n    %!D..[%1:%2]%!0 ", SplitStrReverseAny(__FILE__, RG_PATH_SEPARATORS), __LINE__); \
            Print(stderr, __VA_ARGS__); \
            failures++; \
        } \
    } while (false)

#define TEST(Condition) \
    TEST_EX((Condition),  "%1", RG_STRINGIFY(Condition))
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

static inline void RunBenchmark(const char *label, Size iterations, FunctionRef<void()> func)
{
    Print("    + %1", label);

    int64_t time = GetMonotonicTime();
    int64_t clock = GetClockCounter();

    for (Size i = 0; i < iterations; i++) {
        func();
    }

    time = GetMonotonicTime() - time;
    clock = GetClockCounter() - clock;

    PrintLn(" %1 ms / %2 cycles (%3 cycles per iteration)", time, clock, clock / iterations);
}

}
