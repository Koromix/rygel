// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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
#include <string.h>
#include <inttypes.h>

#if defined(_WIN32)
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif
#if defined(_M_IX86) || defined(__i386__)
    #if defined(_MSC_VER)
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

#pragma pack(push, 1)
typedef struct PackedBFG {
    int8_t a;
    int64_t b;
    signed char c;
    const char *d;
    short e;
    struct {
        float f;
        double g;
    } inner;
} PackedBFG;
#pragma pack(pop)

typedef int CharCallback(int idx, char c);
typedef int BinaryIntFunc(int a, int b);

EXPORT int64_t ConcatenateToInt1(int8_t a, int8_t b, int8_t c, int8_t d, int8_t e, int8_t f,
                                 int8_t g, int8_t h, int8_t i, int8_t j, int8_t k, int8_t l)
{
    int64_t ret = 100000000000ull * a + 10000000000ull * b + 1000000000ull * c +
                  100000000ull * d + 10000000ull * e + 1000000ull * f +
                  100000ull * g + 10000ull * h + 1000ull * i +
                  100ull * j + 10ull * k + 1ull * l;
    return ret;
}

EXPORT PackedBFG FASTCALL MakePackedBFG(int x, double y, PackedBFG *p, const char *str)
{
    PackedBFG bfg;

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

    return bfg;
}

EXPORT int CallMeChar(CharCallback *func)
{
    int ret = 0;

    ret += func(0, 'a');
    ret += func(1, 'b');

    return ret;
}

static int AddInt(int a, int b) { return a + b; }
static int SubstractInt(int a, int b) { return a - b; }
static int MultiplyInt(int a, int b) { return a * b; }
static int DivideInt(int a, int b) { return a / b; }

EXPORT BinaryIntFunc *GetBinaryIntFunction(const char *type)
{
    if (!strcmp(type, "add")) {
        return AddInt;
    } else if (!strcmp(type, "substract")) {
        return SubstractInt;
    } else if (!strcmp(type, "multiply")) {
        return MultiplyInt;
    } else if (!strcmp(type, "divide")) {
        return DivideInt;
    } else {
        return NULL;
    }
}
