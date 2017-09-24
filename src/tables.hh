#pragma once

#include "kutil.hh"

// TODO: Reorganize basic domain types -- move to common file?
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
            uint8_t function; // Switch to dedicated enum
            uint8_t params[2];
            size_t children_count;
            size_t children_idx;
        } test;

        struct {
            GhmCode code;
            int16_t error;
        } ghm;
    } u;
};

struct DiagnosisInfo {
    enum class Flag {
        SexDifference = 1
    };

    DiagnosisCode code;

    uint16_t flags;
    struct Attributes {
        uint8_t raw[37];

        uint8_t cmd;
        uint8_t jump;
        uint8_t severity;
    } attributes[2];
    uint16_t warnings;

    uint16_t exclusion_set_idx;
    uint8_t cma_exclusion_offset;
    uint8_t cma_exclusion_mask;

    const Attributes &Attributes(Sex sex) const
    {
        StaticAssert((int)Sex::Male == 1);
        return attributes[(int)sex - 1];
    }

    HASH_SET_HANDLER(DiagnosisInfo, code);
};

struct ExclusionInfo {
    uint8_t raw[256];
};

struct ProcedureInfo {
    ProcedureCode code;
    int8_t phase;

    Date limit_dates[2];
    uint8_t bytes[55];

    HASH_SET_HANDLER(ProcedureInfo, code);
};

template <size_t N>
struct ValueRangeCell {
    struct {
        int min;
        int max;
    } limits[N];
    int value;

    bool Test(size_t idx, int value) const
    {
        DebugAssert(idx < N);
        return (value >= limits[idx].min && value < limits[idx].max);
    }
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

    uint8_t cma_exclusion_offset;
    uint8_t cma_exclusion_mask;

    HASH_SET_HANDLER(GhmRootInfo, code);
};

struct GhsInfo {
    GhmCode ghm;

    struct {
        GhsCode ghs;
        int16_t low_duration_treshold;
        int16_t high_duration_treshold;
    } sectors[2]; // 0 for public, 1 for private

    int8_t bed_authorization;
    int8_t unit_authorization;
    int8_t minimal_duration;

    int8_t minimal_age;

    uint8_t main_diagnosis_mask;
    uint8_t main_diagnosis_offset;
    uint8_t diagnosis_mask;
    uint8_t diagnosis_offset;

    uint8_t proc_mask;
    uint8_t proc_offset;

    HASH_SET_HANDLER_N(GhmHandler, GhsInfo, ghm);
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
    DiagnosisCode diag_code;
    ProcedureCode proc_code;
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
                        const TableInfo &table, size_t section_idx,
                        HeapArray<ValueRangeCell<2>> *out_cells);

bool ParseGhsTable(const uint8_t *file_data, const char *filename,
                   const TableInfo &table, HeapArray<GhsInfo> *out_nodes);
bool ParseAuthorizationTable(const uint8_t *file_data, const char *filename,
                             const TableInfo &table, HeapArray<AuthorizationInfo> *out_auths);
bool ParseSrcPairTable(const uint8_t *file_data, const char *filename,
                       const TableInfo &table, size_t section_idx,
                       HeapArray<SrcPair> *out_pairs);

struct TableIndex {
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

    ArrayRef<GhsInfo> ghs;
    ArrayRef<AuthorizationInfo> authorizations;
    ArrayRef<SrcPair> src_pairs[2];

    HashSet<DiagnosisCode, const DiagnosisInfo *> *diagnoses_map;
    HashSet<ProcedureCode, const ProcedureInfo *> *procedures_map;
    HashSet<GhmRootCode, const GhmRootInfo *> *ghm_roots_map;

    HashSet<GhmCode, const GhsInfo *, GhsInfo::GhmHandler> *ghm_to_ghs_map;

    const DiagnosisInfo *FindDiagnosis(DiagnosisCode code) const;
    ArrayRef<const ProcedureInfo> FindProcedure(ProcedureCode code) const;
    const ProcedureInfo *FindProcedure(ProcedureCode code, int8_t phase, Date date) const;
    const GhmRootInfo *FindGhmRoot(GhmRootCode code) const;
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
    } maps;

    const TableIndex *FindIndex(Date date) const;
    TableIndex *FindIndex(Date date)
        { return (TableIndex *)((const TableSet *)this)->FindIndex(date); }
};

bool LoadTableSet(ArrayRef<const char *const> filenames, TableSet *out_set);
