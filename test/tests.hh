// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../src/core/libcc/libcc.hh"

namespace RG {

#define TEST_FUNCTION(Label) \
    static Size tests = 0; \
    static Size failures = 0; \
    PrintLn("%1", Label); \
    RG_DEFER { PrintLn("  Failures: %1 / %2", failures, tests); };

#define TEST(Condition) \
    do { \
        tests++; \
        if (!(Condition)) { \
            PrintLn(stderr, "  [FAIL] %1", RG_STRINGIFY(Condition)); \
            failures++; \
        } \
    } while (false)

static inline void RunBenchmark(const char *label, Size iterations, FunctionRef<void()> func)
{
    Print("  + %1", label);

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
