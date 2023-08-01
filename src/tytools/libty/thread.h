/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_THREAD_H
#define TY_THREAD_H

#include "common.h"
#ifndef _WIN32
    #include <pthread.h>
#endif

_HS_BEGIN_C

#ifdef _WIN32
typedef unsigned long ty_thread_id; // DWORD
#else
typedef pthread_t ty_thread_id;
#endif

typedef struct ty_thread {
    ty_thread_id thread_id;
#ifdef _WIN32
    void *h; // HANDLE
#else
    bool init;
#endif
} ty_thread;

typedef struct ty_mutex {
    // We don't want to include windows.h in public headers!
#if defined(_WIN64)
    struct { uint64_t dummy[5]; } mutex; // CRITICAL_SECTION
#elif defined(_WIN32)
    struct { uint32_t dummy[6]; } mutex; // CRITICAL_SECTION
#else
    pthread_mutex_t mutex;
#endif
    bool init;
} ty_mutex;

typedef struct ty_cond {
#ifdef _WIN32
    uintptr_t cv; // CONDITION_VARIABLE
#else
    pthread_cond_t cond;
#endif
    bool init;
} ty_cond;

typedef int ty_thread_func(void *udata);

int ty_thread_create(ty_thread *thread, ty_thread_func *f, void *udata);
int ty_thread_join(ty_thread *thread);
void ty_thread_detach(ty_thread *thread);

ty_thread_id ty_thread_get_self_id(void);

int ty_mutex_init(ty_mutex *mutex);
void ty_mutex_release(ty_mutex *mutex);

void ty_mutex_lock(ty_mutex *mutex);
void ty_mutex_unlock(ty_mutex *mutex);

int ty_cond_init(ty_cond *cond);
void ty_cond_release(ty_cond *cond);

void ty_cond_signal(ty_cond *cond);
void ty_cond_broadcast(ty_cond *cond);

bool ty_cond_wait(ty_cond *cond, ty_mutex *mutex, int timeout);

_HS_END_C

#endif
