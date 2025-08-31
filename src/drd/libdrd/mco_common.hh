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
#include "common.hh"

namespace K {

union mco_GhmRootCode {
    int32_t value;
    struct {
#if defined(K_BIG_ENDIAN)
        int8_t cmd;
        char type;
        int8_t seq;
        int8_t pad1;
#else
        int8_t pad1;
        int8_t seq;
        char type;
        int8_t cmd;
#endif
    } parts;

    mco_GhmRootCode() = default;
#if defined(K_BIG_ENDIAN)
    constexpr mco_GhmRootCode(int8_t cmd, char type, int8_t seq)
        : parts { cmd, type, seq, 0 } {}
#else
    constexpr mco_GhmRootCode(int8_t cmd, char type, int8_t seq)
        : parts { 0, seq, type, cmd } {}
#endif

    static mco_GhmRootCode Parse(Span<const char> str, unsigned int flags = K_DEFAULT_PARSE_FLAGS,
                                 Span<const char> *out_remaining = nullptr)
    {
        mco_GhmRootCode code = {};
        {
            bool valid = (flags & (int)ParseFlag::End ? str.len == 5 : str.len >= 5) &&
                         IsAsciiDigit(str[0]) && IsAsciiDigit(str[1]) && IsAsciiAlpha(str[2]) &&
                         IsAsciiDigit(str[3]) && IsAsciiDigit(str[4]);
            if (!valid) [[unlikely]] {
                if (flags & (int)ParseFlag::Log) {
                    LogError("Malformed GHM root code '%1'", str);
                }
                return code;
            }

            code.parts.cmd = (int8_t)(10 * (str[0] - '0') + (str[1] - '0'));
            code.parts.type = UpperAscii(str[2]);
            code.parts.seq = (int8_t)(10 * (str[3] - '0') + (str[4] - '0'));
        }

        if (out_remaining) {
            *out_remaining = str.Take(5, str.len - 5);
        }
        return code;
    }

    constexpr bool IsValid() const { return value; }
    constexpr bool IsError() const { return parts.cmd == 90; }

    constexpr bool operator==(mco_GhmRootCode other) const { return value == other.value; }
    constexpr bool operator!=(mco_GhmRootCode other) const { return value != other.value; }

    bool operator<(mco_GhmRootCode other) const { return value < other.value; }
    bool operator<=(mco_GhmRootCode other) const { return value <= other.value; }
    bool operator>(mco_GhmRootCode other) const { return value > other.value; }
    bool operator>=(mco_GhmRootCode other) const { return value >= other.value; }

    template <Size N>
    Span<char> ToString(char (&buf)[N]) const
    {
        static_assert(N >= 6);

        if (IsValid()) [[likely]] {
            // We need to be fast here (at least for drdR), sprintf is too slow
            buf[0] = (char)('0' + (parts.cmd / 10));
            buf[1] = (char)('0' + (parts.cmd % 10));
            buf[2] = parts.type;
            buf[3] = (char)('0' + (parts.seq / 10));
            buf[4] = (char)('0' + (parts.seq % 10));
            buf[5] = 0;

            return MakeSpan(buf, 5);
        } else {
            buf[0] = '?';
            buf[1] = 0;
            return MakeSpan(buf, 1);
        }
    }

    operator FmtArg() const
    {
        FmtArg arg;
        arg.type = FmtType::Buffer;
        ToString(arg.u.buf);
        return arg;
    }

    constexpr uint64_t Hash() const { return HashTraits<int32_t>::Hash(value); }
};

union mco_GhmCode {
    int32_t value;
    struct {
#if defined(K_BIG_ENDIAN)
        int8_t cmd;
        char type;
        int8_t seq;
        char mode;
#else
        char mode;
        int8_t seq;
        char type;
        int8_t cmd;
#endif
    } parts;

    mco_GhmCode() = default;
#if defined(K_BIG_ENDIAN)
    constexpr mco_GhmCode(int8_t cmd, char type, int8_t seq, char mode)
        : parts { cmd, type, seq, mode } {}
#else
    constexpr mco_GhmCode(int8_t cmd, char type, int8_t seq, char mode)
        : parts { mode, seq, type, cmd } {}
#endif

    static mco_GhmCode Parse(Span<const char> str, unsigned int flags = K_DEFAULT_PARSE_FLAGS,
                             Span<const char> *out_remaining = nullptr)
    {
        mco_GhmCode code = {};
        {
            bool valid = (str.len >= 5) && (!(flags & (int)ParseFlag::End) || str.len < 7) &&
                         IsAsciiDigit(str[0]) && IsAsciiDigit(str[1]) && IsAsciiAlpha(str[2]) &&
                         IsAsciiDigit(str[3]) && IsAsciiDigit(str[4]) &&
                         (str.len == 5 || str[5] == ' ' || IsAsciiAlphaOrDigit(str[5]));
            if (!valid) [[unlikely]] {
                if (flags & (int)ParseFlag::Log) {
                    LogError("Malformed GHM code '%1'", str);
                }
                return code;
            }

            code.parts.cmd = (int8_t)(10 * (str[0] - '0') + (str[1] - '0'));
            code.parts.type = UpperAscii(str[2]);
            code.parts.seq = (int8_t)(10 * (str[3] - '0') + (str[4] - '0'));
            if (str.len >= 6) {
                code.parts.mode = UpperAscii(str[5]);
            }
        }

        if (out_remaining) {
            *out_remaining = str.Take(6, str.len - 6);
        }
        return code;
    }

    constexpr bool IsValid() const { return value; }
    constexpr bool IsError() const { return parts.cmd == 90; }

    int Severity() const
    {
        if (parts.mode >= '1' && parts.mode < '5') {
            return parts.mode - '1';
        } else if (parts.mode >= 'A' && parts.mode < 'E') {
            return parts.mode - 'A';
        } else {
            return 0;
        }
    }

