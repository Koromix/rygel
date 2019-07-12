// Hacks for Clang support on Windows (Niels)

#if defined(_WIN32)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <inttypes.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>

#if !defined(__MINGW32__)
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

typedef struct { int dummy; } pthread_mutexattr_t;
typedef struct { int dummy; } pthread_condattr_t;

typedef CRITICAL_SECTION pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

int clock_gettime(int clock, struct timespec *ts)
{
    static const uint64_t EPOCH = (uint64_t)116444736000000000ULL;

    SYSTEMTIME system_time;
    FILETIME file_time;
    uint64_t time;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((uint64_t)file_time.dwLowDateTime) + (((uint64_t)file_time.dwHighDateTime) << 32);

    ts->tv_sec  = (long)((time - EPOCH) / 10000000L);
    ts->tv_nsec = (long)(system_time.wMilliseconds * 1000000);

    return 0;
}

int gettimeofday(struct timeval *tp, struct timezone *tzp)
{
    struct timespec ts;
    clock_gettime(0, &ts);

    tp->tv_sec = ts.tv_sec;
    tp->tv_usec = ts.tv_nsec / 1000;

    return 0;
}

int pthread_mutex_init(pthread_mutex_t *m, pthread_mutexattr_t *a)
{
    (void)a;

    InitializeCriticalSection(m);
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *m)
{
    DeleteCriticalSection(m);
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *m)
{
    EnterCriticalSection(m);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m)
{
    LeaveCriticalSection(m);
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *m)
{
    return TryEnterCriticalSection(m) ? 0 : EBUSY;
}

static unsigned long long _pthread_time_in_ms_from_timespec(const struct timespec *ts)
{
    unsigned long long t = ts->tv_sec * 1000;
    t += ts->tv_nsec / 1000000;

    return t;
}

static unsigned long long _pthread_time_in_ms(void)
{
    struct __timeb64 tb;
    _ftime64(&tb);

    return tb.time * 1000 + tb.millitm;
}

static unsigned long long _pthread_rel_time_in_ms(const struct timespec *ts)
{
    unsigned long long t1 = _pthread_time_in_ms_from_timespec(ts);
    unsigned long long t2 = _pthread_time_in_ms();

    if (t1 < t2) return 1;
    return t1 - t2;
}

int pthread_cond_init(pthread_cond_t *c, pthread_condattr_t *a)
{
    (void) a;

    InitializeConditionVariable(c);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *c)
{
    (void)c;
    return 0;
}

int pthread_cond_signal(pthread_cond_t *c)
{
    WakeConditionVariable(c);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *c)
{
    WakeAllConditionVariable(c);
    return 0;
}

int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
    SleepConditionVariableCS(c, m, INFINITE);
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, struct timespec *t)
{
    unsigned long long tm = _pthread_rel_time_in_ms(t);

    if (!SleepConditionVariableCS(c, m, tm))
        return ETIMEDOUT;
    if (!_pthread_rel_time_in_ms(t))
        return ETIMEDOUT;

    return 0;
}

#endif
