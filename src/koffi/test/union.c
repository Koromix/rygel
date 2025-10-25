// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

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

EXPORT float GetMultiDouble(MultiU u) { return u.d; }
EXPORT float GetMultiUnsigned(MultiU u) { return u.u; }
