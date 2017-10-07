/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"

enum class Sex: uint8_t {
    Male = 1,
    Female
};
static const char *const SexNames[] = {
    "Male",
    "Female"
};

union GhmRootCode {
    int32_t value;
    struct {
        int8_t cmd;
        char type;
        int8_t seq;
    } parts;

    GhmRootCode() = default;

    static GhmRootCode FromString(const char *str, bool errors = true)
    {
        GhmRootCode code;

        code.value = 0;
        if (str[0]) {
            int end_offset;
            sscanf(str, "%02" SCNu8 "%c%02" SCNu8 "%n",
                   &code.parts.cmd, &code.parts.type, &code.parts.seq, &end_offset);
            if (end_offset != 5 || str[5]) {
                if (errors) {
                    LogError("Malformed GHM root code '%1'", str);
                }
                code.value = 0;
            }
            code.parts.type = UpperAscii(code.parts.type);
        }

        return code;
    }

    bool IsValid() const { return value; }
    bool IsError() const { return parts.cmd == 90; }

    bool operator==(GhmRootCode other) const { return value == other.value; }
    bool operator!=(GhmRootCode other) const { return value != other.value; }

    operator FmtArg() const
    {
        FmtArg arg;
        arg.type = FmtArg::Type::StrBuf;
        // TODO: Improve Fmt API to avoid snprintf everywhere
        snprintf(arg.value.str_buf, sizeof(arg.value.str_buf), "%02" PRIu8 "%c%02" PRIu16,
                 parts.cmd, parts.type, parts.seq);
        return arg;
    }
};
static inline uint64_t DefaultHash(GhmRootCode code) { return DefaultHash(code.value); }
static inline bool DefaultCompare(GhmRootCode code1, GhmRootCode code2)
    { return code1 == code2; }

union GhmCode {
    int32_t value;
    struct {
        int8_t cmd;
        char type;
        int8_t seq;
        char mode;
    } parts;

    GhmCode() = default;

