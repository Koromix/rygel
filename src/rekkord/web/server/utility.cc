// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "utility.hh"

namespace K {

bool IsMailValid(Span<const char> str)
{
    const auto test_char = [](char c) { return strchr("<>& ", c) || IsAsciiControl(c); };

    Span<const char> domain;
    Span<const char> prefix = SplitStr(str, '@', &domain);

    if (!prefix.len || !domain.len)
        return false;
    if (std::any_of(prefix.begin(), prefix.end(), test_char))
        return false;
    if (std::any_of(domain.begin(), domain.end(), test_char))
        return false;

    return true;
}

bool IsEnumValid(Span<const char> str, Span<const char *const> values)
{
    for (const char *value: values) {
        if (str == value)
            return true;
    }
    return false;
}

bool IsStringValid(Span<const char> str)
{
    const auto test_char = [](char c) { return !IsAsciiControl(c); };

    if (!str.len)
        return false;
    if (!std::all_of(str.begin(), str.end(), test_char))
        return false;
    if (IsAsciiWhite(str[0]) || IsAsciiWhite(str[str.len - 1]))
        return false;

    return true;
}

}