    constexpr bool operator==(mco_GhmCode other) const { return value == other.value; }
    constexpr bool operator!=(mco_GhmCode other) const { return value != other.value; }

    bool operator<(mco_GhmCode other) const { return value < other.value; }
    bool operator<=(mco_GhmCode other) const { return value <= other.value; }
    bool operator>(mco_GhmCode other) const { return value > other.value; }
    bool operator>=(mco_GhmCode other) const { return value >= other.value; }

    template <Size N>
    Span<char> ToString(char (&buf)[N]) const
    {
        static_assert(N >= 6);

        if (IsValid()) [[likely]] {
            // We need to be fast here (at least for drdR), sprintf is too slow
            buf[0] = (char)('0' + (parts.cmd / 10));
            buf[1] = (char)('0' + (parts.cmd % 10));
            buf[2] = parts.type;
            buf[3] = (char)('0' + (parts.seq / 10));
            buf[4] = (char)('0' + (parts.seq % 10));
            buf[5] = parts.mode;
            buf[6] = 0;
            return MakeSpan(buf, 6);
        } else {
            buf[0] = '?';
            buf[1] = 0;
            return MakeSpan(buf, 1);
        }
    }

    operator FmtArg() const
    {
        FmtArg arg;
        arg.type = FmtType::Buffer;
        ToString(arg.u.buf);
        return arg;
    }

    constexpr mco_GhmRootCode Root() const
    {
        mco_GhmRootCode ghm_root = {};
        ghm_root.parts.cmd = parts.cmd;
        ghm_root.parts.type = parts.type;
        ghm_root.parts.seq = parts.seq;
        return ghm_root;
    }

    constexpr uint64_t Hash() const { return HashTraits<int32_t>::Hash(value); }
};

struct mco_GhsCode {
    int16_t number;

    mco_GhsCode() = default;
    explicit mco_GhsCode(int16_t number) : number(number) {}

    static mco_GhsCode Parse(Span<const char> str, unsigned int flags = K_DEFAULT_PARSE_FLAGS,
                             Span<const char> *out_remaining = nullptr)
    {
        mco_GhsCode code = {};

        if (!ParseInt(str, &code.number, flags & ~(int)ParseFlag::Log, out_remaining) ||
                ((flags & (int)ParseFlag::Validate) && !code.IsValid())) {
            if (flags & (int)ParseFlag::Log) {
                LogError("Malformed GHS code '%1'", str);
            }
            code.number = 0;
        }

        return code;
    }

    constexpr bool IsValid() const { return number > 0; }

    constexpr bool operator==(mco_GhsCode other) const { return number == other.number; }
    constexpr bool operator!=(mco_GhsCode other) const { return number != other.number; }

    bool operator<(mco_GhsCode other) const { return number < other.number; }
    bool operator<=(mco_GhsCode other) const { return number <= other.number; }
    bool operator>(mco_GhsCode other) const { return number > other.number; }
    bool operator>=(mco_GhsCode other) const { return number >= other.number; }

    operator FmtArg() const { return FmtArg(number); }

    constexpr uint64_t Hash() const { return HashTraits<int16_t>::Hash(number); }
};

enum class mco_SupplementType {
    Rea,
    Reasi,
    Si,
    Src,
    Nn1,
    Nn2,
    Nn3,
    Rep,

    Ohb,
    Aph,
    Ant,
    Rap,
    Dia,
    Dip,
    Ent1,
    Ent2,
    Ent3,
    Sdc
};
static const char *const mco_SupplementTypeNames[] = {
    "REA",
    "REASI",
    "SI",
    "SRC",
    "NN1",
    "NN2",
    "NN3",
    "REP",

    "OHB",
    "APH",
    "ANT",
    "RAP",
    "DIA",
    "DIP",
    "ENT1",
    "ENT2",
    "ENT3",
    "SDC"
};

template <typename T>
union mco_SupplementCounters {
    T values[K_LEN(mco_SupplementTypeNames)];
    struct {
        T rea;
        T reasi;
        T si;
        T src;
        T nn1;
        T nn2;
        T nn3;
        T rep;

        T ohb;
        T aph;
        T ant;
        T rap;
        T dia;
        T dip;
        T ent1;
        T ent2;
        T ent3;
        T sdc;
    } st;
    static_assert(K_SIZE(mco_SupplementCounters::values) == K_SIZE(mco_SupplementCounters::st));

    template <typename U>
    mco_SupplementCounters &operator+=(const mco_SupplementCounters<U> &other)
    {
        st.rea += other.st.rea;
        st.reasi += other.st.reasi;
        st.si += other.st.si;
        st.src += other.st.src;
        st.nn1 += other.st.nn1;
        st.nn2 += other.st.nn2;
        st.nn3 += other.st.nn3;
        st.rep += other.st.rep;

        st.ohb += other.st.ohb;
        st.aph += other.st.aph;
        st.ant += other.st.ant;
        st.rap += other.st.rap;
        st.dia += other.st.dia;
        st.dip += other.st.dip;
        st.ent1 += other.st.ent1;
        st.ent2 += other.st.ent2;
        st.ent3 += other.st.ent3;
        st.sdc += other.st.sdc;

        return *this;
    }
    template <typename U>
    mco_SupplementCounters operator+(const mco_SupplementCounters<U> &other)
    {
        mco_SupplementCounters copy = *this;
        copy += other;
        return copy;
    }

    template <typename U>
    bool operator==(const mco_SupplementCounters<U> &other) const
    {
        return !memcmp(this, &other, K_SIZE(*this));
    }
    template <typename U>
    bool operator !=(const mco_SupplementCounters<U> &other) const { return !(*this == other); }
};

}