    static GhmCode FromString(const char *str, bool errors = true)
    {
        GhmCode code;

        code.value = 0;
        if (str[0]) {
            int end_offset;
            sscanf(str, "%02" SCNu8 "%c%02" SCNu8 "%n",
                   &code.parts.cmd, &code.parts.type, &code.parts.seq, &end_offset);
            if (end_offset == 5 && (!str[5] || !str[6])) {
                code.parts.mode = str[5];
            } else {
                if (errors) {
                    LogError("Malformed GHM code '%1'", str);
                }
                code.value = 0;
            }
            code.parts.type = UpperAscii(code.parts.type);
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

    bool operator==(GhmCode other) const { return value == other.value; }
    bool operator!=(GhmCode other) const { return value != other.value; }

    operator FmtArg() const
    {
        FmtArg arg;
        arg.type = FmtArg::Type::StrBuf;
        snprintf(arg.value.str_buf, sizeof(arg.value.str_buf), "%02" PRIu8 "%c%02" PRIu16 "%c",
                 parts.cmd, parts.type, parts.seq, parts.mode);
        return arg;
    }

    GhmRootCode Root() const
    {
        GhmRootCode root_code = {};
        root_code.parts.cmd = parts.cmd;
        root_code.parts.type = parts.type;
        root_code.parts.seq = parts.seq;
        return root_code;
    }
};
static inline uint64_t DefaultHash(GhmCode code) { return DefaultHash(code.value); }
static inline bool DefaultCompare(GhmCode code1, GhmCode code2)
    { return code1 == code2; }

union DiagnosisCode {
    int64_t value;
    char str[7];

    DiagnosisCode() = default;

    static DiagnosisCode FromString(const char *str, bool errors = true)
    {
        DiagnosisCode code;

        code.value = 0;
        if (str[0]) {
            for (size_t i = 0; i < sizeof(code.str) - 1 && str[i] && str[i] != ' '; i++) {
                code.str[i] = UpperAscii(str[i]);
            }

            bool valid = (IsAsciiAlpha(code.str[0]) && IsAsciiDigit(code.str[1]) &&
                          IsAsciiDigit(code.str[2]));
            if (valid) {
                size_t end = 3;
                while (code.str[end]) {
                    valid &= (IsAsciiDigit(code.str[end]) || (end < 5 && code.str[end] == '+'));
                    end++;
                }
                while (end > 3 && code.str[--end] == '+') {
                    code.str[end] = '\0';
                }
            }

            if (!valid) {
                if (errors) {
                    LogError("Malformed diagnosis code '%1'", str);
                }
                code.value = 0;
            }
        }

        return code;
    }

    bool IsValid() const { return value; }

    bool operator==(DiagnosisCode other) const { return value == other.value; }
    bool operator!=(DiagnosisCode other) const { return value != other.value; }

    bool Matches(const char *other_str) const
    {
        size_t i = 0;
        while (str[i] && other_str[i] && str[i] == other_str[i]) {
            i++;
        }
        return !other_str[i];
    }
    bool Matches(DiagnosisCode other) const { return Matches(other.str); }

    operator FmtArg() const { return FmtArg(str); }
};
static inline uint64_t DefaultHash(DiagnosisCode code) { return DefaultHash(code.str); }
static inline bool DefaultCompare(DiagnosisCode code1, DiagnosisCode code2)
    { return code1 == code2; }

union ProcedureCode {
    int64_t value;
    char str[8];

    ProcedureCode() = default;

    static ProcedureCode FromString(const char *str, bool errors = true)
    {
        ProcedureCode code;

        code.value = 0;
        if (str[0]) {
            for (size_t i = 0; i < sizeof(code.str) - 1 && str[i] && str[i] != ' '; i++) {
                code.str[i] = UpperAscii(str[i]);
            }

            bool valid = (IsAsciiAlpha(code.str[0]) && IsAsciiAlpha(code.str[1]) &&
                          IsAsciiAlpha(code.str[2]) && IsAsciiAlpha(code.str[3]) &&
                          IsAsciiDigit(code.str[4]) && IsAsciiDigit(code.str[5]) &&
                          IsAsciiDigit(code.str[6]) && !code.str[7]);
            if (!valid) {
                if (errors) {
                    LogError("Malformed procedure code '%1'", str);
                }
                code.value = 0;
            }
        }

        return code;
    }

    bool IsValid() const { return value; }

    bool operator==(ProcedureCode other) const { return value == other.value; }
    bool operator!=(ProcedureCode other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};
static inline uint64_t DefaultHash(ProcedureCode code) { return DefaultHash(code.str); }
static inline bool DefaultCompare(ProcedureCode code1, ProcedureCode code2)
    { return code1 == code2; }

struct GhsCode {
    int16_t number;

    GhsCode() = default;
    explicit GhsCode(int16_t number) : number(number) {}

    static GhsCode FromString(const char *str, bool errors = true)
    {
        GhsCode code;

        char *end_ptr;
        errno = 0;
        unsigned long l = strtoul(str, &end_ptr, 10);
        if (!errno && !end_ptr[0] && l <= INT16_MAX) {
            code.number = (int16_t)l;
        } else {
            if (errors) {
                LogError("Malformed GHS code '%1'", str);
            }
            code.number = 0;
        }

        return code;
    }

    bool IsValid() const { return number; }

    bool operator==(GhsCode other) const { return number == other.number; }
    bool operator!=(GhsCode other) const { return number != other.number; }

    operator FmtArg() const { return FmtArg(number); }
};
static inline uint64_t DefaultHash(GhsCode code) { return DefaultHash(code.number); }
static inline bool DefaultCompare(GhsCode code1, GhsCode code2)
    { return code1 == code2; }

struct UnitCode {
    int16_t number;

    UnitCode() = default;
    explicit UnitCode(uint32_t code) : number(code) {}

    bool IsValid() const { return number; }

    bool operator==(const UnitCode &other) const { return number == other.number; }
    bool operator!=(const UnitCode &other) const { return number != other.number; }

    operator FmtArg() const { return FmtArg(number); }
};
static inline uint64_t DefaultHash(UnitCode code) { return DefaultHash(code.number); }
static inline bool DefaultCompare(UnitCode code1, UnitCode code2)
    { return code1 == code2; }
