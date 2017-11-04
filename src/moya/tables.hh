/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "codes.hh"

enum class TableType: uint32_t {
    UnknownTable,

    GhmDecisionTree,
    DiagnosisTable,
    ProcedureTable,
    GhmRootTable,
    SeverityTable,

    GhsTable,
    AuthorizationTable,
    SrcPairTable
};
static const char *const TableTypeNames[] = {
    "Unknown Table",

    "GHM Decision Tree",
    "Diagnosis Table",
    "Procedure Table",
    "GHM Root Table",
    "Severity Table",

    "GHS Table",
    "Authorization Table",
    "SRC Pair Table"
};

struct ListMask {
    int16_t offset;
    uint8_t value;
};

struct TableInfo {
    struct Section {
        Size raw_offset;
        Size raw_len;
        Size values_count;
        Size value_len;
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
            uint8_t function; // Switch to dedicated enum
            uint8_t params[2];
            Size children_count;
            Size children_idx;
        } test;

        struct {
            GhmCode ghm;
            int16_t error;
        } ghm;
    } u;
};

struct DiagnosisInfo {
    enum class Flag {
        SexDifference = 1
    };

    DiagnosisCode diag;

    uint16_t flags;
    struct Attributes {
        uint8_t raw[37];

        uint8_t cmd;
        uint8_t jump;
        uint8_t severity;
    } attributes[2];
    uint16_t warnings;

    uint16_t exclusion_set_idx;
    ListMask cma_exclusion_mask;

    const Attributes &Attributes(Sex sex) const
    {
        StaticAssert((int)Sex::Male == 1);
        return attributes[(int)sex - 1];
    }

    HASH_SET_HANDLER(DiagnosisInfo, diag);
};

struct ExclusionInfo {
    uint8_t raw[256];
};

struct ProcedureInfo {
    ProcedureCode proc;
    int8_t phase;

    Date limit_dates[2];
    uint8_t bytes[55];

    HASH_SET_HANDLER(ProcedureInfo, proc);
};

template <Size N>
struct ValueRangeCell {
    struct {
        int min;
        int max;
    } limits[N];
    int value;

    bool Test(Size idx, int value) const
    {
        DebugAssert(idx < N);
        return (value >= limits[idx].min && value < limits[idx].max);
    }
};

struct GhmRootInfo {
    GhmRootCode ghm_root;

    int8_t confirm_duration_treshold;

    bool allow_ambulatory;
    int8_t short_duration_treshold;

    int8_t young_severity_limit;
    int8_t young_age_treshold;
    int8_t old_severity_limit;
    int8_t old_age_treshold;

    int8_t childbirth_severity_list;

    ListMask cma_exclusion_mask;

    HASH_SET_HANDLER(GhmRootInfo, ghm_root);
};

struct GhsInfo {
    GhmCode ghm;
    GhsCode ghs[2]; // 0 for public, 1 for private

    int8_t bed_authorization;
    int8_t unit_authorization;
    int8_t minimal_duration;

    int8_t minimal_age;

    ListMask main_diagnosis_mask;
    ListMask diagnosis_mask;
    LocalArray<ListMask, 4> procedure_masks;

    HASH_SET_HANDLER_N(GhmHandler, GhsInfo, ghm);
    HASH_SET_HANDLER_N(GhmRootHandler, GhsInfo, ghm.Root());
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

struct SrcPair {
    DiagnosisCode diag;
    ProcedureCode proc;
};

Date ConvertDate1980(uint16_t days);

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
                        const TableInfo &table, int section_idx,
                        HeapArray<ValueRangeCell<2>> *out_cells);

bool ParseGhsTable(const uint8_t *file_data, const char *filename,
                   const TableInfo &table, HeapArray<GhsInfo> *out_nodes);
bool ParseAuthorizationTable(const uint8_t *file_data, const char *filename,
                             const TableInfo &table, HeapArray<AuthorizationInfo> *out_auths);
bool ParseSrcPairTable(const uint8_t *file_data, const char *filename,
                       const TableInfo &table, int section_idx,
                       HeapArray<SrcPair> *out_pairs);

struct TableIndex {
    Date limit_dates[2];

    const TableInfo *tables[ARRAY_SIZE(TableTypeNames)];
    uint32_t changed_tables;

    ArrayRef<GhmDecisionNode> ghm_nodes;
    ArrayRef<DiagnosisInfo> diagnoses;
    ArrayRef<ExclusionInfo> exclusions;
    ArrayRef<ProcedureInfo> procedures;
    ArrayRef<GhmRootInfo> ghm_roots;
    ArrayRef<ValueRangeCell<2>> gnn_cells;
    ArrayRef<ValueRangeCell<2>> cma_cells[3];

    ArrayRef<GhsInfo> ghs;
    ArrayRef<AuthorizationInfo> authorizations;
    ArrayRef<SrcPair> src_pairs[2];

    HashSet<DiagnosisCode, const DiagnosisInfo *> *diagnoses_map;
    HashSet<ProcedureCode, const ProcedureInfo *> *procedures_map;
    HashSet<GhmRootCode, const GhmRootInfo *> *ghm_roots_map;

    HashSet<GhmCode, const GhsInfo *, GhsInfo::GhmHandler> *ghm_to_ghs_map;
    HashSet<GhmRootCode, const GhsInfo *, GhsInfo::GhmRootHandler> *ghm_root_to_ghs_map;

    const DiagnosisInfo *FindDiagnosis(DiagnosisCode code) const;
    ArrayRef<const ProcedureInfo> FindProcedure(ProcedureCode code) const;
    const ProcedureInfo *FindProcedure(ProcedureCode code, int8_t phase, Date date) const;
    const GhmRootInfo *FindGhmRoot(GhmRootCode code) const;

    ArrayRef<const GhsInfo> FindCompatibleGhs(GhmRootCode ghm_root) const;
    ArrayRef<const GhsInfo> FindCompatibleGhs(GhmCode ghm) const;
};

class TableSet {
public:
    HeapArray<TableInfo> tables;
    HeapArray<TableIndex> indexes;

    struct {
        HeapArray<GhmDecisionNode> ghm_nodes;
        HeapArray<DiagnosisInfo> diagnoses;
        HeapArray<ExclusionInfo> exclusions;
        HeapArray<ProcedureInfo> procedures;
        HeapArray<GhmRootInfo> ghm_roots;
        HeapArray<ValueRangeCell<2>> gnn_cells;
        HeapArray<ValueRangeCell<2>> cma_cells[3];

        HeapArray<GhsInfo> ghs;
        HeapArray<AuthorizationInfo> authorizations;
        HeapArray<SrcPair> src_pairs[2];
    } store;

    struct {
        HeapArray<HashSet<DiagnosisCode, const DiagnosisInfo *>> diagnoses;
        HeapArray<HashSet<ProcedureCode, const ProcedureInfo *>> procedures;
        HeapArray<HashSet<GhmRootCode, const GhmRootInfo *>> ghm_roots;

        HeapArray<HashSet<GhmCode, const GhsInfo *, GhsInfo::GhmHandler>> ghm_to_ghs;
        HeapArray<HashSet<GhmRootCode, const GhsInfo *, GhsInfo::GhmRootHandler>> ghm_root_to_ghs;
    } maps;

    const TableIndex *FindIndex(Date date) const;
    TableIndex *FindIndex(Date date)
        { return (TableIndex *)((const TableSet *)this)->FindIndex(date); }
};

bool LoadTableFiles(ArrayRef<const char *const> filenames, TableSet *out_set);
