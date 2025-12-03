// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

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

static const Size pwd_MinLength = 10;
static const Size pwd_MaxLength = 256;

bool pwd_CheckPassword(Span<const char> password, Span<const char *const> blacklist, unsigned int flags = UINT_MAX);
static inline bool pwd_CheckPassword(Span<const char> password, unsigned int flags = UINT_MAX)
    { return pwd_CheckPassword(password, {}, flags); }

bool pwd_GeneratePassword(unsigned int flags, Span<char> out_password);
static inline bool pwd_GeneratePassword(Span<char> out_password)
    { return pwd_GeneratePassword(UINT_MAX, out_password); }

}
