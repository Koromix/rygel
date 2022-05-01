// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

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

typedef struct Pack3 {
    int a;
    int b;
    int c;
} Pack3;

typedef struct IJK1 { int8_t i; int8_t j; int8_t k; } IJK1;
typedef struct IJK4 { int32_t i; int32_t j; int32_t k; } IJK4;
typedef struct IJK8 { int64_t i; int64_t j; int64_t k; } IJK8;

typedef struct BFG {
    int8_t a;
    int64_t b;
    signed char c;
    const char *d;
    short e;
    struct {
        float f;
        double g;
    } inner;
} BFG;
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

EXPORT void FillPack3(int a, int b, int c, Pack3 *p)
{
    p->a = a;
    p->b = b;
    p->c = c;
}

EXPORT Pack3 RetPack3(int a, int b, int c)
{
    Pack3 p;
    
    p.a = a;
    p.b = b;
    p.c = c;

    return p;
}

EXPORT void FASTCALL AddPack3(int a, int b, int c, Pack3 *p)
{
    p->a += a;
    p->b += b;
    p->c += c;
}

EXPORT int64_t ConcatenateToInt1(int8_t a, int8_t b, int8_t c, int8_t d, int8_t e, int8_t f,
                                 int8_t g, int8_t h, int8_t i, int8_t j, int8_t k, int8_t l)
{
    int64_t ret = 100000000000ull * a + 10000000000ull * b + 1000000000ull * c +
                  100000000ull * d + 10000000ull * e + 1000000ull * f +
                  100000ull * g + 10000ull * h + 1000ull * i +
                  100ull * j + 10ull * k + 1ull * l;
    return ret;
}

EXPORT int64_t ConcatenateToInt4(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f,
                                 int32_t g, int32_t h, int32_t i, int32_t j, int32_t k, int32_t l)
{
    int64_t ret = 100000000000ull * a + 10000000000ull * b + 1000000000ull * c +
                  100000000ull * d + 10000000ull * e + 1000000ull * f +
                  100000ull * g + 10000ull * h + 1000ull * i +
                  100ull * j + 10ull * k + 1ull * l;
    return ret;
}

EXPORT int64_t ConcatenateToInt8(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f,
                                 int64_t g, int64_t h, int64_t i, int64_t j, int64_t k, int64_t l)
{
    int64_t ret = 100000000000ull * a + 10000000000ull * b + 1000000000ull * c +
                  100000000ull * d + 10000000ull * e + 1000000ull * f +
                  100000ull * g + 10000ull * h + 1000ull * i +
                  100ull * j + 10ull * k + 1ull * l;
    return ret;
}

EXPORT const char *ConcatenateToStr1(int8_t a, int8_t b, int8_t c, int8_t d, int8_t e, int8_t f,
                                     int8_t g, int8_t h, IJK1 ijk, int8_t l)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "%d%d%d%d%d%d%d%d%d%d%d%d", a, b, c, d, e, f, g, h, ijk.i, ijk.j, ijk.k, l);
    return buf;
}

EXPORT const char *ConcatenateToStr4(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f,
                                     int32_t g, int32_t h, IJK4 *ijk, int32_t l)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "%d%d%d%d%d%d%d%d%d%d%d%d", a, b, c, d, e, f, g, h, ijk->i, ijk->j, ijk->k, l);
    return buf;
}

EXPORT const char *ConcatenateToStr8(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f,
                                     int64_t g, int64_t h, IJK8 ijk, int64_t l)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64
                               "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64,
             a, b, c, d, e, f, g, h, ijk.i, ijk.j, ijk.k, l);
    return buf;
}

EXPORT BFG STDCALL MakeBFG(BFG *p, int x, double y, const char *str)
{
    BFG bfg;

    char buf[64];
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

EXPORT PackedBFG FASTCALL MakePackedBFG(int x, double y, PackedBFG *p, const char *str)
{
    PackedBFG bfg;

    char buf[64];
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

EXPORT const char *ReturnBigString(const char *str)
{
    static char buf[16 * 1024 * 1024];

    size_t len = strlen(str);
    memcpy(buf, str, len + 1);

    return buf;
}
