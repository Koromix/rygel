#pragma once

#include "kutil.hh"

enum class TableType {
    DecisionTree,
    DiagnosticInfo,
    ProcedureInfo,
    GhmRootInfo,
    ChildbirthInfo
};
static const char *const TableTypeNames[] = {
    "Decision Tree",
    "Diagnostic Table",
    "Procedure Table",
    "GHM Root Table",
    "Childbirth Table"
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
    union {
        char str[7];
        uint64_t value;
    } code;

    union {
        uint8_t values[48];
        struct {
            uint8_t cmd;
        } info;
    } sex[2];
    uint16_t warnings;

    uint16_t exclusion_set_bit;
    uint16_t exclusion_set_idx;
};

struct ProcedureInfo {
    union {
        char str[8];
        uint64_t value;
    } code;
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
    union {
        char str[6];
        uint64_t value;
    } code;

    uint8_t confirm_duration_treshold;

    bool allow_ambulatory;
    uint8_t short_duration_treshold;

    uint8_t young_severity_limit;
    uint8_t young_age_treshold;
    uint8_t old_severity_limit;
    uint8_t old_age_treshold;

    uint8_t childbirth_severity_list;

    uint8_t cma_exclusion_offset;
    uint8_t cma_exclusion_mask;
};

struct DecisionNode {
    enum class Type {
        Test,
        Leaf
    };

    Type type;
    union {
        struct {
            int function; // Switch to dedicated enum
            int params[2];
            size_t children_count;
            size_t children_idx;
        } test;

        struct {
            char ghm[8];
            int error;
        } leaf;
    } u;
};

bool ParseTableHeaders(const uint8_t *file_data, size_t file_len,
                       const char *filename, DynamicArray<TableInfo> *out_tables);
bool ParseDecisionTree(const uint8_t *file_data, const char *filename,
                       const TableInfo &table, DynamicArray<DecisionNode> *out_nodes);
bool ParseDiagnosticTable(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, DynamicArray<DiagnosticInfo> *out_diags);
bool ParseProcedureTable(const uint8_t *file_data, const char *filename,
                         const TableInfo &table, DynamicArray<ProcedureInfo> *out_procedures);
bool ParseValueRangeTable(const uint8_t *file_data, const char *filename,
                          const TableInfo::Section &section,
                          DynamicArray<ValueRangeCell<2>> *out_cells);
bool ParseGhmRootTable(const uint8_t *file_data, const char *filename,
                       const TableInfo &table, DynamicArray<GhmRootInfo> *out_ghm_roots);
