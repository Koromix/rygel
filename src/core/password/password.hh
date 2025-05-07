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

#pragma once

#include "src/core/base/base.hh"

namespace RG {

enum class pwd_CheckFlag {
    Classes = 1 << 0,
    Score = 1 << 1
};

enum class pwd_GenerateFlag {
    Uppers = 1 << 0,
    UppersNoAmbi = 1 << 1,
    Lowers = 1 << 2,
    LowersNoAmbi = 1 << 3,
    Digits = 1 << 4,
    DigitsNoAmbi = 1 << 5,
    Specials = 1 << 6,
    Dangerous = 1 << 7,

    Check = 1 << 8
};

static const Size pwd_MinLength = 8;
static const Size pwd_MaxLength = 256;

bool pwd_CheckPassword(Span<const char> password, Span<const char *const> blacklist, unsigned int flags = UINT_MAX);
static inline bool pwd_CheckPassword(Span<const char> password, unsigned int flags = UINT_MAX)
    { return pwd_CheckPassword(password, {}, flags); }

bool pwd_GeneratePassword(unsigned int flags, Span<char> out_password);
static inline bool pwd_GeneratePassword(Span<char> out_password)
    { return pwd_GeneratePassword(UINT_MAX, out_password); }

}
