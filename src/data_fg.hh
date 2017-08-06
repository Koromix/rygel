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
    uint16_t version[2];
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
    } mask[2];
    uint16_t warnings;

    uint16_t exclusion_set_idx;
    uint16_t exclusion_set_bit;

    HASH_SET_HANDLER(DiagnosisInfo, code);
};

struct ExclusionInfo {
    uint8_t mask[256];
};

struct ProcedureInfo {
    ProcedureCode code;
    int8_t phase;

    Date limit_dates[2];
    uint8_t values[55];

    HASH_SET_HANDLER(ProcedureInfo, code);
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
            uint16_t high_duration_treshold;
            uint16_t low_duration_treshold;
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
                       const char *filename, HeapArray<TableInfo> *out_tables);

bool ParseGhmDecisionTree(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, HeapArray<GhmDecisionNode> *out_nodes);
bool ParseDiagnosisTable(const uint8_t *file_data, const char *filename,
                         const TableInfo &table, HeapArray<DiagnosisInfo> *out_diags);
bool ParseExclusionTable(const uint8_t *file_data, const char *filename,
                         const TableInfo &table, HeapArray<ExclusionInfo> *out_exclusions);
bool ParseProcedureTable(const uint8_t *file_data, const char *filename,
                         const TableInfo &table, HeapArray<ProcedureInfo> *out_procs);
bool ParseGhmRootTable(const uint8_t *file_data, const char *filename,
                       const TableInfo &table, HeapArray<GhmRootInfo> *out_ghm_roots);
bool ParseSeverityTable(const uint8_t *file_data, const char *filename,
                        const TableInfo &table, size_t section_idx,
                        HeapArray<ValueRangeCell<2>> *out_cells);

bool ParseGhsDecisionTree(const uint8_t *file_data, const char *filename,
                           const TableInfo &table, HeapArray<GhsDecisionNode> *out_nodes);
bool ParseAuthorizationTable(const uint8_t *file_data, const char *filename,
                             const TableInfo &table, HeapArray<AuthorizationInfo> *out_units);
bool ParseSupplementPairTable(const uint8_t *file_data, const char *filename,
                              const TableInfo &table, size_t section_idx,
                              HeapArray<DiagnosisProcedurePair> *out_pairs);

struct ClassifierIndex {
    Date limit_dates[2];

    const TableInfo *tables[CountOf(TableTypeNames)];
    uint32_t changed_tables;

    ArrayRef<GhmDecisionNode> ghm_nodes;
    ArrayRef<DiagnosisInfo> diagnoses;
    ArrayRef<ExclusionInfo> exclusions;
    ArrayRef<ProcedureInfo> procedures;
    ArrayRef<GhmRootInfo> ghm_roots;
    ArrayRef<ValueRangeCell<2>> gnn_cells;
    ArrayRef<ValueRangeCell<2>> cma_cells[3];

    ArrayRef<GhsDecisionNode> ghs_nodes;
    ArrayRef<AuthorizationInfo> authorizations;
    ArrayRef<DiagnosisProcedurePair> supplement_pairs[2];

    HashSet<DiagnosisCode, const DiagnosisInfo *> *diagnoses_map;
    HashSet<ProcedureCode, const ProcedureInfo *> *procedures_map;

    const DiagnosisInfo *FindDiagnosis(DiagnosisCode code) const;
    ArrayRef<const ProcedureInfo> FindProcedure(ProcedureCode code) const;
    const ProcedureInfo *FindProcedure(ProcedureCode code, int8_t phase, Date date) const;
};

class ClassifierSet {
public:
    HeapArray<TableInfo> tables;
    HeapArray<ClassifierIndex> indexes;

    struct {
        HeapArray<GhmDecisionNode> ghm_nodes;
        HeapArray<DiagnosisInfo> diagnoses;
        HeapArray<ExclusionInfo> exclusions;
        HeapArray<ProcedureInfo> procedures;
        HeapArray<GhmRootInfo> ghm_roots;
        HeapArray<ValueRangeCell<2>> gnn_cells;
        HeapArray<ValueRangeCell<2>> cma_cells[3];

        HeapArray<GhsDecisionNode> ghs_nodes;
        HeapArray<AuthorizationInfo> authorizations;
        HeapArray<DiagnosisProcedurePair> supplement_pairs[2];
    } store;

    struct {
        HeapArray<HashSet<DiagnosisCode, const DiagnosisInfo *>> diagnoses;
        HeapArray<HashSet<ProcedureCode, const ProcedureInfo *>> procedures;
    } maps;

    const ClassifierIndex *FindIndex(Date date) const;
    ClassifierIndex *FindIndex(Date date)
        { return (ClassifierIndex *)((const ClassifierSet *)this)->FindIndex(date); }
};

bool LoadClassifierSet(ArrayRef<const char *const> filenames, ClassifierSet *out_set);
