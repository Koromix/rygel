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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>
#if __has_include(<uchar.h>)
    #include <uchar.h>
#else
    typedef uint16_t char16_t;
    typedef uint32_t char32_t;
#endif
#if defined(_WIN32)
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <unistd.h>
    #include <errno.h>
    #include <pthread.h>
#endif

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

bool DoReturnBool(int cond);

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

typedef struct Float1 {
    float f;
} Float1;
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

typedef struct BufferInfo {
    int len;
    uint8_t *ptr;
} BufferInfo;

typedef struct OpaqueStruct {
    int a;
    int b;
    int c;
    int d;
} OpaqueStruct;

EXPORT int sym_int = 0;
EXPORT const char *sym_str = NULL;
EXPORT int sym_int3[3] = { 0, 0, 0 };

static char16_t *write_ptr16;
static int write_max16;
static char32_t *write_ptr32;
static int write_max32;

EXPORT void CallFree(void *ptr)
{
    free(ptr);
}

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

EXPORT Float1 PackFloat1(float f, Float1 *out)
{
    Float1 ret;

    ret.f = f;
    *out = ret;

    return ret;
}

EXPORT Float1 ThroughFloat1(Float1 f1)
{
    return f1;
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

#if defined(_WIN32)
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

EXPORT const wchar_t *PrintFmtWide(const wchar_t *fmt, ...)
{
    const int size = 256;
    wchar_t *ptr = malloc(size * sizeof(wchar_t));

    va_list ap;
    va_start(ap, fmt);
    vswprintf(ptr, size, fmt, ap);
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

EXPORT void Concat16Out(const char16_t *str1, const char16_t *str2, const char16_t **out)
{
    const int size = 1024;
    char16_t *ptr = malloc(size * 2);

    size_t len1 = Length16(str1);
    size_t len2 = Length16(str2);

    memcpy(ptr, str1, len1 * 2);
    memcpy(ptr + len1, str2, len2 * 2);
    ptr[len1 + len2] = 0;

    *out = ptr;
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

EXPORT size_t UpperCaseStrAscii32(const char32_t *str, char32_t *out)
{
    size_t len = 0;

    while (str[len]) {
        char32_t c = str[len];

        if (c >= 'a' && c <= 'z') {
            out[len] = (char32_t)(c - 32);
        } else {
            out[len] = c;
        }

        len++;
    }
    out[len] = 0;

    return len;
}

EXPORT void ChangeDirectory(const char *dirname)
{
    chdir(dirname);
}

EXPORT void UpperToInternalBuffer(const char *str, char **ptr)
{
    static char buf[512];

    size_t len = 0;

    while (str[len]) {
        char c = str[len];

        if (c >= 'a' && c <= 'z') {
            buf[len] = (char)(c - 32);
        } else {
            buf[len] = c;
        }

        len++;
    }
    buf[len] = 0;

    *ptr = buf;
}

EXPORT int ComputeLengthUntilNul(const void *ptr)
{
    return (int)strlen(ptr);
}

static size_t StringLength16(const char16_t *str16)
{
    size_t len = 0;

    while (str16[len]) {
        len++;
    }

    return len;
}

EXPORT int ComputeLengthUntilNul16(const int16_t *ptr)
{
    return (int)StringLength16((const char16_t *)ptr);
}

EXPORT void ReverseStringVoid(void *ptr)
{
    char *str = (char *)ptr;
    size_t len = strlen(str);

    for (size_t i = 0; i < len / 2; i++) {
        char tmp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = tmp;
    }
}

EXPORT void ReverseString16Void(void *ptr)
{
    char16_t *str16 = (char16_t *)ptr;
    size_t len = StringLength16(ptr);

    for (size_t i = 0; i < len / 2; i++) {
        char16_t tmp = str16[i];
        str16[i] = str16[len - i - 1];
        str16[len - i - 1] = tmp;
    }
}

typedef int BinaryIntFunc(int a, int b);
typedef int VariadicIntFunc(int n, ...);

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

static int AddIntN(int n, ...)
{
    int total = 0;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        total += va_arg(ap, int);
    }
    va_end(ap);

    return total;
}

static int MultiplyIntN(int n, ...)
{
    int total = 1;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        total *= va_arg(ap, int);
    }
    va_end(ap);

    return total;
}

EXPORT VariadicIntFunc *GetVariadicIntFunction(const char *type)
{
    if (!strcmp(type, "add")) {
        return AddIntN;
    } else if (!strcmp(type, "multiply")) {
        return MultiplyIntN;
    } else {
        return NULL;
    }
}

EXPORT void FillBufferDirect(BufferInfo buf, int c)
{
    memset(buf.ptr, c, buf.len);
}

EXPORT void FillBufferIndirect(const BufferInfo *buf, int c)
{
    memset(buf->ptr, c, buf->len);
}

EXPORT const char *GetLatin1String()
{
    // ®²
    return "Microsoft\xAE\xB2";
}

EXPORT int BoolToInt(bool a)
{
    return (int)a;
}

EXPORT unsigned int BoolToMask12(bool a, bool b, bool c, bool d, bool e, bool f,
                                 bool g, bool h, bool i, bool j, bool k, bool l)
{
    unsigned int mask = ((unsigned int)a << 11) | ((unsigned int)b << 10) | ((unsigned int)c << 9) |
                        ((unsigned int)d << 8) | ((unsigned int)e << 7) | ((unsigned int)f << 6) |
                        ((unsigned int)g << 5) | ((unsigned int)h << 4) | ((unsigned int)i << 3) |
                        ((unsigned int)j << 2) | ((unsigned int)k << 1) | ((unsigned int)l << 0);
    return mask;
}

EXPORT int IfElseInt(bool cond, int a, int b)
{
    return cond ? a : b;
}

EXPORT const char *IfElseStr(const char *a, const char *b, bool cond)
{
    return cond ? a : b;
}

EXPORT int GetSymbolInt()
{
    return sym_int;
}

EXPORT const char *GetSymbolStr()
{
    return sym_str;
}

EXPORT void GetSymbolInt3(int out[3])
{
    out[0] = sym_int3[0];
    out[1] = sym_int3[1];
    out[2] = sym_int3[2];
}

EXPORT void WriteConfigure16(char16_t *buf, int max)
{
    assert(max > 0);

    write_ptr16 = buf;
    write_max16 = max - 1;
}

EXPORT void WriteString16(const char16_t *str)
{
    int len = 0;

    while (str[len] && len < write_max16) {
        write_ptr16[len] = str[len];
        len++;
    }

    write_ptr16[len] = 0;
}

EXPORT void WriteConfigure32(char32_t *buf, int max)
{
    assert(max > 0);

    write_ptr32 = buf;
    write_max32 = max - 1;
}

EXPORT void WriteString32(const char32_t *str)
{
    int len = 0;

    while (str[len] && len < write_max32) {
        write_ptr32[len] = str[len];
        len++;
    }

    write_ptr32[len] = 0;
}

EXPORT bool ReturnBool(int cond)
{
    bool ret = DoReturnBool(cond);
    assert(ret == !!cond);
    return ret;
}

EXPORT int ComputeWideLength(const wchar_t *str)
{
    return (int)wcslen(str);
}

EXPORT void FillOpaqueStruct(unsigned int value, OpaqueStruct *opaque)
{
    opaque->a = (value >> 24) & 0xFF;
    opaque->b = (value >> 16) & 0xFF;
    opaque->c = (value >> 8) & 0xFF;
    opaque->d = (value >> 0) & 0xFF;
}
