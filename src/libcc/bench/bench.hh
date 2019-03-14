// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc.hh"

struct Checkpoint {
    int64_t time;
    int64_t clock;
};

static inline Checkpoint GetTime()
{
    return { GetMonotonicTime(), GetClockCounter() };
}

static inline Checkpoint StartBenchmark(const char *name)
{
    Print(" + %1", name);
    return GetTime();
}

static inline void EndBenchmark(Checkpoint start, unsigned int iterations)
{
    Checkpoint now = GetTime();
    uint64_t time = now.time - start.time;
    uint64_t clock = now.clock - start.clock;
    PrintLn(" %1 ms / %2 cycles (%3 cycles per iteration)", time, clock,
            clock / iterations);
}
