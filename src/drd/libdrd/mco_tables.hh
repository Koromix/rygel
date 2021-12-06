// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "mco_common.hh"

namespace RG {

enum class mco_TableType: uint32_t {
    UnknownTable,

    GhmDecisionTree,
    DiagnosisTable,
    ProcedureTable,
    ProcedureAdditionTable,
    ProcedureExtensionTable,
    GhmRootTable,
    SeverityTable,
    GhmToGhsTable,
    AuthorizationTable,
    SrcPairTable,
    PriceTablePublic,
    PriceTablePrivate,
    GhsMinorationTable
};
static const char *const mco_TableTypeNames[] = {
    "Unknown Table",

    "GHM Decision Tree",
    "Diagnosis Table",
    "Procedure Table",
    "Procedure Addition Table",
    "Procedure Extension Table",
    "GHM Root Table",
    "Severity Table",
    "GHM To GHS Table",
    "Authorization Table",
    "SRC Pair Table",
    "Price Table (public)",
    "Price Table (private)",
    "GHS Minoration Table"
};

struct mco_TableInfo {
    struct Section {
        Size raw_offset;
        Size raw_len;
        Size values_count;
        Size value_len;
    };

    const char *filename;
    Date build_date;
    uint16_t version[2];
    Date limit_dates[2];

    char raw_type[9];
    mco_TableType type;

    LocalArray<Section, 16> sections;
};

struct mco_GhmDecisionNode {
    uint8_t function; // XXX: Switch to dedicated enum?

    union {
        struct {
            uint8_t params[2];
            Size children_count;
            Size children_idx;
        } test;

        struct { // Function 12
            mco_GhmCode ghm;
            int16_t error;
        } ghm;
    } u;
};

struct mco_DiagnosisInfo {
    drd_DiagnosisCode diag;
    uint8_t sexes;

    uint16_t warnings;

    uint8_t raw[37];

    int8_t cmd;
    int8_t jump;
    int8_t severity;

    int8_t cma_minimum_age;
    int8_t cma_maximum_age;
    uint16_t exclusion_set_idx;
    drd_ListMask cma_exclusion_mask;

    inline uint8_t GetByte(uint8_t byte_idx) const
    {
        RG_ASSERT(byte_idx < RG_SIZE(raw));
        return raw[byte_idx];
    }
    inline bool Test(drd_ListMask mask) const { return GetByte((uint8_t)mask.offset) & mask.value; }
    inline bool Test(uint8_t offset, uint8_t value) const { return GetByte(offset) & value; }

    RG_HASHTABLE_HANDLER(mco_DiagnosisInfo, diag);
};

struct mco_ExclusionInfo {
    uint8_t raw[232];
};

struct mco_ProcedureInfo {
    drd_ProcedureCode proc;
    int8_t phase;
    uint8_t activities;

    Date limit_dates[2];

    int16_t additions[8];
    struct {
        int16_t offset;
        int16_t len;
    } addition_list;

    uint64_t extensions;
    uint64_t disabled_extensions;

    uint8_t bytes[52];

    inline uint8_t GetByte(int16_t byte_idx) const
    {
        RG_ASSERT(byte_idx >= 0 && byte_idx < RG_SIZE(bytes));
        return bytes[byte_idx];
    }
    inline bool Test(drd_ListMask mask) const { return GetByte(mask.offset) & mask.value; }
    inline bool Test(int16_t offset, uint8_t value) const { return GetByte(offset) & value; }

    Span<const char> ActivitiesToStr(Span<char> out_buf) const;
    Span<const char> ExtensionsToStr(Span<char> out_buf) const;

    RG_HASHTABLE_HANDLER(mco_ProcedureInfo, proc);
};

struct mco_ProcedureLink {
    drd_ProcedureCode proc;
    int8_t phase;
    int8_t activity;

    int16_t addition_idx;
};

template <Size N>
struct mco_ValueRangeCell {
    struct {
        int min;
        int max;
    } limits[N];
    int value;

    bool Test(Size idx, int value) const
    {
        RG_ASSERT(idx < N);
        return (value >= limits[idx].min && value < limits[idx].max);
    }
};

struct mco_GhmRootInfo {
    mco_GhmRootCode ghm_root;

    int8_t confirm_duration_threshold;

    bool allow_ambulatory;
    int8_t short_duration_threshold;
    bool allow_raac;
    bool gradated;

    int8_t young_severity_limit;
    int8_t young_age_threshold;
    int8_t old_severity_limit;
    int8_t old_age_threshold;

    int8_t childbirth_severity_list;

    drd_ListMask cma_exclusion_mask;

    RG_HASHTABLE_HANDLER(mco_GhmRootInfo, ghm_root);
};

struct mco_GhmToGhsInfo {
    enum class SpecialMode: int8_t {
        None,
        Diabetes2,
        Diabetes3,
        Outpatient,
        Intermediary
    };

    mco_GhmCode ghm;
    mco_GhsCode ghs[2]; // 0 for public, 1 for private

