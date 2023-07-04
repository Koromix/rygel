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

#pragma once

#include "src/core/libcc/libcc.hh"

#include <intrin.h>

namespace RG {

#if _WIN64

struct TEB {
    void *ExceptionList;
    void *StackBase;
    void *StackLimit;
    char _pad1[5216];
    void *DeallocationStack;
    char _pad2[712];
    uint32_t GuaranteedStackBytes;
};
static_assert(RG_OFFSET_OF(TEB, DeallocationStack) == 0x1478);
static_assert(RG_OFFSET_OF(TEB, GuaranteedStackBytes) == 0x1748);

#else

struct TEB {
    void *ExceptionList;
    void *StackBase;
    void *StackLimit;
    char _pad1[3584];
    void *DeallocationStack;
    char _pad2[360];
    uint32_t GuaranteedStackBytes;
};
static_assert(RG_OFFSET_OF(TEB, DeallocationStack) == 0xE0C);
static_assert(RG_OFFSET_OF(TEB, GuaranteedStackBytes) == 0x0F78);

#endif

static inline TEB *GetTEB()
{
#if defined(__aarch64__) || defined(_M_ARM64)
    TEB *teb = (TEB *)__getReg(18);
#elif defined(__x86_64__) || defined(_M_AMD64)
    TEB *teb = (TEB *)__readgsqword(0x30);
#else
    TEB *teb = (TEB *)__readfsdword(0x18);
#endif

    return teb;
}

}
