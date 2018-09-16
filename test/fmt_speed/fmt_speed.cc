#include "stb_sprintf.h"
#include "fmt/format.h"
#include "../../src/common/kutil.hh"

#define ITERATIONS 4000000

struct Checkpoint {
    uint64_t time;
    uint64_t clock;
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

int main(int, char **)
{
#ifdef _WIN32
    FILE *fp = fopen("NUL", "w");
#else
    FILE *fp = fopen("/dev/null", "we");
#endif
    if (!fp) {
        LogError("Cannot open '/dev/null': %1", strerror(errno));
        return 1;
    }
    DEFER { fclose(fp); };

    {
        Checkpoint start = StartBenchmark("printf");
        for (unsigned int i = 0; i < ITERATIONS; i++) {
            fprintf(fp, "%d:%d:%d:%s:%p:%c:%%\n",
                    1234, 42, -313, "str", (void*)1000, 'X');
        }
        EndBenchmark(start, ITERATIONS);
    }

    {
        Checkpoint start = StartBenchmark("stbsp_snprintf");
        for (unsigned int i = 0; i < ITERATIONS; i++) {
            char buf[1024];
            stbsp_snprintf(buf, SIZE(buf), "%d:%d:%d:%s:%p:%c:%%\n",
                           1234, 42, -313, "str", (void*)1000, 'X');
        }
        EndBenchmark(start, ITERATIONS);
    }

    {
        Checkpoint start = StartBenchmark("fmt::format");
        for (unsigned int i = 0; i < ITERATIONS; i++) {
            fmt::format("{}:{}:{}:{}:{}:{}%\n", 1234, 42, -313, "str", (void *)1000, 'X');
        }
        EndBenchmark(start, ITERATIONS);
    }

    {
        Checkpoint start = StartBenchmark("fmt::format_to");
        for (unsigned int i = 0; i < ITERATIONS; i++) {
            fmt::memory_buffer buf;
            fmt::format_to(buf, "{}:{}:{}:{}:{}:{}%\n", 1234, 42, -313, "str", (void *)1000, 'X');
        }
        EndBenchmark(start, ITERATIONS);
    }

    {
        Checkpoint start = StartBenchmark("Print");
        for (unsigned int i = 0; i < ITERATIONS; i++) {
            Print(fp, "%1:%2:%3:%4:%5:%6:%%\n",
                  1234, 42, -313, "str", (void*)1000, 'X');
        }
        EndBenchmark(start, ITERATIONS);
    }

    {
        Checkpoint start = StartBenchmark("Fmt (allocator)");
        for (unsigned int i = 0; i < ITERATIONS; i++) {
            LinkedAllocator temp_alloc;
            Fmt(&temp_alloc, "%1:%2:%3:%4:%5:%6:%%\n",
                1234, 42, -313, "str", (void*)1000, 'X');
        }
        EndBenchmark(start, ITERATIONS);
    }

    {
        Checkpoint start = StartBenchmark("Fmt (heap)");
        HeapArray<char> buf;
        for (unsigned int i = 0; i < ITERATIONS; i++) {
            Fmt(&buf, "%1:%2:%3:%4:%5:%6:%%\n",
                1234, 42, -313, "str", (void*)1000, 'X');
            buf.RemoveFrom(0);
        }
        EndBenchmark(start, ITERATIONS);
    }

    {
        Checkpoint start = StartBenchmark("Fmt (buffer)");
        for (unsigned int i = 0; i < ITERATIONS; i++) {
            LocalArray<char, 1024> buf;
            buf.len = Fmt(buf.data, "%1:%2:%3:%4:%5:%6:%%\n",
                          1234, 42, -313, "str", (void*)1000, 'X').len;
        }
        EndBenchmark(start, ITERATIONS);
    }

    return 0;
}
