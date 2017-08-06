#pragma once

#include "kutil.hh"

union GhmRootCode {
    uint64_t value;
    char str[6];

    GhmRootCode() = default;
    explicit GhmRootCode(const char *code_str)
    {
        if (code_str) {
            strncpy(str, code_str, sizeof(str));
            str[sizeof(str) - 1] = '\0';
        } else {
            value = 0;
        }
    }

    bool operator==(GhmRootCode other) const { return value == other.value; }
    bool operator!=(GhmRootCode other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};

union GhmCode {
    uint64_t value;
    char str[7];

    GhmCode() = default;
    explicit GhmCode(const char *code_str)
    {
        if (code_str) {
            strncpy(str, code_str, sizeof(str));
            str[sizeof(str) - 1] = '\0';
        } else {
            value = 0;
        }
    }

    bool operator==(GhmCode other) const { return value == other.value; }
    bool operator!=(GhmCode other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};

union DiagnosisCode {
    uint64_t value;
    char str[7];

    DiagnosisCode() = default;
    explicit DiagnosisCode(const char *code_str)
    {
        value = 0;
        if (code_str) {
            strncpy(str, code_str, sizeof(str));
            str[sizeof(str) - 1] = '\0';
        }
    }

    bool operator==(DiagnosisCode other) const { return value == other.value; }
    bool operator!=(DiagnosisCode other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};
static inline uint64_t DefaultHash(DiagnosisCode code) { return DefaultHash(code.str); }
static inline bool DefaultCompare(DiagnosisCode code1, DiagnosisCode code2)
    { return code1 == code2; }

union ProcedureCode {
    uint64_t value;
    char str[8];

    ProcedureCode() = default;
    explicit ProcedureCode(const char *code_str)
    {
        value = 0;
        if (code_str) {
            strncpy(str, code_str, sizeof(str));
            str[sizeof(str) - 1] = '\0';
        }
    }

    bool operator==(ProcedureCode other) const { return value == other.value; }
    bool operator!=(ProcedureCode other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};
static inline uint64_t DefaultHash(ProcedureCode code) { return DefaultHash(code.str); }
static inline bool DefaultCompare(ProcedureCode code1, ProcedureCode code2)
    { return code1 == code2; }

struct GhsCode {
    uint16_t number;

    GhsCode() = default;
    explicit GhsCode(uint16_t number)
        : number(number) {}

    bool operator==(GhsCode other) const { return number == other.number; }
    bool operator!=(GhsCode other) const { return number != other.number; }

    operator FmtArg() const { return FmtArg(number); }
};
