// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include <stdbool.h>

#if defined(__x86_64__)

__attribute__((naked)) void DoReturnBool(int cond)
{
    // Return an ABI-compliant but weird bool where bits 8 to 31 are set.
    // On x86_64, only the least signifiant byte matters for bools and the rest is explicitly undefined.

    __asm__ (
        "cmpl $0, %edi\n"
        "setne %r10b\n"
        "movl $0xFFFFFFFF, %eax\n"
        "andb %r10b, %al\n"
        "ret\n"
    );
}

#else

bool DoReturnBool(int cond)
{
    return cond;
}

#endif
