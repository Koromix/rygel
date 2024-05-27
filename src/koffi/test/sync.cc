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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <direct.h>
#else
    #include <unistd.h>
    #include <errno.h>
    #include <pthread.h>
#endif
#include <type_traits>

#ifdef _WIN32
    #define EXPORT_SYMBOL __declspec(dllexport)
    #define EXPORT_FUNCTION extern "C" __declspec(dllexport)
#else
    #define EXPORT_SYMBOL __attribute__((visibility("default")))
    #define EXPORT_FUNCTION extern "C" __attribute__((visibility("default")))
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

extern "C" bool DoReturnBool(int cond);

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

typedef enum Enum1 { Enum1_A = 0, Enum1_B = 42 } Enum1;
typedef enum Enum2 { Enum2_A = -1, Enum2_B = 2147483647 } Enum2;
typedef enum Enum3 { Enum3_A = -1, Enum3_B = 2147483648u } Enum3;
typedef enum Enum4 { Enum4_A = 0, Enum4_B = 2147483648u } Enum4;
typedef enum Enum5 { Enum5_A = 0, Enum5_B = 9223372036854775808ull } Enum5;

EXPORT_SYMBOL int sym_int = 0;
EXPORT_SYMBOL const char *sym_str = NULL;
EXPORT_SYMBOL int sym_int3[3] = { 0, 0, 0 };

static char16_t *write_ptr;
static int write_max;

EXPORT_FUNCTION void CallFree(void *ptr)
{
    free(ptr);
}

EXPORT_FUNCTION int8_t GetMinusOne1(void)
{
    return -1;
}

EXPORT_FUNCTION int16_t GetMinusOne2(void)
{
    return -1;
}

EXPORT_FUNCTION int32_t GetMinusOne4(void)
{
    return -1;
}

EXPORT_FUNCTION int64_t GetMinusOne8(void *dummy)
{
    return -1;
}

EXPORT_FUNCTION void FillPack1(int a, Pack1 *p)
{
    p->a = a;
}

EXPORT_FUNCTION Pack1 RetPack1(int a)
{
    Pack1 p;

    p.a = a;

    return p;
}

EXPORT_FUNCTION void FASTCALL AddPack1(int a, Pack1 *p)
{
    p->a += a;
}

EXPORT_FUNCTION void FillPack2(int a, int b, Pack2 *p)
{
    p->a = a;
    p->b = b;
}

EXPORT_FUNCTION Pack2 RetPack2(int a, int b)
{
    Pack2 p;

    p.a = a;
    p.b = b;

    return p;
}

EXPORT_FUNCTION void FASTCALL AddPack2(int a, int b, Pack2 *p)
{
    p->a += a;
    p->b += b;
}

EXPORT_FUNCTION void FillPack3(int a, int b, int c, Pack3 *p)
{
    p->a = a;
    p->b = b;
    p->c = c;
}

EXPORT_FUNCTION Pack3 RetPack3(int a, int b, int c)
{
    Pack3 p;

    p.a = a;
    p.b = b;
    p.c = c;

    return p;
}

EXPORT_FUNCTION void FASTCALL AddPack3(int a, int b, int c, Pack3 *p)
{
    p->a += a;
    p->b += b;
    p->c += c;
}

EXPORT_FUNCTION Float1 PackFloat1(float f, Float1 *out)
{
    Float1 ret;

    ret.f = f;
    *out = ret;

    return ret;
}

EXPORT_FUNCTION Float1 ThroughFloat1(Float1 f1)
{
    return f1;
}

EXPORT_FUNCTION Float2 PackFloat2(float a, float b, Float2 *out)
{
    Float2 ret;

    ret.a = a;
    ret.b = b;
    *out = ret;

    return ret;
}

EXPORT_FUNCTION Float2 ThroughFloat2(Float2 f2)
{
    return f2;
}

