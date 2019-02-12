// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

enum class Sector: int8_t {
    Public,
    Private
};
static const char *const SectorNames[] = {
    "Public",
    "Private"
};

union DiagnosisCode {
    int64_t value;
    char str[7];

    DiagnosisCode() = default;

    static DiagnosisCode FromString(Span<const char> str, int flags = DEFAULT_PARSE_FLAGS,
                                    Span<const char> *out_remaining = nullptr)
    {
        DiagnosisCode code = {};
        Size end = 0;
        {
            Size copy_len = std::min(SIZE(code.str) - 1, str.len);
            for (; end < copy_len && str[end] != ' '; end++) {
                code.str[end] = UpperAscii(str[end]);
            }

            bool valid = (str.len >= 3 && (!(flags & (int)ParseFlag::End) ||
                                           str.len < 7 || str[end] == ' ')) &&
                         IsAsciiAlpha(code.str[0]) && IsAsciiDigit(code.str[1]) &&
                         IsAsciiDigit(code.str[2]);
            if (LIKELY(valid)) {
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

            if (UNLIKELY(!valid)) {
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

    bool IsValid() const { return value; }

    bool operator==(DiagnosisCode other) const { return value == other.value; }
    bool operator!=(DiagnosisCode other) const { return value != other.value; }

    bool operator<(DiagnosisCode other) const { return CmpStr(str, other.str) < 0; }
    bool operator<=(DiagnosisCode other) const { return CmpStr(str, other.str) <= 0; }
    bool operator>(DiagnosisCode other) const { return CmpStr(str, other.str) > 0; }
    bool operator>=(DiagnosisCode other) const { return CmpStr(str, other.str) >= 0; }

    bool Matches(const char *other_str) const
    {
        Size i = 0;
        while (str[i] == other_str[i] && str[i]) {
            i++;
        }
        return !other_str[i];
    }
    bool Matches(DiagnosisCode other) const { return Matches(other.str); }

    operator FmtArg() const { return FmtArg(str); }

    uint64_t Hash() const { return HashTraits<const char *>::Hash(str); }
};

union ProcedureCode {
    int64_t value;
    char str[8];

    ProcedureCode() = default;

    static ProcedureCode FromString(Span<const char> str, int flags = DEFAULT_PARSE_FLAGS,
                                    Span<const char> *out_remaining = nullptr)
    {
        ProcedureCode code = {};
        {
            Size copy_len = std::min(SIZE(str) - 1, str.len);
            for (Size i = 0; i < copy_len; i++) {
                code.str[i] = UpperAscii(str[i]);
            }

            bool valid = (flags & (int)ParseFlag::End ? str.len == 7 : str.len >= 7) &&
                         IsAsciiAlpha(code.str[0]) && IsAsciiAlpha(code.str[1]) &&
                         IsAsciiAlpha(code.str[2]) && IsAsciiAlpha(code.str[3]) &&
                         IsAsciiDigit(code.str[4]) && IsAsciiDigit(code.str[5]) &&
                         IsAsciiDigit(code.str[6]);
            if (UNLIKELY(!valid)) {
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

    bool IsValid() const { return value; }

    bool operator==(ProcedureCode other) const { return value == other.value; }
    bool operator!=(ProcedureCode other) const { return value != other.value; }

    bool operator<(ProcedureCode other) const { return CmpStr(str, other.str) < 0; }
    bool operator<=(ProcedureCode other) const { return CmpStr(str, other.str) <= 0; }
    bool operator>(ProcedureCode other) const { return CmpStr(str, other.str) > 0; }
    bool operator>=(ProcedureCode other) const { return CmpStr(str, other.str) >= 0; }

    operator FmtArg() const { return FmtArg(str); }

    uint64_t Hash() const { return HashTraits<const char *>::Hash(str); }
};

struct UnitCode {
    int16_t number;

    UnitCode() = default;
    explicit UnitCode(int16_t code) : number(code) {}

    static UnitCode FromString(Span<const char> str, int flags = DEFAULT_PARSE_FLAGS,
                                  Span<const char> *out_remaining = nullptr)
    {
        UnitCode code = {};

        if (!ParseDec(str, &code.number, flags & ~(int)ParseFlag::Log, out_remaining) ||
                ((flags & (int)ParseFlag::Validate) && !code.IsValid())) {
            if (flags & (int)ParseFlag::Log) {
                LogError("Malformed Unit code '%1'", str);
            }
            code.number = 0;
        }

        return code;
    }

    bool IsValid() const { return number > 0 && number <= 9999; }

    bool operator==(UnitCode other) const { return number == other.number; }
    bool operator!=(UnitCode other) const { return number != other.number; }

    bool operator<(UnitCode other) const { return number < other.number; }
    bool operator<=(UnitCode other) const { return number <= other.number; }
    bool operator>(UnitCode other) const { return number > other.number; }
    bool operator>=(UnitCode other) const { return number >= other.number; }

    operator FmtArg() const { return FmtArg(number); }

    uint64_t Hash() const { return HashTraits<int16_t>::Hash(number); }
};

struct ListMask {
    int16_t offset;
    uint8_t value;
};
