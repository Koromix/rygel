#include "kutil.hh"
#include "data_fg.hh"

struct LoadTableData {
    size_t table_idx;
    const char *filename;
    ArrayRef<uint8_t> raw_data;
    bool loaded;
};

#define FAIL_PARSE_IF(Cond) \
    do { \
        if (Cond) { \
            LogError("Malformed binary table file '%1': %2", \
                     filename ? filename : "?", STRINGIFY(Cond)); \
            return false; \
        } \
    } while (false)

static inline void ReverseBytes(uint16_t *u)
{
    *u = (uint16_t)(((*u & 0x00FF) << 8) |
                    ((*u & 0xFF00) >> 8));
}

static inline void ReverseBytes(uint32_t *u)
{
    *u = ((*u & 0x000000FF) << 24) |
         ((*u & 0x0000FF00) << 8)  |
         ((*u & 0x00FF0000) >> 8)  |
         ((*u & 0xFF000000) >> 24);
}

static inline void ReverseBytes(uint64_t *u)
{
    *u = ((*u & 0x00000000000000FF) << 56) |
         ((*u & 0x000000000000FF00) << 40) |
         ((*u & 0x0000000000FF0000) << 24) |
         ((*u & 0x00000000FF000000) << 8)  |
         ((*u & 0x000000FF00000000) >> 8)  |
         ((*u & 0x0000FF0000000000) >> 24) |
         ((*u & 0x00FF000000000000) >> 40) |
         ((*u & 0xFF00000000000000) >> 56);
}

static Date ConvertDate1980(uint16_t days)
{
    static const int8_t days_per_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    Date date;

    if (UNLIKELY(!days)) {
        date.st.year = 1979;
        date.st.month = 12;
        date.st.day = 31;

        return date;
    }

    bool leap_year;
    for (date.st.year = 1980;; date.st.year++) {
        leap_year = ((date.st.year % 4 == 0 && date.st.year % 100 != 0) || date.st.year % 400 == 0);

        int year_days = 365 + leap_year;
        if (days <= year_days)
            break;
        days -= year_days;
    }
    for (date.st.month = 1; date.st.month <= 12; date.st.month++) {
        int month_days = days_per_month[date.st.month - 1] + (date.st.month == 2 && leap_year);
        if (days <= month_days)
            break;
        days -= month_days;
    }
    date.st.day = days;

    return date;
}

static DiagnosisCode ConvertDiagnosisCode(uint16_t code123, uint16_t code456)
{
    DiagnosisCode code = {};

    snprintf(code.str, sizeof(code.str), "%c%02d", code123 / 100 + 65, code123 % 100);

    static const char code456_chars[] = " 0123456789+";
    code456 %= 1584;
    code.str[3] = code456_chars[code456 / 132]; code456 %= 132;
    code.str[4] = code456_chars[code456 / 11]; code456 %= 11;
    code.str[5] = code456_chars[code456];
    for (size_t i = 5; i >= 3 && code.str[i] == ' '; i--) {
        code.str[i] = '\0';
    }

    return code;
}

