#pragma once

#include "kutil.hh"

// NOTE: Move into TableInfo or keep separate?
enum class TableType {
    GhmDecisionTree,
    DiagnosticInfo,
    ProcedureInfo,
    GhmRootInfo,
    ChildbirthInfo
};
static const char *const TableTypeNames[] = {
    "GHM Decision Tree",
    "Diagnostic Table",
    "Procedure Table",
    "GHM Root Table",
    "Childbirth Table"
};

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
union DiagnosticCode {
    char str[7];
    uint64_t value;

    bool operator==(const DiagnosticCode &other) const { return value == other.value; }
    bool operator!=(const DiagnosticCode &other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};
union ProcedureCode {
    char str[8];
    uint64_t value;

    bool operator==(const ProcedureCode &other) const { return value == other.value; }
    bool operator!=(const ProcedureCode &other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(str); }
};

struct TableInfo {
    struct Section {
        size_t raw_offset;
        size_t raw_len;
        size_t values_count;
        size_t value_len;
    };

    Date build_date;
    int16_t version[2];
    Date limit_dates[2];

    TableType type;

    LocalArray<Section, 16> sections;
};

struct DiagnosticInfo {
    DiagnosticCode code;

    union {
        uint8_t values[48];
        struct {
            int8_t cmd;
        } info;
    } sex[2];
    uint16_t warnings;

    uint16_t exclusion_set_idx;
    uint16_t exclusion_set_bit;
};

struct ProcedureInfo {
    ProcedureCode code;
    int8_t phase;

    Date limit_dates[2];
    uint8_t values[55];
};

template <size_t N>
struct ValueRangeCell {
    struct {
        int min;
        int max;
    } limits[N];
    int value;
};

struct GhmRootInfo {
    GhmRootCode code;

    int8_t confirm_duration_treshold;

    bool allow_ambulatory;
    int8_t short_duration_treshold;

    int8_t young_severity_limit;
    int8_t young_age_treshold;
    int8_t old_severity_limit;
    int8_t old_age_treshold;

    int8_t childbirth_severity_list;

    int8_t cma_exclusion_offset;
    uint8_t cma_exclusion_mask;
};

struct GhmDecisionNode {
    enum class Type {
        Test,
        Ghm
    };

    Type type;
    union {
        struct {
            int8_t function; // Switch to dedicated enum
            int8_t params[2];
            size_t children_count;
            size_t children_idx;
        } test;

        struct {
            int error;
            GhmCode code;
        } ghm;
    } u;
};

bool ParseTableHeaders(const uint8_t *file_data, size_t file_len,
                       const char *filename, DynamicArray<TableInfo> *out_tables);
bool ParseGhmDecisionTree(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, DynamicArray<GhmDecisionNode> *out_nodes);
bool ParseDiagnosticTable(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, DynamicArray<DiagnosticInfo> *out_diags);
bool ParseProcedureTable(const uint8_t *file_data, const char *filename,
                         const TableInfo &table, DynamicArray<ProcedureInfo> *out_procs);
bool ParseGhmRootTable(const uint8_t *file_data, const char *filename,
                       const TableInfo &table, DynamicArray<GhmRootInfo> *out_ghm_roots);
bool ParseValueRangeTable(const uint8_t *file_data, const char *filename,
                          const TableInfo::Section &section,
                          DynamicArray<ValueRangeCell<2>> *out_cells);
