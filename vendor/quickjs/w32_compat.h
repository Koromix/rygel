// Hacks for Clang support on Windows (Niels)

#ifndef W32_COMPAT_H
#define W32_COMPAT_H

#if defined(_WIN32)

#include <inttypes.h>
#include <winsock.h>

typedef intptr_t ssize_t;

typedef struct { int dummy; } pthread_mutexattr_t;
typedef struct { int dummy; } pthread_condattr_t;

typedef struct {
    void *DebugInfo;
    long LockCount;
    long RecursionCount;
    void *OwningThread;
    void *LockSemaphore;
    uintptr_t SpinCount;
} pthread_mutex_t;

typedef struct {
    void *ptr;
} pthread_cond_t;

#define CLOCK_REALTIME 0
// #define CLOCK_MONOTONIC 1

#define PTHREAD_MUTEX_INITIALIZER {(void*)-1, -1, 0, 0, 0, 0}
#define PTHREAD_COND_INITIALIZER {0}

int clock_gettime(int clock, struct timespec *spec);
int gettimeofday(struct timeval *tp, struct timezone *tzp);

int pthread_mutex_init(pthread_mutex_t *m, pthread_mutexattr_t *a);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);

int pthread_cond_init(pthread_cond_t *c, pthread_condattr_t *a);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, struct timespec *t);

#endif

#endif
