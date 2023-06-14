// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>
#if __has_include(<uchar.h>)
    #include <uchar.h>
#else
    typedef uint16_t char16_t;
    typedef uint32_t char32_t;
#endif
#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <unistd.h>
    #include <errno.h>
    #include <pthread.h>
#endif

#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif
#if defined(_M_IX86) || defined(__i386__)
    #ifdef _MSC_VER
        #define FASTCALL __fastcall
        #define STDCALL __stdcall
    #else
        #define FASTCALL __attribute__((fastcall))
        #define STDCALL __attribute__((stdcall))
    #endif
#else
    #define FASTCALL
    #define STDCALL
#endif

typedef struct BFG {
    int8_t a;
    char _pad1[7]; short e;
    int64_t b;
    signed char c;
    const char *d;
    struct {
        float f;
        double g;
    } inner;
} BFG;

typedef struct Vec2 {
    double x;
    double y;
} Vec2;

typedef int STDCALL ApplyCallback(int a, int b, int c);
typedef int IntCallback(int x);
typedef int VectorCallback(int len, Vec2 *vec);
typedef int SortCallback(const void *ptr1, const void *ptr2);
typedef int CharCallback(int idx, char c);

typedef struct StructCallbacks {
    IntCallback *first;
    IntCallback *second;
    IntCallback *third;
} StructCallbacks;

EXPORT int8_t GetMinusOne1(void)
{
    return -1;
}

EXPORT int CallJS(const char *str, int (*cb)(const char *str))
{
    char buf[64];
    snprintf(buf, sizeof(buf), "Hello %s!", str);
    return cb(buf);
}

EXPORT float CallRecursiveJS(int i, float (*func)(int i, const char *str, double d))
{
    float f = func(i, "Hello!", 42.0);
    return f;
}

EXPORT BFG ModifyBFG(int x, double y, const char *str, BFG (*func)(BFG bfg), BFG *p)
{
    BFG bfg;

    static char buf[64];
    snprintf(buf, sizeof(buf), "X/%s/X", str);

    bfg.a = x;
    bfg.b = x * 2;
    bfg.c = x - 27;
    bfg.d = buf;
    bfg.e = x * 27;
    bfg.inner.f = (float)y * x;
    bfg.inner.g = (double)y - x;
    *p = bfg;

    bfg = func(bfg);
    return bfg;
}

EXPORT void Recurse8(int i, void (*func)(int i, int v1, double v2, int v3, int v4, int v5, int v6, float v7, int v8))
{
    func(i, i, (double)(i * 2), i + 1, i * 2 + 1, 3 - i, 100 + i, (float)(i % 2), -i - 1);
}

EXPORT int ApplyStd(int a, int b, int c, ApplyCallback *func)
{
    int ret = func(a, b, c);
    return ret;
}

EXPORT int ApplyMany(int x, IntCallback **callbacks, int length)
{
    for (int i = 0; i < length; i++) {
        x = (callbacks[i])(x);
    }

    return x;
}

EXPORT int ApplyStruct(int x, StructCallbacks callbacks)
{
    x = callbacks.first(x);
    x = callbacks.second(x);
    x = callbacks.third(x);

    return x;
}

static IntCallback *callback;

EXPORT void SetIndirect(IntCallback *cb)
{
    callback = cb;
}

EXPORT int CallIndirect(int x)
{
    return callback(x);
}

#ifdef _WIN32

typedef struct CallContext {
    IntCallback *callback;
    int *ptr;
} CallContext;

static DWORD WINAPI CallThreadedFunc(void *udata)
{
    CallContext *ctx = (CallContext *)udata;
    *ctx->ptr = ctx->callback(*ctx->ptr);

    return 0;
}

EXPORT int CallThreaded(IntCallback *func, int x)
{
    CallContext ctx;

    ctx.callback = func ? func : callback;
    ctx.ptr = &x;

    HANDLE h = CreateThread(NULL, 0, CallThreadedFunc, &ctx, 0, NULL);
    if (!h) {
        perror("CreateThread");
        exit(1);
    }

    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);

    return x;
}

#else

typedef struct CallContext {
    IntCallback *callback;
    int *ptr;
} CallContext;

static void *CallThreadedFunc(void *udata)
{
    CallContext *ctx = (CallContext *)udata;
    *ctx->ptr = ctx->callback(*ctx->ptr);

    return NULL;
}

EXPORT int CallThreaded(IntCallback *func, int x)
{
    CallContext ctx;

    ctx.callback = func ? func : callback;
    ctx.ptr = &x;

    pthread_t thread;
    if (pthread_create(&thread, NULL, CallThreadedFunc, &ctx)) {
        perror("pthread_create");
        exit(1);
    }

    pthread_join(thread, NULL);

    return x;
}

#endif

EXPORT int MakeVectors(int len, VectorCallback *func)
{
    Vec2 vectors[512];

    for (int i = 0; i < len; i++) {
        vectors[i].x = (double)i;
        vectors[i].y = (double)-i;
    }

    int ret = func(len, vectors);

    return ret;
}

EXPORT void CallQSort(void *base, size_t nmemb, size_t size, SortCallback *cb)
{
    qsort(base, nmemb, size, cb);
}

EXPORT int CallMeChar(CharCallback *func)
{
    int ret = 0;

    ret += func(0, 'a');
    ret += func(1, 'b');

    return ret;
}