    int8_t bed_authorization;
    int8_t unit_authorization;
    int8_t minimum_duration;
    int8_t minimum_age;
    SpecialMode special_mode;
    drd_ListMask main_diagnosis_mask;
    drd_ListMask diagnosis_mask;
    LocalArray<drd_ListMask, 4> procedure_masks;

    int8_t conditions_count;

    mco_GhsCode Ghs(drd_Sector sector) const
    {
        RG_STATIC_ASSERT((int)drd_Sector::Public == 0);
        return ghs[(int)sector];
    }

    RG_HASHTABLE_HANDLER(mco_GhmToGhsInfo, ghm);
    RG_HASHTABLE_HANDLER_N(GhmRootHandler, mco_GhmToGhsInfo, ghm.Root());
};

struct mco_GhsPriceInfo {
    enum class Flag {
        ExbOnce = 1 << 0,
        Minoration = 1 << 1
    };

    mco_GhsCode ghs;

    int32_t ghs_cents;
    int16_t exh_threshold;
    int16_t exb_threshold;
    int32_t exh_cents;
    int32_t exb_cents;
    uint16_t flags;

    RG_HASHTABLE_HANDLER(mco_GhsPriceInfo, ghs);
};

enum class mco_AuthorizationScope: int8_t {
    Facility,
    Unit,
    Bed
};
static const char *const mco_AuthorizationScopeNames[] = {
    "Facility",
    "Unit",
    "Bed"
};
struct mco_AuthorizationInfo {
    union {
        int16_t value;
        struct {
            mco_AuthorizationScope scope;
            int8_t code;
        } st;
    } type;
    int8_t function;

    RG_HASHTABLE_HANDLER(mco_AuthorizationInfo, type.value);
};

struct mco_SrcPair {
    drd_DiagnosisCode diag;
    drd_ProcedureCode proc;

    RG_HASHTABLE_HANDLER(mco_SrcPair, diag);
};

Date mco_ConvertDate1980(uint16_t days);
static const Date mco_MaxDate1980 = mco_ConvertDate1980(UINT16_MAX);

struct mco_TableIndex {
    Date limit_dates[2];
    bool valid;

    const mco_TableInfo *tables[RG_LEN(mco_TableTypeNames)];
    uint32_t changed_tables;

    Span<const mco_GhmDecisionNode> ghm_nodes;
    Span<const mco_DiagnosisInfo> diagnoses;
    Span<const mco_ExclusionInfo> exclusions;
    Span<const mco_ProcedureInfo> procedures;
    Span<const mco_ProcedureLink> procedure_links;
    Span<const mco_GhmRootInfo> ghm_roots;
    Span<const mco_ValueRangeCell<2>> gnn_cells;
    Span<const mco_ValueRangeCell<2>> cma_cells[3];

    Span<const mco_GhmToGhsInfo> ghs;
    Span<const mco_AuthorizationInfo> authorizations;
    Span<const mco_SrcPair> src_pairs[2];

    double ghs_coefficient[2];
    Span<const mco_GhsPriceInfo> ghs_prices[2];
    mco_SupplementCounters<int32_t> supplement_prices[2];

    const HashTable<drd_DiagnosisCode, const mco_DiagnosisInfo *> *diagnoses_map;
    const HashTable<drd_ProcedureCode, const mco_ProcedureInfo *> *procedures_map;
    const HashTable<mco_GhmRootCode, const mco_GhmRootInfo *> *ghm_roots_map;

    const HashTable<mco_GhmCode, const mco_GhmToGhsInfo *> *ghm_to_ghs_map;
    const HashTable<mco_GhmRootCode, const mco_GhmToGhsInfo *, mco_GhmToGhsInfo::GhmRootHandler> *ghm_root_to_ghs_map;
    const HashTable<int16_t, const mco_AuthorizationInfo *> *authorizations_map;
    const HashTable<drd_DiagnosisCode, const mco_SrcPair *> *src_pairs_map[2];

    const HashTable<mco_GhsCode, const mco_GhsPriceInfo *> *ghs_prices_map[2];

    Span<const mco_DiagnosisInfo> FindDiagnosis(drd_DiagnosisCode diag) const;
    const mco_DiagnosisInfo *FindDiagnosis(drd_DiagnosisCode diag, int sex) const;
    Span<const mco_ProcedureInfo> FindProcedure(drd_ProcedureCode proc) const;
    const mco_ProcedureInfo *FindProcedure(drd_ProcedureCode proc, int8_t phase, Date date) const;
    const mco_GhmRootInfo *FindGhmRoot(mco_GhmRootCode ghm_root) const;

    Span<const mco_GhmToGhsInfo> FindCompatibleGhs(mco_GhmCode ghm) const;
    Span<const mco_GhmToGhsInfo> FindCompatibleGhs(mco_GhmRootCode ghm_root) const;
    const mco_AuthorizationInfo *FindAuthorization(mco_AuthorizationScope scope, int8_t type) const;

    double GhsCoefficient(drd_Sector sector) const;
    const mco_GhsPriceInfo *FindGhsPrice(mco_GhsCode ghs, drd_Sector sector) const;
    const mco_SupplementCounters<int32_t> &SupplementPrices(drd_Sector sector) const;
};

struct mco_TableSet {
    HeapArray<mco_TableInfo> tables;
    HeapArray<mco_TableIndex> indexes;

