// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "common.hh"

union mco_GhmRootCode {
    int32_t value;
    struct {
        int8_t cmd;
        char type;
        int8_t seq;
    } parts;

    mco_GhmRootCode() = default;

    static mco_GhmRootCode FromString(Span<const char> str, int flags = DEFAULT_PARSE_FLAGS,
                                      Span<const char> *out_remaining = nullptr)
    {
        mco_GhmRootCode code = {};
        {
            bool valid = (flags & (int)ParseFlag::End ? str.len == 5 : str.len >= 5) &&
                         IsAsciiDigit(str[0]) && IsAsciiDigit(str[1]) && IsAsciiAlpha(str[2]) &&
                         IsAsciiDigit(str[3]) && IsAsciiDigit(str[4]);
            if (UNLIKELY(!valid)) {
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

    bool IsValid() const { return value; }
    bool IsError() const { return parts.cmd == 90; }

    bool operator==(mco_GhmRootCode other) const { return value == other.value; }
    bool operator!=(mco_GhmRootCode other) const { return value != other.value; }

    operator FmtArg() const
    {
        FmtArg arg;
        arg.type = FmtArg::Type::StrBuf;
        // We need to be fast here (at least for drdR), sprintf is too slow
        arg.value.str_buf[0] = (char)('0' + (parts.cmd / 10));
        arg.value.str_buf[1] = (char)('0' + (parts.cmd % 10));
        arg.value.str_buf[2] = parts.type;
        arg.value.str_buf[3] = (char)('0' + (parts.seq / 10));
        arg.value.str_buf[4] = (char)('0' + (parts.seq % 10));
        arg.value.str_buf[5] = 0;
        return arg;
    }
};
static inline uint64_t DefaultHash(mco_GhmRootCode code) { return DefaultHash(code.value); }
static inline bool DefaultCompare(mco_GhmRootCode code1, mco_GhmRootCode code2)
    { return code1 == code2; }

union mco_GhmCode {
    int32_t value;
    struct {
        int8_t cmd;
        char type;
        int8_t seq;
        char mode;
    } parts;

    mco_GhmCode() = default;

    static mco_GhmCode FromString(Span<const char> str, int flags = DEFAULT_PARSE_FLAGS,
                                  Span<const char> *out_remaining = nullptr)
    {
        mco_GhmCode code = {};
        {
            bool valid = (str.len >= 5) && (!(flags & (int)ParseFlag::End) || str.len < 7) &&
                         IsAsciiDigit(str[0]) && IsAsciiDigit(str[1]) && IsAsciiAlpha(str[2]) &&
                         IsAsciiDigit(str[3]) && IsAsciiDigit(str[4]) &&
                         (str.len == 5 || str[5] == ' ' || IsAsciiAlphaOrDigit(str[5]));
            if (UNLIKELY(!valid)) {
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

    bool IsValid() const { return value; }
    bool IsError() const { return parts.cmd == 90; }
    
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

    bool operator==(mco_GhmCode other) const { return value == other.value; }
    bool operator!=(mco_GhmCode other) const { return value != other.value; }

    operator FmtArg() const
    {
        FmtArg arg;
        arg.type = FmtArg::Type::StrBuf;
        // We need to be fast here (at least for drdR), sprintf is too slow
        arg.value.str_buf[0] = (char)('0' + (parts.cmd / 10));
        arg.value.str_buf[1] = (char)('0' + (parts.cmd % 10));
        arg.value.str_buf[2] = parts.type;
        arg.value.str_buf[3] = (char)('0' + (parts.seq / 10));
        arg.value.str_buf[4] = (char)('0' + (parts.seq % 10));
        arg.value.str_buf[5] = parts.mode;
        arg.value.str_buf[6] = 0;
        return arg;
    }

    mco_GhmRootCode Root() const
    {
        mco_GhmRootCode ghm_root = {};
        ghm_root.parts.cmd = parts.cmd;
        ghm_root.parts.type = parts.type;
        ghm_root.parts.seq = parts.seq;
        return ghm_root;
    }
};
static inline uint64_t DefaultHash(mco_GhmCode code) { return DefaultHash(code.value); }
static inline bool DefaultCompare(mco_GhmCode code1, mco_GhmCode code2)
    { return code1 == code2; }

struct mco_GhsCode {
    int16_t number;

    mco_GhsCode() = default;
    explicit mco_GhsCode(int16_t number) : number(number) {}

    static mco_GhsCode FromString(Span<const char> str, int flags = DEFAULT_PARSE_FLAGS,
                                  Span<const char> *out_remaining = nullptr)
    {
        mco_GhsCode code = {};

        Size end;
        {
            int value = 0;
            for (end = 0; end < str.len; end++) {
                int digit = str[end] - '0';
                if ((unsigned int)digit > 9) {
                    if (flags & (int)ParseFlag::End || !end) {
                        if (flags & (int)ParseFlag::Log) {
                            LogError("Malformed GHS code '%1'", str);
                        }
                        return code;
                    }
                    break;
                }
                value = (value * 10) + digit;
                if (value > INT16_MAX) {
                    if (flags & (int)ParseFlag::Log) {
                        LogError("GHS code '%1' is too big", str);
                    }
                    return code;
                }
            }
            code.number = (int16_t)value;
        }

        if (out_remaining) {
            *out_remaining = str.Take(end, str.len - end);
        }
        return code;
    }

    bool IsValid() const { return number; }

    bool operator==(mco_GhsCode other) const { return number == other.number; }
    bool operator!=(mco_GhsCode other) const { return number != other.number; }

    operator FmtArg() const { return FmtArg(number); }
};
static inline uint64_t DefaultHash(mco_GhsCode code) { return DefaultHash(code.number); }
static inline bool DefaultCompare(mco_GhsCode code1, mco_GhsCode code2)
    { return code1 == code2; }

enum class mco_SupplementType {
    Rea,
    Reasi,
    Si,
    Src,
    Nn1,
    Nn2,
    Nn3,
    Rep
};
static const char *const mco_SupplementTypeNames[] = {
    "REA",
    "REASI",
    "SI",
    "SRC",
    "NN1",
    "NN2",
    "NN3",
    "REP"
};

template <typename T>
union mco_SupplementCounters {
    T values[ARRAY_SIZE(mco_SupplementTypeNames)] = {};
    struct {
        T rea;
        T reasi;
        T si;
        T src;
        T nn1;
        T nn2;
        T nn3;
        T rep;
    } st;
    StaticAssert(SIZE(mco_SupplementCounters::values) == SIZE(mco_SupplementCounters::st));

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
        return st.rea == other.st.rea &&
               st.reasi == other.st.reasi &&
               st.si == other.st.si &&
               st.src == other.st.src &&
               st.nn1 == other.st.nn1 &&
               st.nn2 == other.st.nn2 &&
               st.nn3 == other.st.nn3 &&
               st.rep == other.st.rep;
    }
    template <typename U>
    bool operator !=(const mco_SupplementCounters<U> &other) const { return !(*this == other); }
};
