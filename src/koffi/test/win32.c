// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include <stdlib.h>

#define IMPORT __declspec(dllimport)
#define EXPORT __declspec(dllexport)

IMPORT int __cdecl DoDivideBySafe1(int a, int b);
IMPORT int __cdecl DoDivideBySafe2(int a, int b);

EXPORT int DivideBySafe1(int a, int b)
{
    return DoDivideBySafe1(a, b);
}

EXPORT int DivideBySafe2(int a, int b)
{
    return DoDivideBySafe2(a, b);
}

EXPORT int CallThrough(int (__stdcall *func)(int value), int value)
{
    int ret = func(value);
    return ret;
}
