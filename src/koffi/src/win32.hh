// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "src/core/libcc/libcc.hh"

#include <intrin.h>

namespace RG {

#if _WIN64

struct TEB {
    char _pad1[8];
    void *StackBase;
    void *StackLimit;
    char _pad2[5216];
    void *DeallocationStack;
    char _pad3[712];
    uint32_t GuaranteedStackBytes;
};
RG_STATIC_ASSERT(RG_OFFSET_OF(TEB, DeallocationStack) == 0x1478);
RG_STATIC_ASSERT(RG_OFFSET_OF(TEB, GuaranteedStackBytes) == 0x1748);

#else

struct TEB {
    char _pad1[4];
    void *StackBase;
    void *StackLimit;
    char _pad2[3584];
    void *DeallocationStack;
    char _pad3[360];
    uint32_t GuaranteedStackBytes;
};
RG_STATIC_ASSERT(RG_OFFSET_OF(TEB, DeallocationStack) == 0xE0C);
RG_STATIC_ASSERT(RG_OFFSET_OF(TEB, GuaranteedStackBytes) == 0x0F78);

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