EXPORT_FUNCTION Float3 PackFloat3(float a, float b, float c, Float3 *out)
{
    Float3 ret;

    ret.a = a;
    ret.b[0] = b;
    ret.b[1] = c;
    *out = ret;

    return ret;
}

EXPORT_FUNCTION Float3 ThroughFloat3(Float3 f3)
{
    return f3;
}

EXPORT_FUNCTION Double2 PackDouble2(double a, double b, Double2 *out)
{
    Double2 ret;

    ret.a = a;
    ret.b = b;
    *out = ret;

    return ret;
}

EXPORT_FUNCTION Double3 PackDouble3(double a, double b, double c, Double3 *out)
{
    Double3 ret;

    ret.a = a;
    ret.s.b = b;
    ret.s.c = c;
    *out = ret;

    return ret;
}

EXPORT_FUNCTION IntFloat ReverseFloatInt(FloatInt sfi)
{
    IntFloat sif;

    sif.i = (int)sfi.f;
    sif.f = (float)sfi.i;

    return sif;
}

EXPORT_FUNCTION FloatInt ReverseIntFloat(IntFloat sif)
{
    FloatInt sfi;

    sfi.i = (int)sif.f;
    sfi.f = (float)sif.i;

    return sfi;
}

EXPORT_FUNCTION int64_t ConcatenateToInt1(int8_t a, int8_t b, int8_t c, int8_t d, int8_t e, int8_t f,
                                 int8_t g, int8_t h, int8_t i, int8_t j, int8_t k, int8_t l)
{
    int64_t ret = 100000000000ull * a + 10000000000ull * b + 1000000000ull * c +
                  100000000ull * d + 10000000ull * e + 1000000ull * f +
                  100000ull * g + 10000ull * h + 1000ull * i +
                  100ull * j + 10ull * k + 1ull * l;
    return ret;
}

EXPORT_FUNCTION int64_t ConcatenateToInt4(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f,
                                 int32_t g, int32_t h, int32_t i, int32_t j, int32_t k, int32_t l)
{
    int64_t ret = 100000000000ull * a + 10000000000ull * b + 1000000000ull * c +
                  100000000ull * d + 10000000ull * e + 1000000ull * f +
                  100000ull * g + 10000ull * h + 1000ull * i +
                  100ull * j + 10ull * k + 1ull * l;
    return ret;
}

EXPORT_FUNCTION int64_t ConcatenateToInt8(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f,
                                 int64_t g, int64_t h, int64_t i, int64_t j, int64_t k, int64_t l)
{
    int64_t ret = 100000000000ull * a + 10000000000ull * b + 1000000000ull * c +
                  100000000ull * d + 10000000ull * e + 1000000ull * f +
                  100000ull * g + 10000ull * h + 1000ull * i +
                  100ull * j + 10ull * k + 1ull * l;
    return ret;
}

EXPORT_FUNCTION const char *ConcatenateToStr1(int8_t a, int8_t b, int8_t c, int8_t d, int8_t e, int8_t f,
                                     int8_t g, int8_t h, IJK1 ijk, int8_t l)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "%d%d%d%d%d%d%d%d%d%d%d%d", a, b, c, d, e, f, g, h, ijk.i, ijk.j, ijk.k, l);
    return buf;
}

EXPORT_FUNCTION const char *ConcatenateToStr4(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f,
                                     int32_t g, int32_t h, IJK4 *ijk, int32_t l)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "%d%d%d%d%d%d%d%d%d%d%d%d", a, b, c, d, e, f, g, h, ijk->i, ijk->j, ijk->k, l);
    return buf;
}

EXPORT_FUNCTION const char *ConcatenateToStr8(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f,
                                     int64_t g, int64_t h, IJK8 ijk, int64_t l)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64
                               "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64 "%" PRIi64,
             a, b, c, d, e, f, g, h, ijk.i, ijk.j, ijk.k, l);
    return buf;
}

EXPORT_FUNCTION BFG STDCALL MakeBFG(BFG *p, int x, double y, const char *str)
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

