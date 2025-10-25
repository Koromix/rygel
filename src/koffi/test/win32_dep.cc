// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include <stdlib.h>
#include <stdexcept>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define EXPORT extern "C" __declspec(dllexport)

EXPORT int __cdecl DoDivideBySafe1(int a, int b)
{
    __try {
        return a / b;
    } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        return -42;
    }
}

static int InnerDivide2(int a, int b)
{
    try {
        if (b == 0)
            throw std::invalid_argument("cannot divide by 0");
        return a / b;
    } catch (std::invalid_argument &) {
        return -42;
    }
}

EXPORT int __cdecl DoDivideBySafe2(int a, int b)
{
    return InnerDivide2(a, b);
}
