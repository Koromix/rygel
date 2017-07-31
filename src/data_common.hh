#pragma once

#include "kutil.hh"

union GhmRootCode {
    char str[6];
    uint64_t value;

    bool operator==(const GhmRootCode &other) const { return value == other.value; }
    bool operator!=(const GhmRootCode &other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};
union GhmCode {
    char str[7];
    uint64_t value;

    bool operator==(const GhmCode &other) const { return value == other.value; }
    bool operator!=(const GhmCode &other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};
struct GhsCode {
    uint16_t value;

    bool operator==(const GhsCode &other) const { return value == other.value; }
    bool operator!=(const GhsCode &other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(value); }
};
union DiagnosisCode {
    char str[7];
    uint64_t value;

    bool operator==(const DiagnosisCode &other) const { return value == other.value; }
    bool operator!=(const DiagnosisCode &other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};
union ProcedureCode {
    char str[8];
    uint64_t value;

    bool operator==(const ProcedureCode &other) const { return value == other.value; }
    bool operator!=(const ProcedureCode &other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};
