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

typedef union SingleU {
    float f;
} SingleU;

typedef union DualU {
    double d;
    uint64_t u;
} DualU;

typedef union MultiU {
    double d;
    float f2[2];
    uint64_t u;
    uint8_t raw[8];
    struct {
        short a;
        char b;
        char c;
        int d;
    } st;
} MultiU;

EXPORT SingleU MakeSingleU(float f)
{
    SingleU u;

    u.f = f;

    return u;
}

EXPORT void MakeSingleUIndirect(float f, SingleU *out)
{
    out->f = f;
}

EXPORT DualU MakeDualU(double d)
{
    DualU u;

    u.d = d;

    return u;
}

EXPORT void MakeDualUIndirect(double d, DualU *out)
{
    out->d = d;
}

EXPORT MultiU MakeMultiU(float a, float b)
{
    MultiU u;

    u.f2[0] = a;
    u.f2[1] = b;

    return u;
}

EXPORT void MakeMultiUIndirect(float a, float b, MultiU *out)
{
    out->f2[0] = a;
    out->f2[1] = b;
}

EXPORT float GetMultiDouble(MultiU u) { return (float)u.d; }
EXPORT float GetMultiUnsigned(MultiU u) { return (float)u.u; }
