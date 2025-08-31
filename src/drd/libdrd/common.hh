// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"

namespace K {

enum class drd_Sector: int8_t {
    Public,
    Private
};
static const char *const drd_SectorNames[] = {
    "Public",
    "Private"
};

union alignas(8) drd_DiagnosisCode {
    int64_t value;
    char str[7];

    drd_DiagnosisCode() = default;

    static drd_DiagnosisCode Parse(Span<const char> str, unsigned int flags = K_DEFAULT_PARSE_FLAGS,
                                   Span<const char> *out_remaining = nullptr)
    {
        drd_DiagnosisCode code = {};
        Size end = 0;
        {
            Size copy_len = std::min(K_SIZE(code.str) - 1, str.len);
            for (; end < copy_len && str[end] != ' '; end++) {
                code.str[end] = UpperAscii(str[end]);
            }

            bool valid = (str.len >= 3 && (!(flags & (int)ParseFlag::End) ||
                                           str.len < 7 || str[end] == ' ')) &&
                         IsAsciiAlpha(code.str[0]) && IsAsciiDigit(code.str[1]) &&
                         IsAsciiDigit(code.str[2]);
            if (valid) [[likely]] {
                Size real_end = 3;
                while (code.str[real_end]) {
                    valid &= IsAsciiDigit(code.str[real_end]) ||
                             (real_end < 5 && code.str[real_end] == '+');
                    real_end++;
                }
                while (real_end > 3 && code.str[--real_end] == '+') {
                    code.str[real_end] = '\0';
                }
            }

            if (!valid) [[unlikely]] {
                if (flags & (int)ParseFlag::Log) {
                    LogError("Malformed diagnosis code '%1'", str);
                }
                code.value = 0;
            }
        }

        if (out_remaining) {
            *out_remaining = str.Take(end, str.len - end);
        }
        return code;
    }

    constexpr bool IsValid() const { return value; }

    constexpr bool operator==(drd_DiagnosisCode other) const { return value == other.value; }
    constexpr bool operator!=(drd_DiagnosisCode other) const { return value != other.value; }

    bool operator<(drd_DiagnosisCode other) const { return CmpStr(str, other.str) < 0; }
    bool operator<=(drd_DiagnosisCode other) const { return CmpStr(str, other.str) <= 0; }
    bool operator>(drd_DiagnosisCode other) const { return CmpStr(str, other.str) > 0; }
    bool operator>=(drd_DiagnosisCode other) const { return CmpStr(str, other.str) >= 0; }

    bool Matches(const char *other_str) const
    {
        Size i = 0;
        while (str[i] == other_str[i] && str[i]) {
            i++;
        }
        return !other_str[i];
    }
    bool Matches(drd_DiagnosisCode other) const { return Matches(other.str); }

    operator FmtArg() const { return FmtArg(str); }

    constexpr uint64_t Hash() const { return HashTraits<const char *>::Hash(str); }
};

union alignas(8) drd_ProcedureCode {
    int64_t value;
    char str[8];

    drd_ProcedureCode() = default;

    static drd_ProcedureCode Parse(Span<const char> str, unsigned int flags = K_DEFAULT_PARSE_FLAGS,
                                   Span<const char> *out_remaining = nullptr)
    {
        drd_ProcedureCode code = {};
        {
            Size copy_len = std::min(K_SIZE(str) - 1, str.len);
            for (Size i = 0; i < copy_len; i++) {
                code.str[i] = UpperAscii(str[i]);
            }

            bool valid = (flags & (int)ParseFlag::End ? str.len == 7 : str.len >= 7) &&
                         IsAsciiAlpha(code.str[0]) && IsAsciiAlpha(code.str[1]) &&
                         IsAsciiAlpha(code.str[2]) && IsAsciiAlpha(code.str[3]) &&
                         IsAsciiDigit(code.str[4]) && IsAsciiDigit(code.str[5]) &&
                         IsAsciiDigit(code.str[6]);
            if (!valid) [[unlikely]] {
                if (flags & (int)ParseFlag::Log) {
                    LogError("Malformed procedure code '%1'", str);
                }
                code.value = 0;
                return code;
            }
        }

        if (out_remaining) {
            *out_remaining = str.Take(7, str.len - 7);
        }
        return code;
    }

    constexpr bool IsValid() const { return value; }

    constexpr bool operator==(drd_ProcedureCode other) const { return value == other.value; }
    constexpr bool operator!=(drd_ProcedureCode other) const { return value != other.value; }

    bool operator<(drd_ProcedureCode other) const { return CmpStr(str, other.str) < 0; }
    bool operator<=(drd_ProcedureCode other) const { return CmpStr(str, other.str) <= 0; }
    bool operator>(drd_ProcedureCode other) const { return CmpStr(str, other.str) > 0; }
    bool operator>=(drd_ProcedureCode other) const { return CmpStr(str, other.str) >= 0; }

    operator FmtArg() const { return FmtArg(str); }

    constexpr uint64_t Hash() const { return HashTraits<const char *>::Hash(str); }
};

struct drd_UnitCode {
    int16_t number;

    drd_UnitCode() = default;
    explicit drd_UnitCode(int16_t code) : number(code) {}

    static drd_UnitCode Parse(Span<const char> str, unsigned int flags = K_DEFAULT_PARSE_FLAGS,
                              Span<const char> *out_remaining = nullptr)
    {
        drd_UnitCode code = {};

        if (!ParseInt(str, &code.number, flags & ~(int)ParseFlag::Log, out_remaining) ||
                ((flags & (int)ParseFlag::Validate) && !code.IsValid())) {
            if (flags & (int)ParseFlag::Log) {
                LogError("Malformed Unit code '%1'", str);
            }
            code.number = 0;
        }

        return code;
    }

    constexpr bool IsValid() const { return number > 0 && number <= 9999; }

    constexpr bool operator==(drd_UnitCode other) const { return number == other.number; }
    constexpr bool operator!=(drd_UnitCode other) const { return number != other.number; }

    bool operator<(drd_UnitCode other) const { return number < other.number; }
    bool operator<=(drd_UnitCode other) const { return number <= other.number; }
    bool operator>(drd_UnitCode other) const { return number > other.number; }
    bool operator>=(drd_UnitCode other) const { return number >= other.number; }

    operator FmtArg() const { return FmtArg(number); }

    constexpr uint64_t Hash() const { return HashTraits<int16_t>::Hash(number); }
};

struct drd_ListMask {
    int16_t offset;
    uint8_t value;
};

}