EXPORT_FUNCTION PackedBFG FASTCALL MakePackedBFG(int x, double y, PackedBFG *p, const char *str)
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

EXPORT_FUNCTION void MakePolymorphBFG(int type, int x, double y, const char *str, void *p)
{
    if (type == 0) {
        MakeBFG((BFG *)p, x, y, str);
    } else if (type == 1) {
        MakePackedBFG(x, y, (PackedBFG *)p, str);
    }
}

#ifdef _WIN32
// Exported by DEF file
const char *ReturnBigString(const char *str)
#else
EXPORT_FUNCTION const char *ReturnBigString(const char *str)
#endif
{
    const char *copy = strdup(str);
    return copy;
}

EXPORT_FUNCTION const char *PrintFmt(const char *fmt, ...)
{
    const int size = 256;
    char *ptr = (char *)malloc(size);

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

EXPORT_FUNCTION const char16_t *Concat16(const char16_t *str1, const char16_t *str2)
{
    const int size = 1024;
    char16_t *ptr = (char16_t *)malloc(size * 2);

    size_t len1 = Length16(str1);
    size_t len2 = Length16(str2);

    memcpy(ptr, str1, len1 * 2);
    memcpy(ptr + len1, str2, len2 * 2);
    ptr[len1 + len2] = 0;

    return ptr;
}

EXPORT_FUNCTION void Concat16Out(const char16_t *str1, const char16_t *str2, const char16_t **out)
{
    const int size = 1024;
    char16_t *ptr = (char16_t *)malloc(size * 2);

    size_t len1 = Length16(str1);
    size_t len2 = Length16(str2);

    memcpy(ptr, str1, len1 * 2);
    memcpy(ptr + len1, str2, len2 * 2);
    ptr[len1 + len2] = 0;

    *out = ptr;
}

EXPORT_FUNCTION FixedString ReturnFixedStr(FixedString str)
{
    return str;
}

EXPORT_FUNCTION FixedWide ReturnFixedWide(FixedWide str)
{
    return str;
}

EXPORT_FUNCTION uint32_t ThroughUInt32UU(uint32_t v)
{
    return v;
}
EXPORT_FUNCTION SingleU32 ThroughUInt32SS(SingleU32 s)
{
    return s;
}
EXPORT_FUNCTION SingleU32 ThroughUInt32SU(uint32_t v)
{
    SingleU32 s;
    s.v = v;
    return s;
}
EXPORT_FUNCTION uint32_t ThroughUInt32US(SingleU32 s)
{
    return s.v;
}

EXPORT_FUNCTION uint64_t ThroughUInt64UU(uint64_t v)
{
    return v;
}
EXPORT_FUNCTION SingleU64 ThroughUInt64SS(SingleU64 s)
{
    return s;
}
EXPORT_FUNCTION SingleU64 ThroughUInt64SU(uint64_t v)
{
    SingleU64 s;
    s.v = v;
    return s;
}
EXPORT_FUNCTION uint64_t ThroughUInt64US(SingleU64 s)
{
    return s.v;
}

EXPORT_FUNCTION int64_t ThroughInt64II(int64_t v)
{
    return v;
}
EXPORT_FUNCTION SingleI64 ThroughInt64SS(SingleI64 s)
{
    return s;
}
EXPORT_FUNCTION SingleI64 ThroughInt64SI(int64_t v)
{
    SingleI64 s;
    s.v = v;
    return s;
}
EXPORT_FUNCTION int64_t ThroughInt64IS(SingleI64 s)
{
    return s.v;
}

EXPORT_FUNCTION int CallJS(const char *str, int (*cb)(const char *str))
{
    char buf[64];
    snprintf(buf, sizeof(buf), "Hello %s!", str);
    return cb(buf);
}

EXPORT_FUNCTION float CallRecursiveJS(int i, float (*func)(int i, const char *str, double d))
{
    float f = func(i, "Hello!", 42.0);
    return f;
}

EXPORT_FUNCTION BFG ModifyBFG(int x, double y, const char *str, BFG (*func)(BFG bfg), BFG *p)
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

EXPORT_FUNCTION IntContainer ArrayToStruct(int *values, int len)
{
    IntContainer ic;

    memset(&ic, 0, sizeof(ic));
    memcpy(ic.values, values, len * sizeof(*values));
    ic.len = len;

    return ic;
}

EXPORT_FUNCTION void FillRange(int init, int step, int *out, int len)
{
    while (--len >= 0) {
        *(out++) = init;
        init += step;
    }
}

EXPORT_FUNCTION void MultiplyIntegers(int multiplier, int *values, int len)
{
    for (int i = 0; i < len; i++) {
        values[i] *= multiplier;
    }
}

EXPORT_FUNCTION const char *ThroughStr(StrStruct s)
{
    return s.str;
}

EXPORT_FUNCTION const char16_t *ThroughStr16(StrStruct s)
{
    return s.str16;
}

EXPORT_FUNCTION const char *ThroughStrStar(StrStruct *s)
{
    return s->str;
}

EXPORT_FUNCTION const char16_t *ThroughStrStar16(StrStruct *s)
{
    return s->str16;
}

EXPORT_FUNCTION void ReverseBytes(void *p, int len)
{
    uint8_t *bytes = (uint8_t *)p;

    for (int i = 0; i < len / 2; i++) {
        uint8_t tmp = bytes[i];
        bytes[i] = bytes[len - i - 1];
        bytes[len - i - 1] = tmp;
    }
}

EXPORT_FUNCTION void CopyEndianInts1(EndianInts ints, uint8_t buf[56])
{
    memcpy(buf, &ints, sizeof(ints));
}

EXPORT_FUNCTION void CopyEndianInts2(int16_t i16le, int16_t i16be, uint16_t u16le, uint16_t u16be,
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

EXPORT_FUNCTION uint16_t ReturnEndianInt2(uint16_t v) { return v; }
EXPORT_FUNCTION uint32_t ReturnEndianInt4(uint32_t v) { return v; }
EXPORT_FUNCTION uint64_t ReturnEndianInt8(uint64_t v) { return v; }

EXPORT_FUNCTION BigText ReverseBigText(BigText buf)
{
    BigText ret;
    for (int i = 0; i < sizeof(buf.text); i++) {
        ret.text[sizeof(buf.text) - 1 - i] = buf.text[i];
    }
    return ret;
}

EXPORT_FUNCTION size_t UpperCaseStrAscii(const char *str, char *out)
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

EXPORT_FUNCTION size_t UpperCaseStrAscii16(const char16_t *str, char16_t *out)
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

EXPORT_FUNCTION void ChangeDirectory(const char *dirname)
{
    chdir(dirname);
}

EXPORT_FUNCTION void UpperToInternalBuffer(const char *str, char **ptr)
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

EXPORT_FUNCTION int ComputeLengthUntilNul(const void *ptr)
{
    return (int)strlen((const char *)ptr);
}

static size_t WideStringLength(const char16_t *str16)
{
    size_t len = 0;

    while (str16[len]) {
        len++;
    }

    return len;
}

EXPORT_FUNCTION int ComputeLengthUntilNulWide(const int16_t *ptr)
{
    return (int)WideStringLength((const char16_t *)ptr);
}

EXPORT_FUNCTION void ReverseStringVoid(void *ptr)
{
    char *str = (char *)ptr;
    size_t len = strlen(str);

    for (size_t i = 0; i < len / 2; i++) {
        char tmp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = tmp;
    }
}

EXPORT_FUNCTION void ReverseString16Void(void *ptr)
{
    char16_t *str16 = (char16_t *)ptr;
    size_t len = WideStringLength(str16);

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

EXPORT_FUNCTION BinaryIntFunc *GetBinaryIntFunction(const char *type)
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

EXPORT_FUNCTION VariadicIntFunc *GetVariadicIntFunction(const char *type)
{
    if (!strcmp(type, "add")) {
        return AddIntN;
    } else if (!strcmp(type, "multiply")) {
        return MultiplyIntN;
    } else {
        return NULL;
    }
}

EXPORT_FUNCTION void FillBufferDirect(BufferInfo buf, int c)
{
    memset(buf.ptr, c, buf.len);
}

EXPORT_FUNCTION void FillBufferIndirect(const BufferInfo *buf, int c)
{
    memset(buf->ptr, c, buf->len);
}

EXPORT_FUNCTION const char *GetLatin1String()
{
    // ®²
    return "Microsoft\xAE\xB2";
}

EXPORT_FUNCTION int BoolToInt(bool a)
{
    return (int)a;
}

EXPORT_FUNCTION unsigned int BoolToMask12(bool a, bool b, bool c, bool d, bool e, bool f,
                                 bool g, bool h, bool i, bool j, bool k, bool l)
{
    unsigned int mask = ((unsigned int)a << 11) | ((unsigned int)b << 10) | ((unsigned int)c << 9) |
                        ((unsigned int)d << 8) | ((unsigned int)e << 7) | ((unsigned int)f << 6) |
                        ((unsigned int)g << 5) | ((unsigned int)h << 4) | ((unsigned int)i << 3) |
                        ((unsigned int)j << 2) | ((unsigned int)k << 1) | ((unsigned int)l << 0);
    return mask;
}

EXPORT_FUNCTION int IfElseInt(bool cond, int a, int b)
{
    return cond ? a : b;
}

EXPORT_FUNCTION const char *IfElseStr(const char *a, const char *b, bool cond)
{
    return cond ? a : b;
}

EXPORT_FUNCTION int GetSymbolInt()
{
    return sym_int;
}

EXPORT_FUNCTION const char *GetSymbolStr()
{
    return sym_str;
}

EXPORT_FUNCTION void GetSymbolInt3(int out[3])
{
    out[0] = sym_int3[0];
    out[1] = sym_int3[1];
    out[2] = sym_int3[2];
}

EXPORT_FUNCTION void WriteConfigure(char16_t *buf, int max)
{
    assert(max > 0);

    write_ptr = buf;
    write_max = max - 1;
}

EXPORT_FUNCTION void WriteString(const char16_t *str)
{
    int len = 0;

    while (str[len] && len < write_max) {
        write_ptr[len] = str[len];
        len++;
    }

    write_ptr[len] = 0;
}

EXPORT_FUNCTION bool ReturnBool(int cond)
{
    bool ret = DoReturnBool(cond);
    assert(ret == !!cond);
    return ret;
}

EXPORT_FUNCTION int ReturnEnumValue(Enum1 e)
{
    return (int)e;
}

template<typename T>
static const char *GetEnumPrimitive()
{
    using U = typename std::underlying_type<T>::type;

    switch (sizeof(U)) {
        case 1: return std::is_signed<U>::value ? "Int8" : "UInt8";
        case 2: return std::is_signed<U>::value ? "Int16" : "UInt16";
        case 4: return std::is_signed<U>::value ? "Int32" : "UInt32";
        case 8: return std::is_signed<U>::value ? "Int64" : "UInt64";
    }

    return nullptr;
}

EXPORT_FUNCTION const char *GetEnumPrimitive1() { return GetEnumPrimitive<Enum1>(); }
EXPORT_FUNCTION const char *GetEnumPrimitive2() { return GetEnumPrimitive<Enum2>(); }
EXPORT_FUNCTION const char *GetEnumPrimitive3() { return GetEnumPrimitive<Enum3>(); }
EXPORT_FUNCTION const char *GetEnumPrimitive4() { return GetEnumPrimitive<Enum4>(); }
EXPORT_FUNCTION const char *GetEnumPrimitive5() { return GetEnumPrimitive<Enum5>(); }
