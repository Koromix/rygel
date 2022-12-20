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
    char _pad1[7]; short e;
    int64_t b;
    signed char c;
    const char *d;
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

typedef struct IntContainer {
    int values[16];
    int len;
} IntContainer;

typedef struct StrStruct {
    const char *str;
    const char16_t *str16;
} StrStruct;

typedef int STDCALL ApplyCallback(int a, int b, int c);
typedef int IntCallback(int x);

typedef struct StructCallbacks {
    IntCallback *first;
    IntCallback *second;
    IntCallback *third;
} StructCallbacks;

typedef struct EndianInts {
    int16_t i16le;
    int16_t i16be;
    uint16_t u16le;
    uint16_t u16be;
    int32_t i32le;
    int32_t i32be;
    uint32_t u32le;
    uint32_t u32be;
    int64_t i64le;
    int64_t i64be;
    uint64_t u64le;
    uint64_t u64be;
} EndianInts;

typedef struct BigText {
    char text[262145];
} BigText;

typedef struct Vec2 {
    double x;
    double y;
} Vec2;

typedef int VectorCallback(int len, Vec2 *vec);

EXPORT int8_t GetMinusOne1(void)
{
    return -1;
}

EXPORT int16_t GetMinusOne2(void)
{
    return -1;
}

EXPORT int32_t GetMinusOne4(void)
{
    return -1;
}

EXPORT int64_t GetMinusOne8(void *dummy)
{
    return -1;
}

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

EXPORT void MakePolymorphBFG(int type, int x, double y, const char *str, void *p)
{
    if (type == 0) {
        MakeBFG(p, x, y, str);
    } else if (type == 1) {
        MakePackedBFG(x, y, p, str);
    }
}

#ifdef _WIN32
// Exported by DEF file
const char *ReturnBigString(const char *str)
#else
EXPORT const char *ReturnBigString(const char *str)
#endif
{
    const char *copy = strdup(str);
    return copy;
}

EXPORT const char *PrintFmt(const char *fmt, ...)
{
    const int size = 256;
    char *ptr = malloc(size);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ptr, size, fmt, ap);
    va_end(ap);

    return ptr;
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
    const int size = 1024;
    char16_t *ptr = malloc(size * 2);

    size_t len1 = Length16(str1);
    size_t len2 = Length16(str2);

    memcpy(ptr, str1, len1 * 2);
    memcpy(ptr + len1, str2, len2 * 2);
    ptr[len1 + len2] = 0;

    return ptr;
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

EXPORT void Recurse8(int i, void (*func)(int i, int v1, double v2, int v3, int v4, int v5, int v6, float v7, int v8))
{
    func(i, i, (double)(i * 2), i + 1, i * 2 + 1, 3 - i, 100 + i, (float)(i % 2), -i - 1);
}

EXPORT int ApplyStd(int a, int b, int c, ApplyCallback *func)
{
    int ret = func(a, b, c);
    return ret;
}

EXPORT IntContainer ArrayToStruct(int *values, int len)
{
    IntContainer ic;

    memset(&ic, 0, sizeof(ic));
    memcpy(ic.values, values, len * sizeof(*values));
    ic.len = len;

    return ic;
}

EXPORT void FillRange(int init, int step, int *out, int len)
{
    while (--len >= 0) {
        *(out++) = init;
        init += step;
    }
}

EXPORT void MultiplyIntegers(int multiplier, int *values, int len)
{
    for (int i = 0; i < len; i++) {
        values[i] *= multiplier;
    }
}

EXPORT const char *ThroughStr(StrStruct s)
{
    return s.str;
}

EXPORT const char16_t *ThroughStr16(StrStruct s)
{
    return s.str16;
}

EXPORT const char *ThroughStrStar(StrStruct *s)
{
    return s->str;
}

EXPORT const char16_t *ThroughStrStar16(StrStruct *s)
{
    return s->str16;
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

EXPORT void SetCallback(IntCallback *cb)
{
    callback = cb;
}

EXPORT int CallCallback(int x)
{
    return callback(x);
}

EXPORT void ReverseBytes(void *p, int len)
{
    uint8_t *bytes = (uint8_t *)p;

    for (int i = 0; i < len / 2; i++) {
        uint8_t tmp = bytes[i];
        bytes[i] = bytes[len - i - 1];
        bytes[len - i - 1] = tmp;
    }
}

EXPORT void CopyEndianInts1(EndianInts ints, uint8_t buf[56])
{
    memcpy(buf, &ints, sizeof(ints));
}

EXPORT void CopyEndianInts2(int16_t i16le, int16_t i16be, uint16_t u16le, uint16_t u16be,
                            int32_t i32le, int32_t i32be, uint32_t u32le, uint32_t u32be,
                            int64_t i64le, int64_t i64be, uint64_t u64le, uint64_t u64be,
                            EndianInts *out)
{
    out->i16le = i16le;
    out->i16be = i16be;
    out->u16le = u16le;
    out->u16be = u16be;
    out->i32le = i32le;
    out->i32be = i32be;
    out->u32le = u32le;
    out->u32be = u32be;
    out->i64le = i64le;
    out->i64be = i64be;
    out->u64le = u64le;
    out->u64be = u64be;
}

EXPORT uint16_t ReturnEndianInt2(uint16_t v) { return v; }
EXPORT uint32_t ReturnEndianInt4(uint32_t v) { return v; }
EXPORT uint64_t ReturnEndianInt8(uint64_t v) { return v; }

EXPORT BigText ReverseBigText(BigText buf)
{
    BigText ret;
    for (int i = 0; i < sizeof(buf.text); i++) {
        ret.text[sizeof(buf.text) - 1 - i] = buf.text[i];
    }
    return ret;
}

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

EXPORT size_t UpperCaseStrAscii(const char *str, char *out)
{
    size_t len = 0;

    while (str[len]) {
        char c = str[len];

        if (c >= 'a' && c <= 'z') {
            out[len] = (char)(c - 32);
        } else {
            out[len] = c;
        }

        len++;
    }
    out[len] = 0;

    return len;
}

EXPORT size_t UpperCaseStrAscii16(const char16_t *str, char16_t *out)
{
    size_t len = 0;

    while (str[len]) {
        char16_t c = str[len];

        if (c >= 'a' && c <= 'z') {
            out[len] = (char16_t)(c - 32);
        } else {
            out[len] = c;
        }

        len++;
    }
    out[len] = 0;

    return len;
}
