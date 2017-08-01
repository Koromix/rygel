#pragma once

#include "kutil.hh"
#include "data_common.hh"

enum class TableType {
    UnknownTable,

    GhmDecisionTree,
    DiagnosisTable,
    ProcedureTable,
    GhmRootTable,
    SeverityTable,

    GhsDecisionTree,
    AuthorizationTable,
    SupplementPairTable
};
static const char *const TableTypeNames[] = {
    "Unknown Table",

    "GHM Decision Tree",
    "Diagnosis Table",
    "Procedure Table",
    "GHM Root Table",
    "Severity Table",

    "GHS Decision Tree",
    "Unit Reference Table",
    "Supplement Pair Table"
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

    char raw_type[9];
    TableType type;

    LocalArray<Section, 16> sections;
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
            uint8_t params[2];
            size_t children_count;
            // TODO: Switch to relative indexes (for GHS nodes too)
            size_t children_idx;
        } test;

        struct {
            int error;
            GhmCode code;
        } ghm;
    } u;
};

struct DiagnosisInfo {
    enum class Flag {
        SexDifference = 1
    };

    DiagnosisCode code;

    uint16_t flags;
    union {
        uint8_t values[48];
        struct {
            uint8_t cmd;
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

//struct BirthSeverityTable {
//    DynamicArray<ValueRangeCell<2>> gnn_table;
//    DynamicArray<ValueRangeCell<2>> cma_tables[3];
//};

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

struct GhsDecisionNode {
    enum class Type {
        Ghm,
        Test,
        Ghs
    };

    Type type;
    union {
        struct {
            GhmCode code;
            size_t next_ghm_idx;
        } ghm;

        struct {
            int8_t function;
            uint8_t params[2];
            size_t fail_goto_idx;
        } test;

        struct {
            GhsCode code;
            int16_t high_duration_treshold;
            int16_t low_duration_treshold;
        } ghs[2]; // 0 for public, 1 for private
    } u;
};

enum class AuthorizationType: uint8_t {
    Facility,
    Unit,
    Bed
};
static const char *const AuthorizationTypeNames[] = {
    "Facility",
    "Unit",
    "Bed"
};
struct AuthorizationInfo {
    AuthorizationType type;
    int8_t code;
    int8_t function;
};

struct DiagnosisProcedurePair {
    DiagnosisCode diag_code;
    ProcedureCode proc_code;
};

bool ParseTableHeaders(const ArrayRef<const uint8_t> file_data,
                       const char *filename, DynamicArray<TableInfo> *out_tables);

bool ParseGhmDecisionTree(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, DynamicArray<GhmDecisionNode> *out_nodes);
bool ParseDiagnosisTable(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, DynamicArray<DiagnosisInfo> *out_diags);
bool ParseProcedureTable(const uint8_t *file_data, const char *filename,
                         const TableInfo &table, DynamicArray<ProcedureInfo> *out_procs);
bool ParseGhmRootTable(const uint8_t *file_data, const char *filename,
                       const TableInfo &table, DynamicArray<GhmRootInfo> *out_ghm_roots);
bool ParseSeverityTable(const uint8_t *file_data, const char *filename,
                        const TableInfo &table, size_t section_idx,
                        DynamicArray<ValueRangeCell<2>> *out_cells);

bool ParseGhsDecisionTree(const uint8_t *file_data, const char *filename,
                           const TableInfo &table, DynamicArray<GhsDecisionNode> *out_nodes);
bool ParseAuthorizationTable(const uint8_t *file_data, const char *filename,
                             const TableInfo &table, DynamicArray<AuthorizationInfo> *out_units);
bool ParseSupplementPairTable(const uint8_t *file_data, const char *filename,
                              const TableInfo &table, size_t section_idx,
                              DynamicArray<DiagnosisProcedurePair> *out_pairs);

struct ClassifierSet {
    Date limit_dates[2];
    const TableInfo *tables[CountOf(TableTypeNames)];

    ArrayRef<GhmDecisionNode> ghm_nodes;
    ArrayRef<DiagnosisInfo> diagnoses;
    ArrayRef<ProcedureInfo> procedures;
    ArrayRef<GhmRootInfo> ghm_roots;
    ArrayRef<ValueRangeCell<2>> gnn_cells;
    ArrayRef<ValueRangeCell<2>> cma_cells[3];

    ArrayRef<GhsDecisionNode> ghs_nodes;
    ArrayRef<AuthorizationInfo> authorizations;
    ArrayRef<DiagnosisProcedurePair> supplement_pairs[2];
};

class ClassifierStore {
public:
    DynamicArray<TableInfo> tables;

    DynamicArray<ClassifierSet> sets;

    DynamicArray<GhmDecisionNode> ghm_nodes;
    DynamicArray<DiagnosisInfo> diagnoses;
    DynamicArray<ProcedureInfo> procedures;
    DynamicArray<GhmRootInfo> ghm_roots;
    DynamicArray<ValueRangeCell<2>> gnn_cells;
    DynamicArray<ValueRangeCell<2>> cma_cells[3];

    DynamicArray<GhsDecisionNode> ghs_nodes;
    DynamicArray<AuthorizationInfo> authorizations;
    DynamicArray<DiagnosisProcedurePair> supplement_pairs[2];

    const ClassifierSet *FindSet(Date date) const;
    ClassifierSet *FindSet(Date date)
        { return (ClassifierSet *)((const ClassifierStore *)this)->FindSet(date); }
};

bool LoadClassifierStore(ArrayRef<const char *const> filenames, ClassifierStore *out_store);
