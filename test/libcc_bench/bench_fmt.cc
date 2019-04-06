// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "vendor/stb_sprintf.h"
#include "vendor/fmt/format.h"
#include "../../src/libcc/libcc.hh"
#include "bench.hh"

#define ITERATIONS 4000000

void BenchFmt()
{
    const int iterations = 4000000;

#ifdef _WIN32
    FILE *fp = fopen("NUL", "w");
#else
    FILE *fp = fopen("/dev/null", "we");
#endif
    RG_ASSERT(fp);
    RG_DEFER { fclose(fp); };

    PrintLn("String formatting");

    // printf
    {
        Checkpoint start = StartBenchmark("printf");
        for (int i = 0; i < iterations; i++) {
            fprintf(fp, "%d:%d:%d:%s:%p:%c:%%\n",
                    1234, 42, -313, "str", (void*)1000, 'X');
        }
        EndBenchmark(start, iterations);
    }

    // snprintf
    {
        Checkpoint start = StartBenchmark("snprintf");
        for (int i = 0; i < iterations; i++) {
            char buf[1024];
            snprintf(buf, RG_SIZE(buf), "%d:%d:%d:%s:%p:%c:%%\n",
                    1234, 42, -313, "str", (void*)1000, 'X');
        }
        EndBenchmark(start, iterations);
    }

#ifndef _WIN32
    // asprintf
    {
        Checkpoint start = StartBenchmark("asprintf");
        for (int i = 0; i < iterations; i++) {
            char *ret = nullptr;
            asprintf(&ret, "%d:%d:%d:%s:%p:%c:%%\n",
                     1234, 42, -313, "str", (void*)1000, 'X');
            free(ret);
        }
        EndBenchmark(start, iterations);
    }
#endif

    // stbsp_snprintf
    {
        Checkpoint start = StartBenchmark("stbsp_snprintf");
        for (int i = 0; i < iterations; i++) {
            char buf[1024];
            stbsp_snprintf(buf, RG_SIZE(buf), "%d:%d:%d:%s:%p:%c:%%\n",
                           1234, 42, -313, "str", (void*)1000, 'X');
        }
        EndBenchmark(start, iterations);
    }

    // fmt::format
    {
        Checkpoint start = StartBenchmark("fmt::format");
        for (int i = 0; i < iterations; i++) {
            fmt::format("{}:{}:{}:{}:{}:{}%\n", 1234, 42, -313, "str", (void *)1000, 'X');
        }
        EndBenchmark(start, iterations);
    }

    // fmt::format_to
    {
        Checkpoint start = StartBenchmark("fmt::format_to");
        for (int i = 0; i < iterations; i++) {
            fmt::memory_buffer buf;
            fmt::format_to(buf, "{}:{}:{}:{}:{}:{}%\n", 1234, 42, -313, "str", (void *)1000, 'X');
        }
        EndBenchmark(start, iterations);
    }

    // libcc Print
    {
        Checkpoint start = StartBenchmark("libcc Print");
        for (int i = 0; i < iterations; i++) {
            Print(fp, "%1:%2:%3:%4:%5:%6:%%\n",
                  1234, 42, -313, "str", (void*)1000, 'X');
        }
        EndBenchmark(start, iterations);
    }

    // libcc Fmt (allocator)
    {
        Checkpoint start = StartBenchmark("libcc Fmt (allocator)");
        for (int i = 0; i < iterations; i++) {
            LinkedAllocator temp_alloc;
            Fmt(&temp_alloc, "%1:%2:%3:%4:%5:%6:%%\n",
                1234, 42, -313, "str", (void*)1000, 'X');
        }
        EndBenchmark(start, iterations);
    }

    // libcc Fmt (heap)
    {
        Checkpoint start = StartBenchmark("libcc Fmt (heap)");
        HeapArray<char> buf;
        for (int i = 0; i < iterations; i++) {
            Fmt(&buf, "%1:%2:%3:%4:%5:%6:%%\n",
                1234, 42, -313, "str", (void*)1000, 'X');
            buf.RemoveFrom(0);
        }
        EndBenchmark(start, iterations);
    }

    // libcc Fmt (buffer)
    {
        Checkpoint start = StartBenchmark("libcc Fmt (buffer)");
        for (int i = 0; i < iterations; i++) {
            LocalArray<char, 1024> buf;
            buf.len = Fmt(buf.data, "%1:%2:%3:%4:%5:%6:%%\n",
                          1234, 42, -313, "str", (void*)1000, 'X').len;
        }
        EndBenchmark(start, iterations);
    }
}
