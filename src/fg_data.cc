#include "kutil.hh"
#include "fg_data.hh"

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

static DiagnosisCode ConvertDiagnosticCode(uint16_t code123, uint16_t code456)
{
    DiagnosisCode code = {};

    snprintf(code.str, sizeof(code.str), "%c%02d", code123 / 100 + 65, code456 % 100);

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
bool ParseTableHeaders(const uint8_t *file_data, size_t file_len,
                       const char *filename, DynamicArray<TableInfo> *out_tables)
{
    DEFER_NC(out_tables_guard, len = out_tables->len) { out_tables->RemoveFrom(len); };

    // Since FG 10.10b, each tab file can contain several tables, with a different
    // date range for each. The struct layout changed a bit around FG 11.11, which is
    // the first version supported here.
    struct PackedHeader1111 {
        char signature[8];
        char version[4];
        char date[6];
        char name[8];
        uint8_t pad1;
        uint8_t sections_count;
        uint8_t pad2[4];
    } __attribute__((__packed__));
    struct PackedSection1111 {
        uint8_t pad1[18];
        uint16_t values_count;
        uint16_t value_len;
        uint32_t raw_len;
        uint32_t raw_offset;
        uint8_t pad2[3];
    } __attribute__((__packed__));
    struct PackedTablePtr1111 {
        uint16_t date_range[2];
        uint8_t pad1[2]; // No idea what those two bytes are for
        uint32_t raw_offset;
    } __attribute__((__packed__));

    // FIXME: What about other filename uses in other functions?
    if (!filename) {
        filename = "?";
    }

#define FAIL_PARSE_IF(Cond) \
        do { \
            if (Cond) { \
                LogError("Malformed binary table file '%1': %2", filename, STRINGIFY(Cond)); \
                return false; \
            } \
        } while (false)

    PackedHeader1111 main_header;
    PackedSection1111 main_section;
    {
        FAIL_PARSE_IF(file_len < sizeof(PackedHeader1111) + sizeof(PackedSection1111));

        memcpy(&main_header, file_data, sizeof(PackedHeader1111));
        FAIL_PARSE_IF(main_header.sections_count != 1);

        memcpy(&main_section, file_data + sizeof(PackedHeader1111), sizeof(PackedSection1111));
#ifdef ARCH_LITTLE_ENDIAN
        ReverseBytes(&main_section.values_count);
        ReverseBytes(&main_section.value_len);
        ReverseBytes(&main_section.raw_len);
        ReverseBytes(&main_section.raw_offset);
#endif

        int version = 0, revision = 0;
        sscanf(main_header.version, "%2u%2u", &version, &revision);
        FAIL_PARSE_IF(version < 11 || (version == 11 && revision < 10));
        FAIL_PARSE_IF(main_section.value_len != sizeof(PackedTablePtr1111));
        FAIL_PARSE_IF(file_len < sizeof(PackedHeader1111) +
                                 main_section.values_count * sizeof(PackedTablePtr1111));
    }

    for (int i = 0; i < main_section.values_count; i++) {
        TableInfo table = {};

        PackedTablePtr1111 raw_table_ptr;
        {
            memcpy(&raw_table_ptr, file_data + sizeof(PackedHeader1111) +
                                   sizeof(PackedSection1111) + i * sizeof(PackedTablePtr1111),
                   sizeof(PackedTablePtr1111));
#ifdef ARCH_LITTLE_ENDIAN
            ReverseBytes(&raw_table_ptr.date_range[0]);
            ReverseBytes(&raw_table_ptr.date_range[1]);
            ReverseBytes(&raw_table_ptr.raw_offset);
#endif
            FAIL_PARSE_IF(file_len < raw_table_ptr.raw_offset + sizeof(PackedHeader1111));
        }

        PackedHeader1111 raw_table_header;
        PackedSection1111 raw_table_sections[CountOf(table.sections.data)];
        {
            memcpy(&raw_table_header, file_data + raw_table_ptr.raw_offset,
                   sizeof(PackedHeader1111));
            FAIL_PARSE_IF(file_len < raw_table_ptr.raw_offset +
                                     raw_table_header.sections_count * sizeof(PackedSection1111));
            FAIL_PARSE_IF(raw_table_header.sections_count > CountOf(raw_table_sections));

            for (int j = 0; j < raw_table_header.sections_count; j++) {
                memcpy(&raw_table_sections[j], file_data + raw_table_ptr.raw_offset +
                                               sizeof(PackedHeader1111) + j * sizeof(PackedSection1111),
                       sizeof(PackedSection1111));
#ifdef ARCH_LITTLE_ENDIAN
                ReverseBytes(&raw_table_sections[j].values_count);
                ReverseBytes(&raw_table_sections[j].value_len);
                ReverseBytes(&raw_table_sections[j].raw_len);
                ReverseBytes(&raw_table_sections[j].raw_offset);
#endif
                FAIL_PARSE_IF(file_len < raw_table_ptr.raw_offset +
                                         raw_table_sections[j].raw_offset +
                                         raw_table_sections[j].raw_len);
            }
        }

        // Parse header information
        sscanf(main_header.date, "%2" SCNd8 "%2" SCNd8 "%4" SCNd16,
               &table.build_date.st.day, &table.build_date.st.month,
               &table.build_date.st.year);
        table.build_date.st.year += 2000;
        FAIL_PARSE_IF(!table.build_date.IsValid());
        sscanf(raw_table_header.version, "%2" SCNd16 "%2" SCNd16,
               &table.version[0], &table.version[1]);
        table.limit_dates[0] = ConvertDate1980(raw_table_ptr.date_range[0]);
        table.limit_dates[1] = ConvertDate1980(raw_table_ptr.date_range[1]);
        FAIL_PARSE_IF(table.limit_dates[1] < table.limit_dates[0]);
        if (!strncmp(raw_table_header.name, "ARBREDEC", sizeof(raw_table_header.name))) {
            table.type = TableType::GhmDecisionTree;
        } else if (!strncmp(raw_table_header.name, "DIAG10CR", sizeof(raw_table_header.name))) {
            table.type = TableType::DiagnosticInfo;
        } else if (!strncmp(raw_table_header.name, "CCAMCARA", sizeof(raw_table_header.name))) {
            table.type = TableType::ProcedureInfo;
        } else if (!strncmp(raw_table_header.name, "RGHMINFO", sizeof(raw_table_header.name))) {
            table.type = TableType::GhmRootInfo;
        } else if (!strncmp(raw_table_header.name, "GHSINFO ", sizeof(raw_table_header.name))) {
            table.type = TableType::GhsDecisionTree;
        } else if (!strncmp(raw_table_header.name, "TABCOMBI", sizeof(raw_table_header.name))) {
            table.type = TableType::ChildbirthInfo;
        } else {
            LogError("Unknown table type in '%1': '%2'", filename,
                     MakeStrRef(raw_table_header.name, sizeof(raw_table_header.name)));
            return false;
        }

        // Parse table sections
        table.sections.len = raw_table_header.sections_count;
        for (int j = 0; j < raw_table_header.sections_count; j++) {
            FAIL_PARSE_IF(raw_table_sections[j].raw_len != raw_table_sections[j].values_count *
                                                           raw_table_sections[j].value_len);
            table.sections[j].raw_offset = raw_table_ptr.raw_offset +
                                           raw_table_sections[j].raw_offset;
            table.sections[j].raw_len = raw_table_sections[j].raw_len;
            table.sections[j].values_count = raw_table_sections[j].values_count;
            table.sections[j].value_len = raw_table_sections[j].value_len;
        }

        out_tables->Append(table);
    }

#undef FAIL_PARSE_IF

    out_tables_guard.disable();
    return true;
}

bool ParseGhmDecisionTree(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, DynamicArray<GhmDecisionNode> *out_nodes)
{
    DEFER_NC(out_nodes_guard, len = out_nodes->len) { out_nodes->RemoveFrom(len); };

    struct PackedTreeNode {
        uint8_t function;
        uint8_t params[2];
        uint8_t children_count;
        uint16_t children_idx;
    } __attribute__((__packed__));

#define FAIL_PARSE_IF(Cond) \
        do { \
            if (Cond) { \
                LogError("Malformed binary table file '%1': %2", filename, STRINGIFY(Cond)); \
                return false; \
            } \
        } while (false)

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

#undef FAIL_PARSE_IF

    out_nodes_guard.disable();
    return true;
}

bool ParseDiagnosticTable(const uint8_t *file_data, const char *filename,
                          const TableInfo &table, DynamicArray<DiagnosticInfo> *out_diags)
{
    DEFER_NC(out_diags_guard, len = out_diags->len) { out_diags->RemoveFrom(len); };

    struct PackedDiagnosticPtr  {
        uint16_t code456;

        uint16_t section2_idx;
        uint8_t section3_idx;
        uint16_t section4_bit;
        uint16_t section4_idx;
    } __attribute__((__packed__));

#define FAIL_PARSE_IF(Cond) \
        do { \
            if (Cond) { \
                LogError("Malformed binary table file '%1': %2", filename, STRINGIFY(Cond)); \
                return false; \
            } \
        } while (false)

    FAIL_PARSE_IF(table.sections.len != 5);
    FAIL_PARSE_IF(table.sections[0].values_count != 26 * 100 || table.sections[0].value_len != 2);
    FAIL_PARSE_IF(table.sections[1].value_len != sizeof(PackedDiagnosticPtr));
    FAIL_PARSE_IF(!table.sections[2].value_len || table.sections[2].value_len % 2 ||
                  table.sections[2].value_len / 2 > sizeof(DiagnosticInfo::sex[0].values));
    FAIL_PARSE_IF(!table.sections[3].value_len ||
                  table.sections[3].value_len > sizeof(DiagnosticInfo::warnings) * 8);
    FAIL_PARSE_IF(!table.sections[4].value_len);

    size_t block_start = table.sections[1].raw_offset;
    for (size_t root_idx = 0; root_idx < table.sections[0].values_count; root_idx++) {
        size_t block_end;
        {
            uint16_t end_idx = *(uint16_t *)(file_data + table.sections[0].raw_offset +
                                             root_idx * 2);
            ReverseBytes(&end_idx);
            FAIL_PARSE_IF(end_idx > table.sections[1].values_count);
            block_end = table.sections[1].raw_offset + end_idx * sizeof(PackedDiagnosticPtr);
        }

        for (size_t block_offset = block_start; block_offset < block_end;
             block_offset += sizeof(PackedDiagnosticPtr)) {
            DiagnosticInfo diag = {};

            PackedDiagnosticPtr raw_diag_ptr;
            {
                memcpy(&raw_diag_ptr, file_data + block_offset, sizeof(PackedDiagnosticPtr));
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

            diag.code = ConvertDiagnosticCode(root_idx, raw_diag_ptr.code456);

            // Flags and warnings
            {
                const uint8_t *sex_data = file_data + table.sections[2].raw_offset +
                                          raw_diag_ptr.section2_idx * table.sections[2].value_len;
                memcpy(diag.sex[0].values, sex_data, table.sections[2].value_len / 2);
                memcpy(diag.sex[1].values, sex_data + table.sections[2].value_len / 2,
                       table.sections[2].value_len / 2);

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

#undef FAIL_PARSE_IF

    out_diags_guard.disable();
    return true;
}

bool ParseProcedureTable(const uint8_t *file_data, const char *filename,
                         const TableInfo &table, DynamicArray<ProcedureInfo> *out_procs)
{
    DEFER_NC(out_proc_guard, len = out_procs->len) { out_procs->RemoveFrom(len); };

    struct PackedProcedurePtr  {
        char char4;
        uint16_t seq_phase;

        uint16_t section2_idx;
        uint16_t date_min;
        uint16_t date_max;
    } __attribute__((__packed__));

#define FAIL_PARSE_IF(Cond) \
        do { \
            if (Cond) { \
                LogError("Malformed binary table file '%1': %2", filename, STRINGIFY(Cond)); \
                return false; \
            } \
        } while (false)

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

#undef FAIL_PARSE_IF

    out_proc_guard.disable();
    return true;
}

bool ParseGhmRootTable(const uint8_t *file_data, const char *filename,
                       const TableInfo &table, DynamicArray<GhmRootInfo> *out_ghm_roots)
{
    DEFER_NC(out_ghm_roots_guard, len = out_ghm_roots->len) { out_ghm_roots->RemoveFrom(len); };

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
        uint8_t childbirth_severity_mode;
    } __attribute__((__packed__));;

#define FAIL_PARSE_IF(Cond) \
        do { \
            if (Cond) { \
                LogError("Malformed binary table file '%1': %2", filename, STRINGIFY(Cond)); \
                return false; \
            } \
        } while (false)

    FAIL_PARSE_IF(table.sections.len != 1);
    FAIL_PARSE_IF(table.sections[0].value_len != sizeof(PackedGhmRoot));

    for (size_t i = 0; i < table.sections[0].values_count; i++) {
        GhmRootInfo ghm_root = {};

        PackedGhmRoot raw_ghm_root;
        memcpy(&raw_ghm_root, file_data + table.sections[0].raw_offset +
                                 i * sizeof(PackedGhmRoot), sizeof(PackedGhmRoot));
#ifdef ARCH_LITTLE_ENDIAN
        ReverseBytes(&raw_ghm_root.type_seq);
#endif

        // GHM root code
        {
            static char letters1[] = {0, 'C', 'H', 'K', 'M', 'Z', ' ', ' ', ' ', ' '};

            // TODO: Improve Fmt API to replace sprintf/snprintf
            snprintf(ghm_root.code.str, sizeof(ghm_root.code.str), "%02u%c%02u",
                     raw_ghm_root.cmd, letters1[raw_ghm_root.type_seq / 100 % 10],
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

        if (raw_ghm_root.childbirth_severity_mode) {
            FAIL_PARSE_IF(raw_ghm_root.childbirth_severity_mode < 2 ||
                          raw_ghm_root.childbirth_severity_mode > 4);
            ghm_root.childbirth_severity_list = raw_ghm_root.childbirth_severity_mode - 1;
        }

        ghm_root.cma_exclusion_offset = raw_ghm_root.cma_exclusion_offset;
        ghm_root.cma_exclusion_mask = raw_ghm_root.cma_exclusion_mask;

        out_ghm_roots->Append(ghm_root);
    }

#undef FAIL_PARSE_IF

    out_ghm_roots_guard.disable();
    return true;
}

bool ParseGhsDecisionTable(const uint8_t *file_data, const char *filename,
                           const TableInfo &table, DynamicArray<GhsDecisionNode> *out_nodes)
{
    DEFER_NC(out_nodes_guard, len = out_nodes->len) { out_nodes->RemoveFrom(len); };

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
    } __attribute__((__packed__));

#define FAIL_PARSE_IF(Cond) \
        do { \
            if (Cond) { \
                LogError("Malformed binary table file '%1': %2", filename, STRINGIFY(Cond)); \
                return false; \
            } \
        } while (false)

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
                (*out_nodes)[ghm_node_idx].u.ghm.next_ghm_idx = out_nodes->len;
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
            for (size_t j = first_test_idx; j < out_nodes->len; j++) {
                (*out_nodes)[j].u.test.fail_goto_idx = out_nodes->len + 1;
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

#undef FAIL_PARSE_IF

    out_nodes_guard.disable();
    return true;
}

bool ParseValueRangeTable(const uint8_t *file_data, const char *filename,
                          const TableInfo::Section &section,
                          DynamicArray<ValueRangeCell<2>> *out_cells)
{
    DEFER_NC(out_cells_guard, len = out_cells->len) { out_cells->RemoveFrom(len); };

    struct PackedCell {
        uint16_t var1_min;
        uint16_t var1_max;
        uint16_t var2_min;
        uint16_t var2_max;
        uint16_t value;
    } __attribute__((__packed__));

#define FAIL_PARSE_IF(Cond) \
        do { \
            if (Cond) { \
                LogError("Malformed binary table file '%1': %2", filename, STRINGIFY(Cond)); \
                return false; \
            } \
        } while (false)

    FAIL_PARSE_IF(section.value_len != sizeof(PackedCell));

    for (size_t i = 0; i < section.values_count; i++) {
        ValueRangeCell<2> cell = {};

        PackedCell raw_cell;
        memcpy(&raw_cell, file_data + section.raw_offset + i * sizeof(PackedCell),
               sizeof(PackedCell));
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

#undef FAIL_PARSE_IF

    out_cells_guard.disable();
    return true;
}