// TODO: Be careful with overflow in offset and length checks
bool ParseTableHeaders(const ArrayRef<const uint8_t> file_data,
                       const char *filename, DynamicArray<TableInfo> *out_tables)
{
    DEFER_NC(out_tables_guard, len = out_tables->len) { out_tables->RemoveFrom(len); };

    // Since FG 10.10b, each tab file can contain several tables, with a different
    // date range for each. The struct layout changed a bit around FG 11.11, which is
    // the first version supported here.
#pragma pack(push, 1)
    struct PackedHeader1111 {
        char signature[8];
        char version[4];
        char date[6];
        char name[8];
        uint8_t pad1;
        uint8_t sections_count;
        uint8_t pad2[4];
	};
    struct PackedSection1111 {
        uint8_t pad1[18];
        uint16_t values_count;
        uint16_t value_len;
        uint32_t raw_len;
        uint32_t raw_offset;
        uint8_t pad2[3];
	};
    struct PackedTablePtr1111 {
        uint16_t date_range[2];
        uint8_t pad1[2]; // No idea what those two bytes are for
        uint32_t raw_offset;
	};
#pragma pack(pop)

    StaticAssert(sizeof(TableInfo::raw_type) > sizeof(PackedHeader1111::name));

    PackedHeader1111 raw_main_header;
    PackedSection1111 raw_main_section;
    {
        FAIL_PARSE_IF(file_data.len < sizeof(PackedHeader1111) + sizeof(PackedSection1111));

        memcpy(&raw_main_header, file_data.ptr, sizeof(PackedHeader1111));
        FAIL_PARSE_IF(raw_main_header.sections_count != 1);

        memcpy(&raw_main_section, file_data.ptr + sizeof(PackedHeader1111), sizeof(PackedSection1111));
#ifdef ARCH_LITTLE_ENDIAN
        ReverseBytes(&raw_main_section.values_count);
        ReverseBytes(&raw_main_section.value_len);
        ReverseBytes(&raw_main_section.raw_len);
        ReverseBytes(&raw_main_section.raw_offset);
#endif

        int version = 0, revision = 0;
        sscanf(raw_main_header.version, "%2u%2u", &version, &revision);
        FAIL_PARSE_IF(version < 11 || (version == 11 && revision < 10));
        FAIL_PARSE_IF(raw_main_section.value_len != sizeof(PackedTablePtr1111));
        FAIL_PARSE_IF(file_data.len < sizeof(PackedHeader1111) +
                                 raw_main_section.values_count * sizeof(PackedTablePtr1111));
    }

    for (int i = 0; i < raw_main_section.values_count; i++) {
        TableInfo table = {};

        PackedTablePtr1111 raw_table_ptr;
        {
            memcpy(&raw_table_ptr, file_data.ptr + sizeof(PackedHeader1111) +
                                   sizeof(PackedSection1111) + i * sizeof(PackedTablePtr1111),
                   sizeof(PackedTablePtr1111));
#ifdef ARCH_LITTLE_ENDIAN
            ReverseBytes(&raw_table_ptr.date_range[0]);
            ReverseBytes(&raw_table_ptr.date_range[1]);
            ReverseBytes(&raw_table_ptr.raw_offset);
#endif
            FAIL_PARSE_IF(file_data.len < raw_table_ptr.raw_offset + sizeof(PackedHeader1111));
        }

        PackedHeader1111 raw_table_header;
        PackedSection1111 raw_table_sections[CountOf(table.sections.data)];
        {
            memcpy(&raw_table_header, file_data.ptr + raw_table_ptr.raw_offset,
                   sizeof(PackedHeader1111));
            FAIL_PARSE_IF(file_data.len < raw_table_ptr.raw_offset +
                                     raw_table_header.sections_count * sizeof(PackedSection1111));
            FAIL_PARSE_IF(raw_table_header.sections_count > CountOf(raw_table_sections));

            for (int j = 0; j < raw_table_header.sections_count; j++) {
                memcpy(&raw_table_sections[j], file_data.ptr + raw_table_ptr.raw_offset +
                                               sizeof(PackedHeader1111) +
                                               j * sizeof(PackedSection1111),
                       sizeof(PackedSection1111));
#ifdef ARCH_LITTLE_ENDIAN
                ReverseBytes(&raw_table_sections[j].values_count);
                ReverseBytes(&raw_table_sections[j].value_len);
                ReverseBytes(&raw_table_sections[j].raw_len);
                ReverseBytes(&raw_table_sections[j].raw_offset);
#endif
                FAIL_PARSE_IF(file_data.len < raw_table_ptr.raw_offset +
                                         raw_table_sections[j].raw_offset +
                                         raw_table_sections[j].raw_len);
            }
        }

        // Parse header information
        sscanf(raw_main_header.date, "%2" SCNd8 "%2" SCNd8 "%4" SCNd16,
               &table.build_date.st.day, &table.build_date.st.month,
               &table.build_date.st.year);
        table.build_date.st.year += 2000;
        FAIL_PARSE_IF(!table.build_date.IsValid());
        sscanf(raw_table_header.version, "%2" SCNd16 "%2" SCNd16,
               &table.version[0], &table.version[1]);
        table.limit_dates[0] = ConvertDate1980(raw_table_ptr.date_range[0]);
        table.limit_dates[1] = ConvertDate1980(raw_table_ptr.date_range[1]);
        FAIL_PARSE_IF(table.limit_dates[1] <= table.limit_dates[0]);

        // Table type
        strncpy(table.raw_type, raw_table_header.name, sizeof(raw_table_header.name));
        table.raw_type[sizeof(table.raw_type) - 1] = '\0';
        table.raw_type[strcspn(table.raw_type, " ")] = '\0';
        if (!strcmp(table.raw_type, "ARBREDEC")) {
            table.type = TableType::GhmDecisionTree;
        } else if (!strcmp(table.raw_type, "DIAG10CR")) {
            table.type = TableType::DiagnosisTable;
        } else if (!strcmp(table.raw_type, "CCAMCARA")) {
            table.type = TableType::ProcedureTable;
        } else if (!strcmp(table.raw_type, "RGHMINFO")) {
            table.type = TableType::GhmRootTable;
        } else if (!strcmp(table.raw_type, "GHSINFO")) {
            table.type = TableType::GhsDecisionTree;
        } else if (!strcmp(table.raw_type, "TABCOMBI")) {
            table.type = TableType::SeverityTable;
        } else if (!strcmp(table.raw_type, "AUTOREFS")) {
            table.type = TableType::AuthorizationTable;
        } else if (!strcmp(table.raw_type, "SRCDGACT")) {
            table.type = TableType::SupplementPairTable;
        } else {
            table.type = TableType::UnknownTable;
        }

        // Parse table sections
        table.sections.len = raw_table_header.sections_count;
        for (int j = 0; j < raw_table_header.sections_count; j++) {
            FAIL_PARSE_IF(raw_table_sections[j].raw_len !=
                            (uint32_t)raw_table_sections[j].values_count *
                            raw_table_sections[j].value_len);
            table.sections[j].raw_offset = raw_table_ptr.raw_offset +
                                           raw_table_sections[j].raw_offset;
            table.sections[j].raw_len = raw_table_sections[j].raw_len;
            table.sections[j].values_count = raw_table_sections[j].values_count;
            table.sections[j].value_len = raw_table_sections[j].value_len;
        }

        out_tables->Append(table);
    }

    out_tables_guard.disable();
    return true;
}