    struct {
        BucketArray<HeapArray<mco_GhmDecisionNode>, 16> ghm_nodes;
        BucketArray<HeapArray<mco_DiagnosisInfo>, 16> diagnoses;
        BucketArray<HeapArray<mco_ExclusionInfo>, 16> exclusions;
        BucketArray<HeapArray<mco_ProcedureInfo>, 16> procedures;
        BucketArray<HeapArray<mco_ProcedureLink>, 16> procedure_links;
        BucketArray<HeapArray<mco_GhmRootInfo>, 16> ghm_roots;
        BucketArray<HeapArray<mco_ValueRangeCell<2>>, 16> gnn_cells;
        BucketArray<HeapArray<mco_ValueRangeCell<2>>, 16> cma_cells[3];
        BucketArray<HeapArray<mco_GhmToGhsInfo>, 16> ghs;
        BucketArray<HeapArray<mco_AuthorizationInfo>, 16> authorizations;
        BucketArray<HeapArray<mco_SrcPair>, 16> src_pairs[2];

        BucketArray<HeapArray<mco_GhsPriceInfo>, 16> ghs_prices[2];
    } store;

    struct {
        BucketArray<HashTable<drd_DiagnosisCode, const mco_DiagnosisInfo *>, 16> diagnoses;
        BucketArray<HashTable<drd_ProcedureCode, const mco_ProcedureInfo *>, 16> procedures;
        BucketArray<HashTable<mco_GhmRootCode, const mco_GhmRootInfo *>, 16> ghm_roots;
        BucketArray<HashTable<mco_GhmCode, const mco_GhmToGhsInfo *>, 16> ghm_to_ghs;
        BucketArray<HashTable<mco_GhmRootCode, const mco_GhmToGhsInfo *,
                               mco_GhmToGhsInfo::GhmRootHandler>, 16> ghm_root_to_ghs;
        BucketArray<HashTable<int16_t, const mco_AuthorizationInfo *>, 16> authorizations;
        BucketArray<HashTable<drd_DiagnosisCode, const mco_SrcPair *>, 16> src_pairs;

        BucketArray<HashTable<mco_GhsCode, const mco_GhsPriceInfo *>, 16> ghs_prices[2];
    } maps;

    BlockAllocator str_alloc;

    mco_TableSet() = default;

    const mco_TableIndex *FindIndex(Date date = {}, bool valid_only = true) const;
    mco_TableIndex *FindIndex(Date date = {}, bool valid_only = true)
        { return (mco_TableIndex *)((const mco_TableSet *)this)->FindIndex(date, valid_only); }
};

class mco_TableSetBuilder {
    RG_DELETE_COPY(mco_TableSetBuilder)

    struct TableLoadInfo {
        Size table_idx;
        Span<uint8_t> raw_data;
        Size prev_index_idx;
    };

    LinkedAllocator file_alloc;
    HeapArray<TableLoadInfo> table_loads;

    mco_TableSet set;

public:
    mco_TableSetBuilder() = default;

    bool LoadTab(StreamReader *st);
    bool LoadPrices(StreamReader *st);
    bool LoadFiles(Span<const char *const> filenames);

    bool Finish(mco_TableSet *out_set);

private:
    bool CommitIndex(Date start_date, Date end_date, TableLoadInfo *current_tables[]);
    void HandleDependencies(mco_TableSetBuilder::TableLoadInfo *current_tables[],
                            Span<const std::pair<mco_TableType, mco_TableType>> pairs);
};

bool mco_LoadTableSet(Span<const char *const> table_directories,
                      Span<const char *const> table_filenames,
                      mco_TableSet *out_set);

class mco_ListSpecifier {
public:
    enum class Table {
        Invalid,
        Diagnoses,
        Procedures
    };
    enum class Type {
        All,
        Mask,
        ReverseMask,
        Cmd,
        CmdJump
    };

    Table table;

    Type type = Type::All;
    union {
        struct {
            uint8_t offset;
            uint8_t mask;
            bool reverse;
        } mask;

        int8_t cmd;

        struct {
            int8_t cmd;
            int8_t jump;
        } cmd_jump;
    } u = {};

    mco_ListSpecifier(Table table = Table::Invalid) : table(table) {}

    static mco_ListSpecifier FromString(const char *spec_str);

    bool IsValid() const { return table != Table::Invalid; }

    bool Match(Span<const uint8_t> values) const
    {
        switch (type) {
            case Type::All: { return true; } break;

            case Type::Mask: {
                return RG_LIKELY(u.mask.offset < values.len) &&
                       values[u.mask.offset] & u.mask.mask;
            } break;

            case Type::ReverseMask: {
                return RG_LIKELY(u.mask.offset < values.len) &&
                       !(values[u.mask.offset] & u.mask.mask);
            } break;

            case Type::Cmd: {
                return values[0] == u.cmd;
            } break;

            case Type::CmdJump: {
                return values[0] == u.cmd_jump.cmd &&
                       values[1] == u.cmd_jump.jump;
            } break;
        }

        RG_UNREACHABLE();
    }
};

}
