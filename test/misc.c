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
#include <stdarg.h>
#if __has_include(<uchar.h>)
    #include <uchar.h>
#else
    typedef uint16_t char16_t;
    typedef uint32_t char32_t;
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

typedef struct Pack1 {
    int a;
} Pack1;
typedef struct Pack2 {
    int a;
    int b;
} Pack2;
typedef struct Pack3 {
    int a;
    int b;
    int c;
} Pack3;

typedef struct Float2 {
    float a;
    float b;
} Float2;
typedef struct Float3 {
    float a;
    float b[2];
} Float3;

typedef struct Double2 {
    double a;
    double b;
} Double2;
typedef struct Double3 {
    double a;
    struct {
        double b;
        double c;
    } s;
} Double3;

typedef struct FloatInt {
    float f;
    int i;
} FloatInt;
typedef struct IntFloat {
    int i;
    float f;
} IntFloat;

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

typedef struct FixedString {
    char buf[64];
} FixedString;
typedef struct FixedWide {
    int16_t buf[64];
} FixedWide;

typedef struct SingleU32 { uint32_t v; } SingleU32;
typedef struct SingleU64 { uint64_t v; } SingleU64;
typedef struct SingleI64 { int64_t v; } SingleI64;

EXPORT void FillPack1(int a, Pack1 *p)
{
    p->a = a;
}

EXPORT Pack1 RetPack1(int a)
{
    Pack1 p;

    p.a = a;

    return p;
}

EXPORT void FASTCALL AddPack1(int a, Pack1 *p)
{
    p->a += a;
}

EXPORT void FillPack2(int a, int b, Pack2 *p)
{
    p->a = a;
    p->b = b;
}

EXPORT Pack2 RetPack2(int a, int b)
{
    Pack2 p;

    p.a = a;
    p.b = b;

    return p;
}

EXPORT void FASTCALL AddPack2(int a, int b, Pack2 *p)
{
    p->a += a;
    p->b += b;
}

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

EXPORT Float2 PackFloat2(float a, float b, Float2 *out)
{
    Float2 ret;

    ret.a = a;
    ret.b = b;
    *out = ret;

    return ret;
}

EXPORT Float2 ThroughFloat2(Float2 f2)
{
    return f2;
}

EXPORT Float3 PackFloat3(float a, float b, float c, Float3 *out)
{
    Float3 ret;

    ret.a = a;
    ret.b[0] = b;
    ret.b[1] = c;
    *out = ret;

    return ret;
}

EXPORT Float3 ThroughFloat3(Float3 f3)
{
    return f3;
}

EXPORT Double2 PackDouble2(double a, double b, Double2 *out)
{
    Double2 ret;

    ret.a = a;
    ret.b = b;
    *out = ret;

    return ret;
}

EXPORT Double3 PackDouble3(double a, double b, double c, Double3 *out)
{
    Double3 ret;

    ret.a = a;
    ret.s.b = b;
    ret.s.c = c;
    *out = ret;

    return ret;
}

EXPORT IntFloat ReverseFloatInt(FloatInt sfi)
{
    IntFloat sif;

    sif.i = (int)sfi.f;
    sif.f = (float)sfi.i;

    return sif;
}

EXPORT FloatInt ReverseIntFloat(IntFloat sif)
{
    FloatInt sfi;

    sfi.i = (int)sif.f;
    sfi.f = (float)sif.i;

    return sfi;
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

EXPORT const char *ReturnBigString(const char *str)
{
    static char buf[16 * 1024 * 1024];

    size_t len = strlen(str);
    memcpy(buf, str, len + 1);

    return buf;
}

EXPORT const char *PrintFmt(const char *fmt, ...)
{
    static char buf[256];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    return buf;
}

size_t Length16(const char16_t *str)
{
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

EXPORT const char16_t *Concat16(const char16_t *str1, const char16_t *str2)
{
    static char16_t buf[1024];

    size_t len1 = Length16(str1);
    size_t len2 = Length16(str2);

    memcpy(buf, str1, len1 * 2);
    memcpy(buf + len1, str2, len2 * 2);
    buf[(len1 + len2) * 2] = 0;

    return buf;
}

EXPORT FixedString ReturnFixedStr(FixedString str)
{
    return str;
}

EXPORT FixedWide ReturnFixedWide(FixedWide str)
{
    return str;
}

EXPORT uint32_t ThroughUInt32UU(uint32_t v)
{
    return v;
}
EXPORT SingleU32 ThroughUInt32SS(SingleU32 s)
{
    return s;
}
EXPORT SingleU32 ThroughUInt32SU(uint32_t v)
{
    SingleU32 s;
    s.v = v;
    return s;
}
EXPORT uint32_t ThroughUInt32US(SingleU32 s)
{
    return s.v;
}

EXPORT uint64_t ThroughUInt64UU(uint64_t v)
{
    return v;
}
EXPORT SingleU64 ThroughUInt64SS(SingleU64 s)
{
    return s;
}
EXPORT SingleU64 ThroughUInt64SU(uint64_t v)
{
    SingleU64 s;
    s.v = v;
    return s;
}
EXPORT uint64_t ThroughUInt64US(SingleU64 s)
{
    return s.v;
}

EXPORT int64_t ThroughInt64II(int64_t v)
{
    return v;
}
EXPORT SingleI64 ThroughInt64SS(SingleI64 s)
{
    return s;
}
EXPORT SingleI64 ThroughInt64SI(int64_t v)
{
    SingleI64 s;
    s.v = v;
    return s;
}
EXPORT int64_t ThroughInt64IS(SingleI64 s)
{
    return s.v;
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