bool ParseGhmDecisionTree(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, DynamicArray<GhmDecisionNode> *out_nodes)
{
    DEFER_NC(out_nodes_guard, len = out_nodes->len) { out_nodes->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedTreeNode {
        uint8_t function;
        uint8_t params[2];
        uint8_t children_count;
        uint16_t children_idx;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.sections.len != 1);
    FAIL_PARSE_IF(table.sections[0].value_len != sizeof(PackedTreeNode));

    for (size_t i = 0; i < table.sections[0].values_count; i++) {
        GhmDecisionNode ghm_node = {};

        PackedTreeNode raw_node;
        memcpy(&raw_node, file_data + table.sections[0].raw_offset +
                          i * sizeof(PackedTreeNode), sizeof(PackedTreeNode));
#ifdef ARCH_LITTLE_ENDIAN
        ReverseBytes(&raw_node.children_idx);
#endif

        if (raw_node.function != 12) {
            ghm_node.type = GhmDecisionNode::Type::Test;
            ghm_node.u.test.function = raw_node.function;
            ghm_node.u.test.params[0] = raw_node.params[0];
            ghm_node.u.test.params[1] = raw_node.params[1];
            if (raw_node.function == 20) {
                ghm_node.u.test.children_idx = raw_node.children_idx + (raw_node.params[0] << 8) +
                                                                       raw_node.params[1];
                ghm_node.u.test.children_count = 1;
            } else {
                ghm_node.u.test.children_idx = raw_node.children_idx;
                ghm_node.u.test.children_count = raw_node.children_count;
            }
            // TODO: Test does not deal with overflow
            if (ghm_node.u.test.children_idx + ghm_node.u.test.children_count > table.sections[0].values_count) {
                return false;
            }
        } else {
            static char chars1[] = {0, 'C', 'H', 'K', 'M', 'Z', ' ', ' ', ' ', ' '};
            static char chars4[] = {0, 'A', 'B', 'C', 'D', 'E', 'J', 'Z', ' ', ' '};

            ghm_node.type = GhmDecisionNode::Type::Ghm;
            snprintf(ghm_node.u.ghm.code.str, sizeof(ghm_node.u.ghm.code.str), "%02u%c%02u%c",
                     raw_node.params[1], chars1[(raw_node.children_idx / 1000) % 10],
                     (raw_node.children_idx / 10) % 100, chars4[raw_node.children_idx % 10]);
            if (strchr(ghm_node.u.ghm.code.str, ' ')) {
                return false;
            }
            ghm_node.u.ghm.error = raw_node.params[0];
        }

        out_nodes->Append(ghm_node);
    }

    out_nodes_guard.disable();
    return true;
}

bool ParseDiagnosisTable(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, DynamicArray<DiagnosisInfo> *out_diags)
{
    DEFER_NC(out_diags_guard, len = out_diags->len) { out_diags->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedDiagnosisPtr  {
        uint16_t code456;

        uint16_t section2_idx;
        uint8_t section3_idx;
        uint16_t section4_bit;
        uint16_t section4_idx;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.sections.len != 5);
    FAIL_PARSE_IF(table.sections[0].values_count != 26 * 100 || table.sections[0].value_len != 2);
    FAIL_PARSE_IF(table.sections[1].value_len != sizeof(PackedDiagnosisPtr));
    FAIL_PARSE_IF(!table.sections[2].value_len || table.sections[2].value_len % 2 ||
                  table.sections[2].value_len / 2 > sizeof(DiagnosisInfo::sex[0].values));
    FAIL_PARSE_IF(!table.sections[3].value_len ||
                  table.sections[3].value_len > sizeof(DiagnosisInfo::warnings) * 8);
    FAIL_PARSE_IF(!table.sections[4].value_len);

    size_t block_start = table.sections[1].raw_offset;
    for (size_t root_idx = 0; root_idx < table.sections[0].values_count; root_idx++) {
        size_t block_end;
        {
            uint16_t end_idx = *(uint16_t *)(file_data + table.sections[0].raw_offset +
                                             root_idx * 2);
            ReverseBytes(&end_idx);
            FAIL_PARSE_IF(end_idx > table.sections[1].values_count);
            block_end = table.sections[1].raw_offset + end_idx * sizeof(PackedDiagnosisPtr);
        }

        for (size_t block_offset = block_start; block_offset < block_end;
             block_offset += sizeof(PackedDiagnosisPtr)) {
            DiagnosisInfo diag = {};

            PackedDiagnosisPtr raw_diag_ptr;
            {
                memcpy(&raw_diag_ptr, file_data + block_offset, sizeof(PackedDiagnosisPtr));
#ifdef ARCH_LITTLE_ENDIAN
                ReverseBytes(&raw_diag_ptr.code456);
                ReverseBytes(&raw_diag_ptr.section2_idx);
                ReverseBytes(&raw_diag_ptr.section4_bit);
                ReverseBytes(&raw_diag_ptr.section4_idx);
#endif

                FAIL_PARSE_IF(raw_diag_ptr.section2_idx >= table.sections[2].values_count);
                FAIL_PARSE_IF(raw_diag_ptr.section3_idx >= table.sections[3].values_count);
                FAIL_PARSE_IF(raw_diag_ptr.section4_idx >= table.sections[4].values_count);
            }

            diag.code = ConvertDiagnosisCode(root_idx, raw_diag_ptr.code456);

            // Flags and warnings
            {
                const uint8_t *sex_data = file_data + table.sections[2].raw_offset +
                                          raw_diag_ptr.section2_idx * table.sections[2].value_len;
                memcpy(diag.sex[0].values, sex_data, table.sections[2].value_len / 2);
                memcpy(diag.sex[1].values, sex_data + table.sections[2].value_len / 2,
                       table.sections[2].value_len / 2);
                if (memcmp(diag.sex[0].values, diag.sex[1].values, sizeof(diag.sex[0].values))) {
                    diag.flags |= (int)DiagnosisInfo::Flag::SexDifference;
                }

                const uint8_t *warn_data = file_data + table.sections[3].raw_offset +
                                           raw_diag_ptr.section3_idx * table.sections[3].value_len;
                for (size_t i = 0; i < table.sections[3].value_len; i++) {
                    if (warn_data[i]) {
                        diag.warnings |= 1 << i;
                    }
                }

                diag.exclusion_set_idx = raw_diag_ptr.section4_idx;
                diag.exclusion_set_bit = raw_diag_ptr.section4_bit;
            }

            out_diags->Append(diag);
        }

        block_start = block_end;
    }

    out_diags_guard.disable();
    return true;
}

bool ParseProcedureTable(const uint8_t *file_data, const char *filename,
                         const TableInfo &table, DynamicArray<ProcedureInfo> *out_procs)
{
    DEFER_NC(out_proc_guard, len = out_procs->len) { out_procs->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedProcedurePtr  {
        char char4;
        uint16_t seq_phase;

        uint16_t section2_idx;
        uint16_t date_min;
        uint16_t date_max;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.sections.len != 3);
    FAIL_PARSE_IF(table.sections[0].values_count != 26 * 26 * 26 ||
                  table.sections[0].value_len != 2);
    FAIL_PARSE_IF(table.sections[1].value_len != sizeof(PackedProcedurePtr));
    FAIL_PARSE_IF(!table.sections[2].value_len ||
                  table.sections[2].value_len > sizeof(ProcedureInfo::values));

    size_t block_start = table.sections[1].raw_offset;
    for (size_t root_idx = 0; root_idx < table.sections[0].values_count; root_idx++) {
        size_t block_end;
        {
            uint16_t end_idx = *(uint16_t *)(file_data + table.sections[0].raw_offset +
                                             root_idx * 2);
            ReverseBytes(&end_idx);
            FAIL_PARSE_IF(end_idx > table.sections[1].values_count);
            block_end = table.sections[1].raw_offset + end_idx * sizeof(PackedProcedurePtr);
        }

        char code123[3];
        {
            size_t root_idx_remain = root_idx;
            for (int i = 0; i < 3; i++) {
                code123[2 - i] = (root_idx_remain % 26) + 65;
                root_idx_remain /= 26;
            }
        }

        for (size_t block_offset = block_start; block_offset < block_end;
             block_offset += sizeof(PackedProcedurePtr)) {
            ProcedureInfo proc = {};

            PackedProcedurePtr raw_proc_ptr;
            {
                memcpy(&raw_proc_ptr, file_data + block_offset, sizeof(PackedProcedurePtr));
#ifdef ARCH_LITTLE_ENDIAN
                ReverseBytes(&raw_proc_ptr.seq_phase);
                ReverseBytes(&raw_proc_ptr.section2_idx);
                ReverseBytes(&raw_proc_ptr.date_min);
                ReverseBytes(&raw_proc_ptr.date_max);
#endif

                FAIL_PARSE_IF(raw_proc_ptr.section2_idx >= table.sections[2].values_count);
            }

            // CCAM code and phase
            {
                memcpy(proc.code.str, code123, 3);
                snprintf(proc.code.str + 3, sizeof(proc.code.str) - 3, "%c%03u",
                         (raw_proc_ptr.char4 % 26) + 65, raw_proc_ptr.seq_phase / 10 % 1000);
                proc.phase = raw_proc_ptr.seq_phase % 10;
            }

            // CCAM information and lists
            {
                proc.limit_dates[0] = ConvertDate1980(raw_proc_ptr.date_min);
                if (raw_proc_ptr.date_max < UINT16_MAX) {
                    proc.limit_dates[1] = ConvertDate1980(raw_proc_ptr.date_max + 1);
                } else {
                    proc.limit_dates[1] = ConvertDate1980(UINT16_MAX);
                }

                const uint8_t *proc_data = file_data + table.sections[2].raw_offset +
                                           raw_proc_ptr.section2_idx * table.sections[2].value_len;
                memcpy(proc.values, proc_data, table.sections[2].value_len);
            }

            out_procs->Append(proc);
        }

        block_start = block_end;
    }

    out_proc_guard.disable();
    return true;
}

bool ParseGhmRootTable(const uint8_t *file_data, const char *filename,
                       const TableInfo &table, DynamicArray<GhmRootInfo> *out_ghm_roots)
{
    DEFER_NC(out_ghm_roots_guard, len = out_ghm_roots->len) { out_ghm_roots->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedGhmRoot {
        uint8_t cmd;
        uint16_t type_seq;
        uint8_t young_severity_mode;
        uint8_t old_severity_mode;
        uint8_t duration_severity_mode;
        uint8_t pad1[2];
        uint8_t cma_exclusion_offset;
        uint8_t cma_exclusion_mask;
        uint8_t confirm_duration_treshold;
        uint8_t childbirth_severity_mode; // Appeared in FG 11d
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.sections.len != 1);
    if (table.version[0] > 11 || (table.version[0] == 11 && table.version[1] > 14)) {
        FAIL_PARSE_IF(table.sections[0].value_len != sizeof(PackedGhmRoot));
    } else {
        FAIL_PARSE_IF(table.sections[0].value_len != sizeof(PackedGhmRoot) - 1);
    }

    for (size_t i = 0; i < table.sections[0].values_count; i++) {
        GhmRootInfo ghm_root = {};

        PackedGhmRoot raw_ghm_root;
        memcpy(&raw_ghm_root, file_data + table.sections[0].raw_offset +
                              i * table.sections[0].value_len, table.sections[0].value_len);
#ifdef ARCH_LITTLE_ENDIAN
        ReverseBytes(&raw_ghm_root.type_seq);
#endif

        // GHM root code
        {
            static char chars1[] = {0, 'C', 'H', 'K', 'M', 'Z', ' ', ' ', ' ', ' '};

            // TODO: Improve Fmt API to replace sprintf/snprintf
            snprintf(ghm_root.code.str, sizeof(ghm_root.code.str), "%02u%c%02u",
                     raw_ghm_root.cmd, chars1[raw_ghm_root.type_seq / 100 % 10],
                     raw_ghm_root.type_seq % 100);
        }

        switch (raw_ghm_root.duration_severity_mode) {
            case 1: {
                ghm_root.allow_ambulatory = true;
            } break;
            case 2: {
                ghm_root.short_duration_treshold = 1;
            } break;
            case 3: {
                ghm_root.short_duration_treshold = 2;
            } break;
            case 4: {
                ghm_root.short_duration_treshold = 3;
            } break;
        }
        ghm_root.confirm_duration_treshold = raw_ghm_root.confirm_duration_treshold;

        if (raw_ghm_root.young_severity_mode == 1) {
            ghm_root.young_age_treshold = 2;
            ghm_root.young_severity_limit = 2;
        }
        switch (raw_ghm_root.old_severity_mode) {
            case 1: {
                ghm_root.old_age_treshold = 70;
                ghm_root.old_severity_limit = 2;
            } break;
            case 2: {
                ghm_root.old_age_treshold = 80;
                ghm_root.old_severity_limit = 2;
            } break;
            case 3: {
                ghm_root.old_age_treshold = 70;
                ghm_root.old_severity_limit = 3;
            } break;
            case 4: {
                ghm_root.old_age_treshold = 80;
                ghm_root.old_severity_limit = 3;
            } break;
            case 5: {
                ghm_root.old_age_treshold = 70;
                ghm_root.old_severity_limit = 4;
            } break;
            case 6: {
                ghm_root.old_age_treshold = 80;
                ghm_root.old_severity_limit = 4;
            } break;
        }

        if (table.sections[0].value_len >= 12 && raw_ghm_root.childbirth_severity_mode) {
            FAIL_PARSE_IF(raw_ghm_root.childbirth_severity_mode < 2 ||
                          raw_ghm_root.childbirth_severity_mode > 4);
            ghm_root.childbirth_severity_list = raw_ghm_root.childbirth_severity_mode - 1;
        }

        ghm_root.cma_exclusion_offset = raw_ghm_root.cma_exclusion_offset;
        ghm_root.cma_exclusion_mask = raw_ghm_root.cma_exclusion_mask;

        out_ghm_roots->Append(ghm_root);
    }

    out_ghm_roots_guard.disable();
    return true;
}

bool ParseSeverityTable(const uint8_t *file_data, const char *filename,
                        const TableInfo &table, size_t section_idx,
                        DynamicArray<ValueRangeCell<2>> *out_cells)
{
    DEFER_NC(out_cells_guard, len = out_cells->len) { out_cells->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedCell {
        uint16_t var1_min;
        uint16_t var1_max;
        uint16_t var2_min;
        uint16_t var2_max;
        uint16_t value;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(section_idx >= table.sections.len);
    FAIL_PARSE_IF(table.sections[section_idx].value_len != sizeof(PackedCell));

    for (size_t i = 0; i < table.sections[section_idx].values_count; i++) {
        ValueRangeCell<2> cell = {};

        PackedCell raw_cell;
        memcpy(&raw_cell, file_data + table.sections[section_idx].raw_offset +
                          i * sizeof(PackedCell), sizeof(PackedCell));
#ifdef ARCH_LITTLE_ENDIAN
        ReverseBytes(&raw_cell.var1_min);
        ReverseBytes(&raw_cell.var1_max);
        ReverseBytes(&raw_cell.var2_min);
        ReverseBytes(&raw_cell.var2_max);
        ReverseBytes(&raw_cell.value);
#endif

        cell.limits[0].min = raw_cell.var1_min;
        cell.limits[0].max = raw_cell.var1_max + 1;
        cell.limits[1].min = raw_cell.var2_min;
        cell.limits[1].max = raw_cell.var2_max + 1;
        cell.value = raw_cell.value;

        out_cells->Append(cell);
    }

    out_cells_guard.disable();
    return true;
}

#if 0
bool ParseBirthSeverityTable(const uint8_t *file_data, const char *filename,
                             const TableInfo &table, BirthSeverityTable *out_table)
{
    DEFER_NC(table_guard,
             gnn_len = out_table->gnn_table.len, cma0_len = out_table->cma_tables[0].len,
             cma1_len = out_table->cma_tables[1].len, cma2_len = out_table->cma_tables[2].len) {
        out_table->gnn_table.RemoveFrom(gnn_len);
        out_table->cma_tables[0].RemoveFrom(cma0_len);
        out_table->cma_tables[1].RemoveFrom(cma1_len);
        out_table->cma_tables[2].RemoveFrom(cma2_len);
    };

    FAIL_PARSE_IF(table.sections.len != 4);

    if (!ParseValueRangeTable(file_data, filename, table.sections[0], &out_table->gnn_table))
        return false;
    if (!ParseValueRangeTable(file_data, filename, table.sections[1], &out_table->cma_tables[0]))
        return false;
    if (!ParseValueRangeTable(file_data, filename, table.sections[2], &out_table->cma_tables[1]))
        return false;
    if (!ParseValueRangeTable(file_data, filename, table.sections[3], &out_table->cma_tables[2]))
        return false;

    table_guard.disable();
    return true;
}
#endif

bool ParseGhsDecisionTree(const uint8_t *file_data, const char *filename,
                           const TableInfo &table, DynamicArray<GhsDecisionNode> *out_nodes)
{
    size_t base_len = out_nodes->len;
    DEFER_N(out_nodes_guard) { out_nodes->RemoveFrom(base_len); };

#pragma pack(push, 1)
    struct PackedGhsNode {
        uint8_t cmd;
        uint16_t type_seq;
        uint8_t low_duration_mode;
        uint8_t function;
        uint8_t params[2];
        uint8_t skip_after_failure;
        uint8_t valid_ghs;
        struct { // 0 for public, 1 for private
            uint16_t ghs_code;
            uint16_t high_duration_treshold;
            uint16_t low_duration_treshold;
        } versions[2];
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.sections.len != 1);
    FAIL_PARSE_IF(table.sections[0].value_len != sizeof(PackedGhsNode));

    uint32_t previous_cmd_type_seq = 0;
    size_t ghm_node_idx = SIZE_MAX;
    size_t first_test_idx = SIZE_MAX;
    for (size_t i = 0; i < table.sections[0].values_count; i++) {
        PackedGhsNode raw_ghs_node;
        memcpy(&raw_ghs_node, file_data + table.sections[0].raw_offset +
                         i * sizeof(PackedGhsNode), sizeof(PackedGhsNode));
#ifdef ARCH_LITTLE_ENDIAN
        ReverseBytes(&raw_ghs_node.type_seq);
        for (size_t j = 0; j < 2; j++) {
            ReverseBytes(&raw_ghs_node.versions[j].ghs_code);
            ReverseBytes(&raw_ghs_node.versions[j].high_duration_treshold);
            ReverseBytes(&raw_ghs_node.versions[j].low_duration_treshold);
        }
#endif

        uint32_t cmd_type_seq = (raw_ghs_node.cmd << 16) | raw_ghs_node.type_seq;
        if (cmd_type_seq != previous_cmd_type_seq) {
            previous_cmd_type_seq = cmd_type_seq;

            FAIL_PARSE_IF(first_test_idx != SIZE_MAX);
            if (ghm_node_idx != SIZE_MAX) {
                (*out_nodes)[ghm_node_idx].u.ghm.next_ghm_idx = out_nodes->len - base_len;
            } else {
                FAIL_PARSE_IF(i);
            }
            ghm_node_idx = out_nodes->len;

            GhsDecisionNode ghm_node = {};
            ghm_node.type = GhsDecisionNode::Type::Ghm;
            {
                static char chars1[] = {0, 'C', 'H', 'K', 'M', 'Z'};
                static char chars4[] = {0, 'A', 'B', 'C', 'D', 'E', 'J',
                                        'Z', 'T', '1', '2', '3', '4'};
                snprintf(ghm_node.u.ghm.code.str, sizeof(ghm_node.u.ghm.code.str), "%02u%c%02u%c",
                         raw_ghs_node.cmd, chars1[raw_ghs_node.type_seq / 10000 % 6],
                         raw_ghs_node.type_seq / 100 % 100,
                         chars4[raw_ghs_node.type_seq % 100 % 13]);
            }
            out_nodes->Append(ghm_node);
        }

        if (raw_ghs_node.function) {
            if (first_test_idx == SIZE_MAX) {
                first_test_idx = out_nodes->len;
            }

            GhsDecisionNode test_node = {};
            test_node.type = GhsDecisionNode::Type::Test;
            test_node.u.test.function = raw_ghs_node.function;
            test_node.u.test.params[0] = raw_ghs_node.params[0];
            test_node.u.test.params[1] = raw_ghs_node.params[1];
            out_nodes->Append(test_node);
        } else {
            FAIL_PARSE_IF(!raw_ghs_node.valid_ghs);
        }

        if (raw_ghs_node.valid_ghs) {
            // TODO: Doubts about correctness of this
            // TODO: Put to 0 at the end
            for (size_t j = first_test_idx; j < out_nodes->len; j++) {
                (*out_nodes)[j].u.test.fail_goto_idx = out_nodes->len + 1 - base_len;
            }
            first_test_idx = SIZE_MAX;

            GhsDecisionNode ghs_node = {};
            ghs_node.type = GhsDecisionNode::Type::Ghs;
            for (int j = 0; j < 2; j++) {
                ghs_node.u.ghs[j].code.value = raw_ghs_node.versions[j].ghs_code;
                ghs_node.u.ghs[j].high_duration_treshold =
                    raw_ghs_node.versions[j].high_duration_treshold;
                ghs_node.u.ghs[j].low_duration_treshold =
                    raw_ghs_node.versions[j].low_duration_treshold;
            }
            out_nodes->Append(ghs_node);
        }
    }
    FAIL_PARSE_IF(first_test_idx != SIZE_MAX);
    FAIL_PARSE_IF(ghm_node_idx + 1 == out_nodes->len);

    out_nodes_guard.disable();
    return true;
}

bool ParseAuthorizationTable(const uint8_t *file_data, const char *filename,
                             const TableInfo &table, DynamicArray<AuthorizationInfo> *out_auths)
{
    DEFER_NC(out_auths_guard, len = out_auths->len) { out_auths->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedAuthorization {
        uint8_t code;
        uint8_t function;
        uint8_t global;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.sections.len != 2);
    FAIL_PARSE_IF(table.sections[0].value_len != 3 || table.sections[0].value_len != 3);

    for (int i = 0; i < 2; i++) {
        for (size_t j = 0; j < table.sections[i].values_count; j++) {
            AuthorizationInfo auth = {};

            PackedAuthorization raw_auth;
            memcpy(&raw_auth, file_data + table.sections[i].raw_offset +
                                          j * sizeof(PackedAuthorization),
                   sizeof(PackedAuthorization));

            if (i == 0) {
                auth.type = AuthorizationType::Bed;
            } else if (!raw_auth.global) {
                auth.type = AuthorizationType::Unit;
            } else {
                auth.type = AuthorizationType::Facility;
            }
            auth.code = raw_auth.code;
            auth.function = raw_auth.function;

            out_auths->Append(auth);
        }
    }

    out_auths_guard.disable();
    return true;
}

bool ParseSupplementPairTable(const uint8_t *file_data, const char *filename,
                              const TableInfo &table, size_t section_idx,
                              DynamicArray<DiagnosisProcedurePair> *out_pairs)
{
    DEFER_NC(out_pairs_guard, len = out_pairs->len) { out_pairs->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedPair {
        uint16_t diag_code123;
        uint16_t diag_code456;
        uint16_t proc_code123;
        uint16_t proc_code456;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(section_idx >= table.sections.len);
    FAIL_PARSE_IF(table.sections[section_idx].value_len != sizeof(PackedPair));

    for (size_t i = 0; i < table.sections[section_idx].values_count; i++) {
        DiagnosisProcedurePair pair = {};

        PackedPair raw_pair;
        memcpy(&raw_pair, file_data + table.sections[section_idx].raw_offset +
                          i * sizeof(PackedPair), sizeof(PackedPair));
#ifdef ARCH_LITTLE_ENDIAN
        ReverseBytes(&raw_pair.diag_code123);
        ReverseBytes(&raw_pair.diag_code456);
        ReverseBytes(&raw_pair.proc_code123);
        ReverseBytes(&raw_pair.proc_code456);
#endif

        pair.diag_code = ConvertDiagnosisCode(raw_pair.diag_code123, raw_pair.diag_code456);
        {
            uint16_t code123_remain = raw_pair.proc_code123;
            for (int i = 0; i < 3; i++) {
                pair.proc_code.str[2 - i] = (code123_remain % 26) + 65;
                code123_remain /= 26;
            }
            snprintf(pair.proc_code.str + 3, sizeof(pair.proc_code.str) - 3, "%c%03u",
                     (raw_pair.proc_code456 / 1000 % 26) + 65,
                     raw_pair.proc_code456 % 1000);
        }

        out_pairs->Append(pair);
    }

    out_pairs_guard.disable();
    return true;
}

const ClassifierSet *ClassifierStore::FindSet(Date date) const
{
    if (date.value) {
        for (size_t i = sets.len - 1; i-- > 0;) {
            if (date >= sets[i].limit_dates[0] && date < sets[i].limit_dates[1])
                return &sets[i];
        }
    } else if (sets.len) {
        return &sets[sets.len - 1];
    }
    return nullptr;
}

static bool CommitClassifierSet(ClassifierStore *store, Date start_date, Date end_sate,
                                LoadTableData *current_tables[])
{
    bool success = true;

    ClassifierSet set = {};

    set.limit_dates[0] = start_date;
    set.limit_dates[1] = end_sate;

#define LOAD_TABLE(MemberName, LoadFunc, ...) \
        do { \
            if (!table->loaded) { \
                set.MemberName.offset = store->MemberName.len; \
                success &= LoadFunc(table->raw_data.ptr, table->filename, \
                                    table_info, ##__VA_ARGS__, &store->MemberName); \
                set.MemberName.len = store->MemberName.len - set.MemberName.offset; \
            } else { \
                set.MemberName = store->sets[store->sets.len - 1].MemberName; \
            } \
        } while (false)

    size_t active_count = 0;
    for (size_t i = 0; i < CountOf(set.tables); i++) {
        if (!current_tables[i])
            continue;

        LoadTableData *table = current_tables[i];
        const TableInfo &table_info = store->tables[table->table_idx];

        switch ((TableType)i) {
            case TableType::GhmDecisionTree: {
                LOAD_TABLE(ghm_nodes, ParseGhmDecisionTree);
            } break;
            case TableType::DiagnosisTable: {
                LOAD_TABLE(diagnoses, ParseDiagnosisTable);
            } break;
            case TableType::ProcedureTable: {
                LOAD_TABLE(procedures, ParseProcedureTable);
            } break;
            case TableType::GhmRootTable: {
                LOAD_TABLE(ghm_roots, ParseGhmRootTable);
            } break;
            case TableType::SeverityTable: {
                LOAD_TABLE(gnn_cells, ParseSeverityTable, 0);
                LOAD_TABLE(cma_cells[0], ParseSeverityTable, 1);
                LOAD_TABLE(cma_cells[1], ParseSeverityTable, 2);
                LOAD_TABLE(cma_cells[2], ParseSeverityTable, 3);
            } break;

            case TableType::GhsDecisionTree: {
                LOAD_TABLE(ghs_nodes, ParseGhsDecisionTree);
            } break;
            case TableType::AuthorizationTable: {
                LOAD_TABLE(authorizations, ParseAuthorizationTable);
            } break;
            case TableType::SupplementPairTable: {
                LOAD_TABLE(supplement_pairs[0], ParseSupplementPairTable, 0);
                LOAD_TABLE(supplement_pairs[1], ParseSupplementPairTable, 1);
            } break;

            case TableType::UnknownTable:
                break;
        }
        table->loaded = true;
        set.tables[i] = &table_info;

        active_count++;
    }

    if (active_count) {
        store->sets.Append(set);
    }

#undef LOAD_TABLE

    return success;
}

bool LoadClassifierStore(ArrayRef<const char *const> filenames, ClassifierStore *out_store)
{
    Assert(!out_store->tables.len);
    Assert(!out_store->sets.len);

    bool success = true;

    Allocator file_alloc;

    DynamicArray<LoadTableData> tables;
    for (const char *filename: filenames) {
        ArrayRef<uint8_t> raw_data;
        if (!ReadFile(&file_alloc, filename, Megabytes(8), &raw_data)) {
            success = false;
            continue;
        }

        size_t start_len = out_store->tables.len;
        if (!ParseTableHeaders(raw_data, filename, &out_store->tables)) {
            success = false;
            continue;
        }
        for (size_t i = start_len; i < out_store->tables.len; i++) {
            if (out_store->tables[i].type == TableType::UnknownTable)
                continue;

            LoadTableData table = {};
            table.table_idx = i;
            table.filename = filename;
            table.raw_data = raw_data;
            tables.Append(table);
        }
    }

    std::sort(tables.begin(), tables.end(),
              [&](const LoadTableData &table1, const LoadTableData &table2) {
        const TableInfo &table_info1 = out_store->tables[table1.table_idx];
        const TableInfo &table_info2 = out_store->tables[table2.table_idx];

        if (table_info1.limit_dates[0] < table_info2.limit_dates[0]) {
            return true;
        } else if (table_info1.limit_dates[0] == table_info2.limit_dates[0]) {
            return table_info1.build_date < table_info2.build_date;
        } else {
            return false;
        }
    });

    LoadTableData *active_tables[CountOf(TableTypeNames)] = {};
    Date start_date = {}, end_date = {};
    for (LoadTableData &table: tables) {
        const TableInfo &table_info = out_store->tables[table.table_idx];

        while (end_date.value && table_info.limit_dates[0] >= end_date) {
            success &= CommitClassifierSet(out_store, start_date, end_date, active_tables);

            start_date = {};
            Date next_end_date = {};
            for (size_t i = 0; i < CountOf(active_tables); i++) {
                if (!active_tables[i])
                    continue;

                const TableInfo &active_info = out_store->tables[active_tables[i]->table_idx];

                if (active_info.limit_dates[1] == end_date) {
                    active_tables[i] = nullptr;
                } else {
                    if (!next_end_date.value || active_info.limit_dates[1] < next_end_date) {
                        next_end_date = active_info.limit_dates[1];
                    }
                }
            }

            start_date = table_info.limit_dates[0];
            end_date = next_end_date;
        }

        if (start_date.value) {
            if (table_info.limit_dates[0] > start_date) {
                success &= CommitClassifierSet(out_store, start_date, table_info.limit_dates[0],
                                               active_tables);
                start_date = table_info.limit_dates[0];
            }
        } else {
            start_date = table_info.limit_dates[0];
        }
        if (!end_date.value || table_info.limit_dates[1] < end_date) {
            end_date = table_info.limit_dates[1];
        }

        active_tables[(int)table_info.type] = &table;
    }
    success &= CommitClassifierSet(out_store, start_date, end_date, active_tables);

    return success;
}
