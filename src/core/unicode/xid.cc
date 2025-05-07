// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/base/base.hh"
#include "xid.inc"

namespace RG {

static bool TestUnicodeTable(Span<const int32_t> table, int32_t uc)
{
    RG_ASSERT(table.len > 0);
    RG_ASSERT(table.len % 2 == 0);

    auto it = std::upper_bound(table.begin(), table.end(), uc,
                               [](int32_t uc, int32_t x) { return uc < x; });
    Size idx = it - table.ptr;

    // Each pair of value in table represents a valid interval
    return idx & 0x1;
}

bool IsXidStart(int32_t uc)
{
    bool match = IsAsciiAlpha(uc) || uc == '_' || TestUnicodeTable(XidStartTable, uc);
    return match;
}

bool IsXidContinue(int32_t uc)
{
    bool match = IsAsciiAlphaOrDigit(uc) || uc == '_' || TestUnicodeTable(XidContinueTable, uc);
    return match;
}

}
