// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "mco_table.hh"

namespace K {

struct ProcedureExtensionInfo {
    drd_ProcedureCode proc;
    int8_t phase;
    int8_t extension;

    LocalDate limit_dates[2];
};

struct ProcedureAdditionInfo {
    drd_ProcedureCode proc1;
    int8_t phase1;
    int8_t activity1;

    drd_ProcedureCode proc2;
    int8_t phase2;
    int8_t activity2;
};

#define FAIL_PARSE_IF(Filename, Cond) \
    do { \
        if (Cond) [[unlikely]] { \
            LogError("Malformed binary table file '%1': %2", \
                     (Filename) ? (Filename) : "?", K_STRINGIFY(Cond)); \
            return false; \
        } \
    } while (false)

Span<const char> mco_ProcedureInfo::ActivitiesToStr(Span<char> out_buf) const
{
    Size offset = 0;
    if (out_buf.len) [[unlikely]] {
        uint8_t value = activities;

        for (int i = 0; value && offset < out_buf.len - 1; i++) {
            out_buf[offset] = (char)('0' + i);
            offset += (value & 0x1);
            value >>= 1;
        }

        out_buf[offset] = 0;
    }

    return MakeSpan(out_buf.ptr, offset);
}

Span<const char> mco_ProcedureInfo::ExtensionsToStr(Span<char> out_buf) const
{
    Size offset = 0;
    if (extensions > 1) {
        uint64_t value = extensions >> 1;

        for (int i = 1; offset < out_buf.len && value; i++) {
            if (value & 0x1) {
                offset += Fmt(out_buf.Take(offset, out_buf.len - offset), "-%1,", FmtInt(i, 2)).len;
            }
            value >>= 1;
        }

        out_buf[--offset] = 0;
    }

    return MakeSpan(out_buf.ptr, offset);
}

LocalDate mco_ConvertDate1980(uint16_t days)
{
    static const int base_days = LocalDate(1979, 12, 31).ToJulianDays();
    return LocalDate::FromJulianDays(base_days + days);
}

static drd_DiagnosisCode ConvertDiagnosisCode(int16_t code123, uint16_t code456)
{
    drd_DiagnosisCode code = {};

    snprintf(code.str, K_SIZE(code.str), "%c%02d", code123 / 100 + 65, code123 % 100);

    static const char code456_chars[] = " 0123456789+";
    code456 %= 1584;
    code.str[3] = code456_chars[code456 / 132]; code456 %= 132;
    code.str[4] = code456_chars[code456 / 11]; code456 %= 11;
    code.str[5] = code456_chars[code456];
    for (int i = 5; i >= 3 && code.str[i] == ' '; i--) {
        code.str[i] = '\0';
    }

    return code;
}

static drd_ProcedureCode ConvertProcedureCode(int16_t root_idx, char char4, uint16_t seq)
{
    drd_ProcedureCode proc = {};

    for (int i = 0; i < 3; i++) {
        proc.str[2 - i] = (char)((root_idx % 26) + 65);
        root_idx /= 26;
    }
    snprintf(proc.str + 3, K_SIZE(proc.str) - 3, "%c%03u", (char4 % 26) + 65, seq % 1000);

    return proc;
}

// XXX: Be careful with overflow in offset and length checks
static bool ParseTableHeaders(Span<const uint8_t> file_data, const char *filename,
                              Allocator *str_alloc, HeapArray<mco_TableInfo> *out_tables)
{
    K_DEFER_NC(out_tables_guard, len = out_tables->len) { out_tables->RemoveFrom(len); };

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
    static_assert(K_SIZE(mco_TableInfo::raw_type) > K_SIZE(PackedHeader1111::name));
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

    PackedHeader1111 raw_main_header;
    PackedSection1111 raw_main_section;
    {
        FAIL_PARSE_IF(filename, file_data.len < K_SIZE(PackedHeader1111) + K_SIZE(PackedSection1111));

        MemCpy(&raw_main_header, file_data.ptr, K_SIZE(PackedHeader1111));
        FAIL_PARSE_IF(filename, raw_main_header.sections_count != 1);

        // Cheap and dirty way to make sure sscanf does not go overboard
        raw_main_header.pad1 = 0;

        MemCpy(&raw_main_section, file_data.ptr + K_SIZE(PackedHeader1111), K_SIZE(PackedSection1111));
        raw_main_section.values_count = BigEndian(raw_main_section.values_count);
        raw_main_section.value_len = BigEndian(raw_main_section.value_len);
        raw_main_section.raw_len = BigEndian(raw_main_section.raw_len);
        raw_main_section.raw_offset = BigEndian(raw_main_section.raw_offset);

        unsigned int version = 0, revision = 0;
        sscanf(raw_main_header.version, "%2u%2u", &version, &revision);
        FAIL_PARSE_IF(filename, version < 11 || (version == 11 && revision < 10));
        FAIL_PARSE_IF(filename, raw_main_section.value_len != K_SIZE(PackedTablePtr1111));
        FAIL_PARSE_IF(filename, file_data.len < K_SIZE(PackedHeader1111) +
                                raw_main_section.values_count * K_SIZE(PackedTablePtr1111));
    }

    for (int i = 0; i < raw_main_section.values_count; i++) {
        mco_TableInfo table = {};

        PackedTablePtr1111 raw_table_ptr;
        MemCpy(&raw_table_ptr, file_data.ptr + K_SIZE(PackedHeader1111) +
                                               K_SIZE(PackedSection1111) + i * K_SIZE(PackedTablePtr1111),
                   K_SIZE(PackedTablePtr1111));
        raw_table_ptr.date_range[0] = BigEndian(raw_table_ptr.date_range[0]);
        raw_table_ptr.date_range[1] = BigEndian(raw_table_ptr.date_range[1]);
        raw_table_ptr.raw_offset = BigEndian(raw_table_ptr.raw_offset);
        FAIL_PARSE_IF(filename, file_data.len < (Size)(raw_table_ptr.raw_offset +
                                                       K_SIZE(PackedHeader1111)));

        PackedHeader1111 raw_table_header;
        PackedSection1111 raw_table_sections[K_LEN(table.sections.data)];
        {
            bool weird_section = false;

            MemCpy(&raw_table_header, file_data.ptr + raw_table_ptr.raw_offset, K_SIZE(PackedHeader1111));
            if (!memcmp(raw_table_header.signature, "GESTCOMP", 8)) [[unlikely]] {
                weird_section = true;

                MemMove(&raw_table_header.pad1, raw_table_header.name,
                        K_SIZE(PackedHeader1111) - offsetof(PackedHeader1111, pad1));
                MemCpy(&raw_table_header.name, raw_table_header.signature, K_SIZE(raw_table_header.name));
            }
            FAIL_PARSE_IF(filename, file_data.len < (Size)(raw_table_ptr.raw_offset +
                                                           (uint32_t)raw_table_header.sections_count * K_SIZE(PackedSection1111)));
            FAIL_PARSE_IF(filename, raw_table_header.sections_count > K_LEN(raw_table_sections));

            // Cheap and dirty way to make sure sscanf does not go overboard
            raw_table_header.pad1 = 0;

            for (int j = 0; j < raw_table_header.sections_count; j++) {
                MemCpy(&raw_table_sections[j], file_data.ptr + raw_table_ptr.raw_offset + K_SIZE(PackedHeader1111) +
                                                               j * K_SIZE(PackedSection1111), K_SIZE(PackedSection1111));
                if (weird_section) [[unlikely]] {
                    MemMove((uint8_t *)&raw_table_sections[j] + 8, &raw_table_sections[j], K_SIZE(PackedSection1111) - 8);
                }
                raw_table_sections[j].values_count = BigEndian(raw_table_sections[j].values_count);
                raw_table_sections[j].value_len = BigEndian(raw_table_sections[j].value_len);
                raw_table_sections[j].raw_len = BigEndian(raw_table_sections[j].raw_len);
                raw_table_sections[j].raw_offset = BigEndian(raw_table_sections[j].raw_offset);

                FAIL_PARSE_IF(filename, file_data.len < (Size)(raw_table_ptr.raw_offset +
                                                               raw_table_sections[j].raw_offset +
                                                               raw_table_sections[j].raw_len));
            }
        }

        if (str_alloc) {
            table.filename = DuplicateString(filename, str_alloc).ptr;
        }

        // Table type
        CopyString(raw_table_header.name, table.raw_type);
        table.raw_type[strcspn(table.raw_type, " ")] = '\0';
        if (TestStr(table.raw_type, "ARBREDEC")) {
            table.type = mco_TableType::GhmDecisionTree;
        } else if (TestStr(table.raw_type, "DIAG10CR")) {
            table.type = mco_TableType::DiagnosisTable;
        } else if (TestStr(table.raw_type, "CCAMCARA")) {
            table.type = mco_TableType::ProcedureTable;
        } else if (TestStr(table.raw_type, "RGHMINFO")) {
            table.type = mco_TableType::GhmRootTable;
        } else if (TestStr(table.raw_type, "GHSINFO")) {
            table.type = mco_TableType::GhmToGhsTable;
        } else if (TestStr(table.raw_type, "TABCOMBI")) {
            table.type = mco_TableType::SeverityTable;
        } else if (TestStr(table.raw_type, "GESTCOMP")) {
            table.type = mco_TableType::ProcedureAdditionTable;
        } else if (TestStr(table.raw_type, "CCAMDESC")) {
            table.type = mco_TableType::ProcedureExtensionTable;
        } else if (TestStr(table.raw_type, "AUTOREFS")) {
            table.type = mco_TableType::AuthorizationTable;
        } else if (TestStr(table.raw_type, "SRCDGACT")) {
            table.type = mco_TableType::SrcPairTable;
        } else if (TestStr(table.raw_type, "GHSMINOR")) {
            table.type = mco_TableType::GhsMinorationTable;
        } else {
            table.type = mco_TableType::UnknownTable;
        }

        // Other metadata
        sscanf(raw_main_header.date, "%2" SCNd8 "%2" SCNd8 "%4" SCNd16,
               &table.build_date.st.day, &table.build_date.st.month,
               &table.build_date.st.year);
        table.build_date.st.year = (int16_t)(table.build_date.st.year + 2000);
        FAIL_PARSE_IF(filename, !table.build_date.IsValid());
        sscanf(raw_table_header.version, "%2" SCNu16 "%2" SCNu16,
               &table.version[0], &table.version[1]);
        table.limit_dates[0] = mco_ConvertDate1980(raw_table_ptr.date_range[0]);
        if (table.type == mco_TableType::GhmDecisionTree &&
                raw_table_ptr.date_range[1] == UINT16_MAX) {
            // Most tab files use UINT16_MAX, but it's dangerous because it means we can
            // continue to use old tables forever without warning. Don't obey for key table,
            // but not all of them because a few remain in use for several versions.
            table.limit_dates[1] = LocalDate((int16_t)(table.limit_dates[0].st.year + 1), 3, 1);
        } else {
            table.limit_dates[1] = mco_ConvertDate1980(raw_table_ptr.date_range[1]);
        }
        FAIL_PARSE_IF(filename, table.limit_dates[1] <= table.limit_dates[0]);

        // Parse table sections
        table.sections.len = raw_table_header.sections_count;
        for (int j = 0; j < raw_table_header.sections_count; j++) {
            FAIL_PARSE_IF(filename, raw_table_sections[j].raw_len !=
                                        (uint32_t)raw_table_sections[j].values_count *
                                        raw_table_sections[j].value_len);
            table.sections[j].raw_offset = (Size)(raw_table_ptr.raw_offset +
                                                  raw_table_sections[j].raw_offset);
            table.sections[j].raw_len = (Size)raw_table_sections[j].raw_len;
            table.sections[j].values_count = raw_table_sections[j].values_count;
            table.sections[j].value_len = raw_table_sections[j].value_len;
        }

        out_tables->Append(table);
    }

    out_tables_guard.Disable();
    return true;
}

static bool ParseGhmDecisionTree(const uint8_t *file_data, const mco_TableInfo &table,
                                 HeapArray<mco_GhmDecisionNode> *out_nodes)
{
    K_DEFER_NC(out_nodes_guard, len = out_nodes->len) { out_nodes->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedTreeNode {
        uint8_t function;
        uint8_t params[2];
        uint8_t children_count;
        uint16_t children_idx;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, table.sections.len != 1);
    FAIL_PARSE_IF(table.filename, table.sections[0].value_len != K_SIZE(PackedTreeNode));

    for (Size i = 0; i < table.sections[0].values_count; i++) {
        mco_GhmDecisionNode ghm_node = {};

        PackedTreeNode raw_node;
        MemCpy(&raw_node, file_data + table.sections[0].raw_offset +
                                      i * K_SIZE(PackedTreeNode), K_SIZE(PackedTreeNode));
        raw_node.children_idx = BigEndian(raw_node.children_idx);

        ghm_node.function = raw_node.function;
        if (raw_node.function != 12) {
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

            FAIL_PARSE_IF(table.filename, !ghm_node.u.test.children_count);
            FAIL_PARSE_IF(table.filename, ghm_node.u.test.children_idx > table.sections[0].values_count);
            FAIL_PARSE_IF(table.filename, ghm_node.u.test.children_count > table.sections[0].values_count -
                                                                           ghm_node.u.test.children_idx);
        } else {
            static char chars1[] = { 0, 'C', 'H', 'K', 'M', 'Z', ' ', ' ', ' ', ' ' };
            static char chars4[] = { 0, 'A', 'B', 'C', 'D', 'E', 'J', 'Z', ' ', ' ' };

            ghm_node.u.ghm.ghm.parts.cmd = (int8_t)raw_node.params[1];
            ghm_node.u.ghm.ghm.parts.type = chars1[(raw_node.children_idx / 1000) % 10];
            ghm_node.u.ghm.ghm.parts.seq = (int8_t)((raw_node.children_idx / 10) % 100);
            ghm_node.u.ghm.ghm.parts.mode = chars4[raw_node.children_idx % 10];
            ghm_node.u.ghm.error = raw_node.params[0];
        }

        out_nodes->Append(ghm_node);
    }

    out_nodes_guard.Disable();
    return true;
}

static bool ParseDiagnosisTable(const uint8_t *file_data, const mco_TableInfo &table,
                                HeapArray<mco_DiagnosisInfo> *out_diagnoses)
{
    K_DEFER_NC(out_guard, len = out_diagnoses->len) { out_diagnoses->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedDiagnosisPtr  {
        uint16_t code456;

        uint16_t section2_idx;
        uint8_t section3_idx;
        uint16_t section4_bit;
        uint16_t section4_idx;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, table.sections.len != 5);
    FAIL_PARSE_IF(table.filename, table.sections[0].values_count != 26 * 100 ||
                                  table.sections[0].value_len != 2);
    FAIL_PARSE_IF(table.filename, table.sections[1].value_len != K_SIZE(PackedDiagnosisPtr));
    FAIL_PARSE_IF(table.filename, !table.sections[2].value_len || table.sections[2].value_len % 2 ||
                                  table.sections[2].value_len / 2 > K_SIZE(mco_DiagnosisInfo::raw));
    FAIL_PARSE_IF(table.filename, !table.sections[3].value_len ||
                                  table.sections[3].value_len > K_SIZE(mco_DiagnosisInfo::warnings) * 8);
    FAIL_PARSE_IF(table.filename, !table.sections[4].value_len);

    Size block_end = table.sections[1].raw_offset;
    for (int16_t root_idx = 0; root_idx < table.sections[0].values_count; root_idx++) {
        Size block_start = block_end;

        // Find block end
        {
            const uint8_t *end_idx_ptr = file_data + table.sections[0].raw_offset +
                                         root_idx * 2;
            uint16_t end_idx = (uint16_t)((end_idx_ptr[0] << 8) | end_idx_ptr[1]);
            FAIL_PARSE_IF(table.filename, end_idx > table.sections[1].values_count);
            block_end = table.sections[1].raw_offset + end_idx * K_SIZE(PackedDiagnosisPtr);
        }
        if (block_end == block_start)
            continue;

        for (Size block_offset = block_start; block_offset < block_end;
             block_offset += K_SIZE(PackedDiagnosisPtr)) {
            PackedDiagnosisPtr raw_diag_ptr;
            MemCpy(&raw_diag_ptr, file_data + block_offset, K_SIZE(PackedDiagnosisPtr));
            raw_diag_ptr.code456 = BigEndian(raw_diag_ptr.code456);
            raw_diag_ptr.section2_idx = BigEndian(raw_diag_ptr.section2_idx);
            raw_diag_ptr.section4_bit = BigEndian(raw_diag_ptr.section4_bit);
            raw_diag_ptr.section4_idx = BigEndian(raw_diag_ptr.section4_idx);
            FAIL_PARSE_IF(table.filename,
                          raw_diag_ptr.section2_idx >= table.sections[2].values_count);
            FAIL_PARSE_IF(table.filename,
                          raw_diag_ptr.section3_idx >= table.sections[3].values_count);
            FAIL_PARSE_IF(table.filename,
                          raw_diag_ptr.section4_idx >= table.sections[4].values_count);

            const auto add_diagnosis_info = [&](uint8_t sexes, const uint8_t *raw) {
                mco_DiagnosisInfo diag_info = {};

                diag_info.diag = ConvertDiagnosisCode(root_idx, raw_diag_ptr.code456);
                diag_info.sexes = sexes;

                const uint8_t *warn_data = file_data + table.sections[3].raw_offset +
                                           raw_diag_ptr.section3_idx * table.sections[3].value_len;
                for (int i = 0; i < table.sections[3].value_len; i++) {
                    if (warn_data[i]) {
                        diag_info.warnings = (uint16_t)(diag_info.warnings | (1 << i));
                    }
                }

                MemCpy(diag_info.raw, raw, table.sections[2].value_len / 2);

                diag_info.cmd = raw[0];
                diag_info.jump = raw[1];
                if (raw[21] & 0x40) {
                    diag_info.severity = 3;
                } else if (raw[21] & 0x80) {
                    diag_info.severity = 2;
                } else if (raw[20] & 0x1) {
                    diag_info.severity = 1;
                }

                if (raw[19] & 0x10) {
                    diag_info.cma_minimum_age = 14;
                }
                if (raw[19] & 0x8 || diag_info.diag.str[0] == 'P') {
                    diag_info.cma_maximum_age = 2;
                }
                diag_info.exclusion_set_idx = raw_diag_ptr.section4_idx;
                diag_info.cma_exclusion_mask.offset = (uint8_t)(raw_diag_ptr.section4_bit >> 3);
                diag_info.cma_exclusion_mask.value =
                    (uint8_t)(0x80 >> (raw_diag_ptr.section4_bit & 0x7));

                out_diagnoses->Append(diag_info);
            };

            const uint8_t *sex1_data = file_data + table.sections[2].raw_offset +
                                       raw_diag_ptr.section2_idx * table.sections[2].value_len;
            const uint8_t *sex2_data = sex1_data + table.sections[2].value_len / 2;

            if (memcmp(sex1_data, sex2_data, table.sections[2].value_len / 2)) {
                add_diagnosis_info(0x1, sex1_data);
                add_diagnosis_info(0x2, sex2_data);
            } else {
                add_diagnosis_info(0x3, sex1_data);
            }
        }
    }

    out_guard.Disable();
    return true;
}

static bool ParseExclusionTable(const uint8_t *file_data, const mco_TableInfo &table,
                                HeapArray<mco_ExclusionInfo> *out_exclusions)
{
    K_DEFER_NC(out_exclusions_guard, len = out_exclusions->len) { out_exclusions->RemoveFrom(len); };

    FAIL_PARSE_IF(table.filename, table.sections.len != 5);
    FAIL_PARSE_IF(table.filename, !table.sections[4].value_len);
    FAIL_PARSE_IF(table.filename, table.sections[4].value_len > K_SIZE(mco_ExclusionInfo::raw));

    for (Size i = 0; i < table.sections[4].values_count; i++) {
        mco_ExclusionInfo *excl = out_exclusions->AppendDefault();
        MemCpy(excl->raw, file_data + table.sections[4].raw_offset +
                                      i * table.sections[4].value_len, table.sections[4].value_len);
        MemSet(excl->raw + table.sections[4].value_len, 0, K_SIZE(excl->raw) - table.sections[4].value_len);
    }

    out_exclusions_guard.Disable();
    return true;
}

static bool ParseProcedureTable(const uint8_t *file_data, const mco_TableInfo &table,
                                HeapArray<mco_ProcedureInfo> *out_procs)
{
    K_DEFER_NC(out_proc_guard, len = out_procs->len) { out_procs->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedProcedurePtr  {
        char char4;
        uint16_t seq_phase;

        uint16_t section2_idx;
        uint16_t date_min;
        uint16_t date_max;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, table.sections.len != 3);
    FAIL_PARSE_IF(table.filename, table.sections[0].values_count != 26 * 26 * 26 ||
                                  table.sections[0].value_len != 2);
    FAIL_PARSE_IF(table.filename, table.sections[1].value_len != K_SIZE(PackedProcedurePtr));
    FAIL_PARSE_IF(table.filename, !table.sections[2].value_len ||
                                  table.sections[2].value_len > K_SIZE(mco_ProcedureInfo::bytes));

    Size block_end = table.sections[1].raw_offset;
    for (int16_t root_idx = 0; root_idx < table.sections[0].values_count; root_idx++) {
        Size block_start = block_end;

        // Find block end
        {
            const uint8_t *end_idx_ptr = file_data + table.sections[0].raw_offset +
                                         root_idx * 2;
            uint16_t end_idx = (uint16_t)((end_idx_ptr[0] << 8) | end_idx_ptr[1]);
            FAIL_PARSE_IF(table.filename, end_idx > table.sections[1].values_count);
            block_end = table.sections[1].raw_offset + end_idx * K_SIZE(PackedProcedurePtr);
        }
        if (block_end == block_start)
            continue;

        for (Size block_offset = block_start; block_offset < block_end;
             block_offset += K_SIZE(PackedProcedurePtr)) {
            mco_ProcedureInfo proc = {};

            PackedProcedurePtr raw_proc_ptr;
            MemCpy(&raw_proc_ptr, file_data + block_offset, K_SIZE(PackedProcedurePtr));
            raw_proc_ptr.seq_phase = BigEndian(raw_proc_ptr.seq_phase);
            raw_proc_ptr.section2_idx = BigEndian(raw_proc_ptr.section2_idx);
            raw_proc_ptr.date_min = BigEndian(raw_proc_ptr.date_min);
            raw_proc_ptr.date_max = BigEndian(raw_proc_ptr.date_max);
            FAIL_PARSE_IF(table.filename,
                          raw_proc_ptr.section2_idx >= table.sections[2].values_count);

            // CCAM code and phase
            proc.proc = ConvertProcedureCode(root_idx, raw_proc_ptr.char4,
                                             raw_proc_ptr.seq_phase / 10);
            proc.phase = (char)(raw_proc_ptr.seq_phase % 10);

            // CCAM information and lists
            {
                proc.limit_dates[0] = mco_ConvertDate1980(raw_proc_ptr.date_min);
                if (raw_proc_ptr.date_max < UINT16_MAX) {
                    proc.limit_dates[1] = mco_ConvertDate1980((uint16_t)(raw_proc_ptr.date_max + 1));
                } else {
                    proc.limit_dates[1] = mco_MaxDate1980;
                }

                const uint8_t *proc_data = file_data + table.sections[2].raw_offset +
                                           raw_proc_ptr.section2_idx * table.sections[2].value_len;
                MemCpy(proc.bytes, proc_data, table.sections[2].value_len);
            }

            // CCAM activities
            if (proc.bytes[31] & 0x1) {
                proc.activities |= 1 << 1;
            }
            if (proc.bytes[32] & 0x80) {
                proc.activities |= 1 << 2;
            }
            if (proc.bytes[32] & 0x40) {
                proc.activities |= 1 << 3;
            }
            if (proc.bytes[22] & 0x20) {
                proc.activities |= 1 << 4;
            }
            if (proc.bytes[32] & 0x20) {
                proc.activities |= 1 << 5;
            }

            out_procs->Append(proc);
        }
    }

    out_proc_guard.Disable();
    return true;
}

static bool ParseProcedureAdditionTable(const uint8_t *file_data, const mco_TableInfo &table,
                                        HeapArray<ProcedureAdditionInfo> *out_additions)
{
    K_DEFER_NC(out_guard, len = out_additions->len) { out_additions->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedRootPtr {
        uint16_t count;
        uint16_t proc1_idx;
    };
    struct PackedProc1 {
        char char4;
        uint32_t seq_phase_activity;
        uint8_t count;
        uint16_t proc2_idx;
    };
    struct PackedProc2 {
        uint16_t root_idx;
        char char4;
        uint32_t seq_phase_activity;
    };
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, table.sections.len != 4);
    FAIL_PARSE_IF(table.filename, table.sections[0].values_count != 26 * 26 * 26 ||
                                  table.sections[0].value_len != K_SIZE(PackedRootPtr));
    FAIL_PARSE_IF(table.filename, table.sections[1].value_len != K_SIZE(PackedProc1));
    FAIL_PARSE_IF(table.filename, table.sections[2].value_len != 2);
    FAIL_PARSE_IF(table.filename, table.sections[3].value_len != K_SIZE(PackedProc2));

    for (int16_t root_idx = 0; root_idx < table.sections[0].values_count; root_idx++) {
        PackedRootPtr raw_root_ptr;
        MemCpy(&raw_root_ptr, file_data + table.sections[0].raw_offset +
                                          root_idx * K_SIZE(PackedRootPtr), K_SIZE(PackedRootPtr));
        raw_root_ptr.count = BigEndian(raw_root_ptr.count);
        raw_root_ptr.proc1_idx = BigEndian(raw_root_ptr.proc1_idx);
        FAIL_PARSE_IF(table.filename,
                      raw_root_ptr.proc1_idx > table.sections[1].values_count - raw_root_ptr.count);

        for (Size i = 0; i < raw_root_ptr.count; i++) {
            PackedProc1 raw_proc1;
            MemCpy(&raw_proc1, file_data + table.sections[1].raw_offset +
                                           (raw_root_ptr.proc1_idx + i) * table.sections[1].value_len, K_SIZE(PackedProc1));
            raw_proc1.seq_phase_activity = BigEndian(raw_proc1.seq_phase_activity);
            raw_proc1.proc2_idx = BigEndian(raw_proc1.proc2_idx);
            FAIL_PARSE_IF(table.filename,
                          raw_proc1.proc2_idx > table.sections[2].values_count - raw_proc1.count);

            drd_ProcedureCode proc1 = ConvertProcedureCode(root_idx, raw_proc1.char4,
                                                       (uint16_t)(raw_proc1.seq_phase_activity / 100));
            int8_t phase1 = (int8_t)(raw_proc1.seq_phase_activity / 10 % 10);
            int8_t activity1 = (int8_t)(raw_proc1.seq_phase_activity % 10);

            for (Size j = 0; j < raw_proc1.count; j++) {
                ProcedureAdditionInfo addition_info = {};

                uint16_t proc2_idx;
                {
                    const uint8_t *proc2_idx_ptr = file_data + table.sections[2].raw_offset +
                                                               (raw_proc1.proc2_idx + j) * K_SIZE(uint16_t);
                    proc2_idx = (uint16_t)((proc2_idx_ptr[0] << 8) | proc2_idx_ptr[1]);
                    FAIL_PARSE_IF(table.filename, proc2_idx >= table.sections[3].values_count);
                }

                PackedProc2 raw_proc2;
                MemCpy(&raw_proc2, file_data + table.sections[3].raw_offset +
                                               proc2_idx * K_SIZE(PackedProc2), K_SIZE(PackedProc2));
                raw_proc2.root_idx = BigEndian(raw_proc2.root_idx);
                raw_proc2.seq_phase_activity = BigEndian(raw_proc2.seq_phase_activity);
                FAIL_PARSE_IF(table.filename, raw_proc2.root_idx >= 26 * 26 * 26);

                addition_info.proc1 = proc1;
                addition_info.phase1 = phase1;
                addition_info.activity1 = activity1;
                addition_info.proc2 =
                    ConvertProcedureCode((int16_t)raw_proc2.root_idx, raw_proc2.char4,
                                         (uint16_t)(raw_proc2.seq_phase_activity / 100));
                addition_info.phase2 = (int8_t)(raw_proc2.seq_phase_activity / 10 % 10);
                addition_info.activity2 = (int8_t)(raw_proc2.seq_phase_activity % 10);

                out_additions->Append(addition_info);
            }
        }
    }

    out_guard.Disable();
    return true;
}

static bool ParseProcedureExtensionTable(const uint8_t *file_data, const mco_TableInfo &table,
                                         HeapArray<ProcedureExtensionInfo> *out_extensions)
{
    K_DEFER_NC(out_guard, len = out_extensions->len) { out_extensions->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedProcedureExtension {
        char char4;
        uint16_t seq_phase;
        uint8_t extension;

        uint16_t date_min;
        uint16_t date_max;
    };
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, table.sections.len != 2);
    FAIL_PARSE_IF(table.filename, table.sections[0].values_count != 26 * 26 * 26 ||
                                  table.sections[0].value_len != 2);
    if (table.version[0] >= 12 || (table.version[0] == 11 && table.version[1] >= 29)) {
        FAIL_PARSE_IF(table.filename, table.sections[1].value_len != K_SIZE(PackedProcedureExtension));
    } else {
        FAIL_PARSE_IF(table.filename, table.sections[1].value_len != K_SIZE(PackedProcedureExtension) - 4);
    }

    Size block_end = table.sections[1].raw_offset;
    for (int16_t root_idx = 0; root_idx < table.sections[0].values_count; root_idx++) {
        Size block_start = block_end;

        // Find block end
        {
            const uint8_t *end_idx_ptr = file_data + table.sections[0].raw_offset +
                                         root_idx * 2;
            uint16_t end_idx = (uint16_t)((end_idx_ptr[0] << 8) | end_idx_ptr[1]);
            FAIL_PARSE_IF(table.filename, end_idx > table.sections[1].values_count);
            block_end = table.sections[1].raw_offset + end_idx * table.sections[1].value_len;
        }
        if (block_end == block_start)
            continue;

        for (Size block_offset = block_start; block_offset < block_end;
             block_offset += table.sections[1].value_len) {
            ProcedureExtensionInfo ext_info = {};

            PackedProcedureExtension raw_proc_ext;
            MemCpy(&raw_proc_ext, file_data + block_offset, table.sections[1].value_len);
            raw_proc_ext.seq_phase = BigEndian(raw_proc_ext.seq_phase);
            raw_proc_ext.date_min = BigEndian(raw_proc_ext.date_min);
            raw_proc_ext.date_max = BigEndian(raw_proc_ext.date_max);

            ext_info.proc = ConvertProcedureCode(root_idx, raw_proc_ext.char4,
                                                 raw_proc_ext.seq_phase / 10);
            ext_info.phase = (char)(raw_proc_ext.seq_phase % 10);

            FAIL_PARSE_IF(table.filename, raw_proc_ext.extension > INT8_MAX);
            ext_info.extension = (int8_t)raw_proc_ext.extension;

            if (table.sections[1].value_len >= 8) {
                ext_info.limit_dates[0] = mco_ConvertDate1980(raw_proc_ext.date_min);
                ext_info.limit_dates[1] = mco_ConvertDate1980(raw_proc_ext.date_max);
            } else {
                ext_info.limit_dates[0] = LocalDate(2000, 1, 1);
                ext_info.limit_dates[1] = mco_MaxDate1980;
            }

            out_extensions->Append(ext_info);
        }
    }

    out_guard.Disable();
    return true;
}

static bool ParseGhmRootTable(const uint8_t *file_data, const mco_TableInfo &table,
                              HeapArray<mco_GhmRootInfo> *out_ghm_roots)
{
    K_DEFER_NC(out_ghm_roots_guard, len = out_ghm_roots->len) { out_ghm_roots->RemoveFrom(len); };

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
        uint8_t confirm_duration_threshold;
        uint8_t childbirth_severity_mode; // Appeared in FG 11d
        uint8_t ignore_raac; // Appeared in FG 2019
        uint8_t force_ghs; // Appeared in FG 2020
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, table.sections.len != 1);
    if (table.version[0] >= 12 || (table.version[0] == 11 && table.version[1] >= 29)) {
        FAIL_PARSE_IF(table.filename, table.sections[0].value_len != K_SIZE(PackedGhmRoot));
    } else if (table.version[0] == 11 && table.version[1] >= 28) {
        FAIL_PARSE_IF(table.filename, table.sections[0].value_len != K_SIZE(PackedGhmRoot) - 1);
    } else if (table.version[0] == 11 && table.version[1] >= 15) {
        FAIL_PARSE_IF(table.filename, table.sections[0].value_len != K_SIZE(PackedGhmRoot) - 2);
    } else {
        FAIL_PARSE_IF(table.filename, table.sections[0].value_len != K_SIZE(PackedGhmRoot) - 3);
    }

    for (Size i = 0; i < table.sections[0].values_count; i++) {
        mco_GhmRootInfo ghm_root = {};

        PackedGhmRoot raw_ghm_root;
        MemCpy(&raw_ghm_root, file_data + table.sections[0].raw_offset +
                                          i * table.sections[0].value_len, table.sections[0].value_len);
        raw_ghm_root.type_seq = BigEndian(raw_ghm_root.type_seq);

        // GHM root code
        {
            static char chars1[] = { 0, 'C', 'H', 'K', 'M', 'Z', ' ', ' ', ' ', ' ' };

            ghm_root.ghm_root.parts.cmd = (int8_t)raw_ghm_root.cmd;
            ghm_root.ghm_root.parts.type = chars1[raw_ghm_root.type_seq / 100 % 10];
            ghm_root.ghm_root.parts.seq = (int8_t)(raw_ghm_root.type_seq % 100);
        }

        switch (raw_ghm_root.duration_severity_mode) {
            case 1: {
                ghm_root.allow_ambulatory = true;
            } break;
            case 2: {
                ghm_root.short_duration_threshold = 1;
            } break;
            case 3: {
                ghm_root.short_duration_threshold = 2;
            } break;
            case 4: {
                ghm_root.short_duration_threshold = 3;
            } break;
        }
        ghm_root.confirm_duration_threshold = (int8_t)raw_ghm_root.confirm_duration_threshold;

        if (raw_ghm_root.young_severity_mode == 1) {
            ghm_root.young_age_threshold = 2;
            ghm_root.young_severity_limit = 1;
        }
        switch (raw_ghm_root.old_severity_mode) {
            case 1: {
                ghm_root.old_age_threshold = 70;
                ghm_root.old_severity_limit = 1;
            } break;
            case 2: {
                ghm_root.old_age_threshold = 80;
                ghm_root.old_severity_limit = 1;
            } break;
            case 3: {
                ghm_root.old_age_threshold = 70;
                ghm_root.old_severity_limit = 2;
            } break;
            case 4: {
                ghm_root.old_age_threshold = 80;
                ghm_root.old_severity_limit = 2;
            } break;
            case 5: {
                ghm_root.old_age_threshold = 70;
                ghm_root.old_severity_limit = 3;
            } break;
            case 6: {
                ghm_root.old_age_threshold = 80;
                ghm_root.old_severity_limit = 3;
            } break;
        }

        if (table.sections[0].value_len >= 12 && raw_ghm_root.childbirth_severity_mode) {
            FAIL_PARSE_IF(table.filename, raw_ghm_root.childbirth_severity_mode < 2 ||
                                          raw_ghm_root.childbirth_severity_mode > 4);
            ghm_root.childbirth_severity_list = (int8_t)(raw_ghm_root.childbirth_severity_mode - 1);
        }
        if (table.sections[0].value_len >= 13) {
            FAIL_PARSE_IF(table.filename, raw_ghm_root.ignore_raac != 0 &&
                                          raw_ghm_root.ignore_raac != 1);
            ghm_root.allow_raac = !raw_ghm_root.ignore_raac;
        }
        if (table.sections[0].value_len >= 14) {
            FAIL_PARSE_IF(table.filename, raw_ghm_root.force_ghs != 0 &&
                                          raw_ghm_root.force_ghs != 1);
            ghm_root.gradated = !raw_ghm_root.force_ghs;
        }

        ghm_root.cma_exclusion_mask.offset = raw_ghm_root.cma_exclusion_offset;
        ghm_root.cma_exclusion_mask.value = raw_ghm_root.cma_exclusion_mask;

        out_ghm_roots->Append(ghm_root);
    }

    out_ghm_roots_guard.Disable();
    return true;
}

static bool ParseSeverityTable(const uint8_t *file_data, const mco_TableInfo &table,
                               int section_idx, HeapArray<mco_ValueRangeCell<2>> *out_cells)
{
    K_DEFER_NC(out_cells_guard, len = out_cells->len) { out_cells->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedCell {
        uint16_t var1_min;
        uint16_t var1_max;
        uint16_t var2_min;
        uint16_t var2_max;
        uint16_t value;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, section_idx >= table.sections.len);
    FAIL_PARSE_IF(table.filename, table.sections[section_idx].value_len != K_SIZE(PackedCell));

    for (Size i = 0; i < table.sections[section_idx].values_count; i++) {
        mco_ValueRangeCell<2> cell = {};

        PackedCell raw_cell;
        MemCpy(&raw_cell, file_data + table.sections[section_idx].raw_offset +
                                      i * K_SIZE(PackedCell), K_SIZE(PackedCell));
        raw_cell.var1_min = BigEndian(raw_cell.var1_min);
        raw_cell.var1_max = BigEndian(raw_cell.var1_max);
        raw_cell.var2_min = BigEndian(raw_cell.var2_min);
        raw_cell.var2_max = BigEndian(raw_cell.var2_max);
        raw_cell.value = BigEndian(raw_cell.value);

        cell.limits[0].min = raw_cell.var1_min;
        cell.limits[0].max = raw_cell.var1_max + 1;
        cell.limits[1].min = raw_cell.var2_min;
        cell.limits[1].max = raw_cell.var2_max + 1;
        cell.value = raw_cell.value;

        out_cells->Append(cell);
    }

    out_cells_guard.Disable();
    return true;
}

static bool ParseGhmToGhsTable(const uint8_t *file_data, const mco_TableInfo &table,
                               HeapArray<mco_GhmToGhsInfo> *out_ghs)
{
    Size start_ghs_len = out_ghs->len;
    K_DEFER_N(out_ghs_guard) { out_ghs->RemoveFrom(start_ghs_len); };

#pragma pack(push, 1)
    struct PackedGhsNode {
        uint8_t cmd;
        uint16_t type_seq;
        uint8_t low_duration_mode;
        uint8_t function;
        uint8_t params[2];
        uint8_t skip_after_failure;
        uint8_t valid_ghs;
        struct {
            uint16_t ghs_code;
             // We get those from the pricing tables, so they're ignored here
            uint16_t high_duration_threshold;
            uint16_t low_duration_threshold;
        } sectors[2];
	};
    static_assert(K_LEN(PackedGhsNode().sectors) == K_LEN(mco_GhmToGhsInfo().ghs));
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, table.sections.len != 1);
    FAIL_PARSE_IF(table.filename, table.sections[0].value_len != K_SIZE(PackedGhsNode));

    mco_GhmToGhsInfo current_ghs = {};
    for (Size i = 0; i < table.sections[0].values_count; i++) {
        PackedGhsNode raw_ghs_node;
        MemCpy(&raw_ghs_node, file_data + table.sections[0].raw_offset +
                                          i * K_SIZE(PackedGhsNode), K_SIZE(PackedGhsNode));
        raw_ghs_node.type_seq = BigEndian(raw_ghs_node.type_seq);
        for (int j = 0; j < 2; j++) {
            raw_ghs_node.sectors[j].ghs_code = BigEndian(raw_ghs_node.sectors[j].ghs_code);
            raw_ghs_node.sectors[j].high_duration_threshold =
                BigEndian(raw_ghs_node.sectors[j].high_duration_threshold);
            raw_ghs_node.sectors[j].low_duration_threshold =
                BigEndian(raw_ghs_node.sectors[j].low_duration_threshold);
        }

        if (!current_ghs.ghm.IsValid()) {
            static char chars1[] = { 0, 'C', 'H', 'K', 'M', 'Z' };
            static char chars4[] = { 0, 'A', 'B', 'C', 'D', 'E', 'J',
                                     'Z', 'T', '1', '2', '3', '4' };

            current_ghs.ghm.parts.cmd = (int8_t)raw_ghs_node.cmd;
            current_ghs.ghm.parts.type = chars1[raw_ghs_node.type_seq / 10000 % 6];
            current_ghs.ghm.parts.seq = (int8_t)(raw_ghs_node.type_seq / 100 % 100);
            current_ghs.ghm.parts.mode = chars4[raw_ghs_node.type_seq % 100 % 13];
        }

        switch (raw_ghs_node.function) {
            case 0: {
                FAIL_PARSE_IF(table.filename, !raw_ghs_node.valid_ghs);
            } break;

            case 1: {
                drd_ListMask mask;
                mask.offset = raw_ghs_node.params[0];
                mask.value = raw_ghs_node.params[1];
                FAIL_PARSE_IF(table.filename, !current_ghs.procedure_masks.Available());
                current_ghs.procedure_masks.Append(mask);
                current_ghs.conditions_count++;
            } break;

            case 2: {
                FAIL_PARSE_IF(table.filename, raw_ghs_node.params[0]);
                FAIL_PARSE_IF(table.filename, current_ghs.unit_authorization);
                current_ghs.unit_authorization = (int8_t)raw_ghs_node.params[1];
                current_ghs.conditions_count++;
            } break;

            case 3: {
                FAIL_PARSE_IF(table.filename, raw_ghs_node.params[0]);
                FAIL_PARSE_IF(table.filename, current_ghs.bed_authorization);
                current_ghs.bed_authorization = (int8_t)raw_ghs_node.params[1];
                current_ghs.conditions_count++;
            } break;

            case 5: {
                FAIL_PARSE_IF(table.filename, current_ghs.main_diagnosis_mask.offset ||
                                              current_ghs.main_diagnosis_mask.value);
                current_ghs.main_diagnosis_mask.offset = raw_ghs_node.params[0];
                current_ghs.main_diagnosis_mask.value = raw_ghs_node.params[1];
                current_ghs.conditions_count++;
            } break;

            case 6: {
                FAIL_PARSE_IF(table.filename, raw_ghs_node.params[0]);
                FAIL_PARSE_IF(table.filename, current_ghs.minimum_duration);
                current_ghs.minimum_duration = (int8_t)(raw_ghs_node.params[1] + 1);
                current_ghs.conditions_count++;
            } break;

            case 7: {
                FAIL_PARSE_IF(table.filename, current_ghs.diagnosis_mask.offset ||
                                              current_ghs.diagnosis_mask.value);
                current_ghs.diagnosis_mask.offset = raw_ghs_node.params[0];
                current_ghs.diagnosis_mask.value = raw_ghs_node.params[1];
                current_ghs.conditions_count++;
            } break;

            case 8: {
                FAIL_PARSE_IF(table.filename, raw_ghs_node.params[0]);
                FAIL_PARSE_IF(table.filename, current_ghs.minimum_age);
                current_ghs.minimum_age = (int8_t)raw_ghs_node.params[1];
                current_ghs.conditions_count++;
            } break;

            case 9: {
                FAIL_PARSE_IF(table.filename, raw_ghs_node.params[0]);
                FAIL_PARSE_IF(table.filename, current_ghs.special_mode != mco_GhmToGhsInfo::SpecialMode::None);
                switch (raw_ghs_node.params[1]) {
                    case 2: { current_ghs.special_mode = mco_GhmToGhsInfo::SpecialMode::Diabetes2; } break;
                    case 3: { current_ghs.special_mode = mco_GhmToGhsInfo::SpecialMode::Diabetes3; } break;
                    default: { FAIL_PARSE_IF(table.filename, true); } break;
                }
            } break;

            case 10: {
                FAIL_PARSE_IF(table.filename, raw_ghs_node.params[0]);
                FAIL_PARSE_IF(table.filename, current_ghs.special_mode != mco_GhmToGhsInfo::SpecialMode::None);
                switch (raw_ghs_node.params[1]) {
                    case 1: { current_ghs.special_mode = mco_GhmToGhsInfo::SpecialMode::Outpatient; } break;
                    case 2: { current_ghs.special_mode = mco_GhmToGhsInfo::SpecialMode::Intermediary; } break;
                    default: { FAIL_PARSE_IF(table.filename, true); } break;
                }
            } break;

            default: {
                FAIL_PARSE_IF(table.filename, true);
            } break;
        }

        if (raw_ghs_node.valid_ghs) {
            for (Size j = 0; j < K_LEN(current_ghs.ghs); j++) {
                current_ghs.ghs[j].number = (int16_t)raw_ghs_node.sectors[j].ghs_code;
            }
            out_ghs->Append(current_ghs);
            current_ghs = {};
        }
    }

    std::stable_sort(out_ghs->begin() + start_ghs_len, out_ghs->end(),
                     [](const mco_GhmToGhsInfo &ghs_info1, const mco_GhmToGhsInfo &ghs_info2) {
        int root_cmp = MultiCmp(ghs_info1.ghm.parts.cmd - ghs_info2.ghm.parts.cmd,
                                ghs_info1.ghm.parts.type - ghs_info2.ghm.parts.type,
                                ghs_info1.ghm.parts.seq - ghs_info2.ghm.parts.seq);
        if (root_cmp) {
            return root_cmp < 0;
        } else if (ghs_info1.ghm.parts.mode >= 'J' && ghs_info2.ghm.parts.mode < 'J') {
            return true;
        } else if (ghs_info2.ghm.parts.mode >= 'J' && ghs_info1.ghm.parts.mode < 'J') {
            return false;
        } else {
            return ghs_info1.ghm.parts.mode < ghs_info2.ghm.parts.mode;
        }
    });

    out_ghs_guard.Disable();
    return true;
}

static bool ParseAuthorizationTable(const uint8_t *file_data, const mco_TableInfo &table,
                                    HeapArray<mco_AuthorizationInfo> *out_auths)
{
    K_DEFER_NC(out_auths_guard, len = out_auths->len) { out_auths->RemoveFrom(len); };

#pragma pack(push, 1)
    struct PackedAuthorization {
        uint8_t code;
        uint8_t function;
        uint8_t global;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, table.sections.len != 2);
    FAIL_PARSE_IF(table.filename, table.sections[0].value_len != 3 ||
                                  table.sections[0].value_len != 3);

    for (Size i = 0; i < 2; i++) {
        for (Size j = 0; j < table.sections[i].values_count; j++) {
            mco_AuthorizationInfo auth = {};

            PackedAuthorization raw_auth;
            MemCpy(&raw_auth, file_data + table.sections[i].raw_offset +
                                          j * K_SIZE(PackedAuthorization), K_SIZE(PackedAuthorization));

            if (i == 0) {
                auth.type.st.scope = mco_AuthorizationScope::Bed;
            } else if (!raw_auth.global) {
                auth.type.st.scope = mco_AuthorizationScope::Unit;
            } else {
                auth.type.st.scope = mco_AuthorizationScope::Facility;
            }
            auth.type.st.code = (int8_t)raw_auth.code;
            auth.function = (int8_t)raw_auth.function;

            out_auths->Append(auth);
        }
    }

    out_auths_guard.Disable();
    return true;
}

static bool ParseSrcPairTable(const uint8_t *file_data, const mco_TableInfo &table,
                              int section_idx, HeapArray<mco_SrcPair> *out_pairs)
{
    Size start_len = out_pairs->len;
    K_DEFER_N(out_pairs_guard) { out_pairs->RemoveFrom(start_len); };

#pragma pack(push, 1)
    struct PackedPair {
        uint16_t diag_code123;
        uint16_t diag_code456;
        uint16_t proc_code123;
        uint16_t proc_code456;
	};
#pragma pack(pop)

    FAIL_PARSE_IF(table.filename, section_idx >= table.sections.len);
    FAIL_PARSE_IF(table.filename, table.sections[section_idx].value_len != K_SIZE(PackedPair));

    for (Size i = 0; i < table.sections[section_idx].values_count; i++) {
        mco_SrcPair pair = {};

        PackedPair raw_pair;
        MemCpy(&raw_pair, file_data + table.sections[section_idx].raw_offset +
                                      i * K_SIZE(PackedPair), K_SIZE(PackedPair));
        raw_pair.diag_code123 = BigEndian(raw_pair.diag_code123);
        raw_pair.diag_code456 = BigEndian(raw_pair.diag_code456);
        raw_pair.proc_code123 = BigEndian(raw_pair.proc_code123);
        raw_pair.proc_code456 = BigEndian(raw_pair.proc_code456);

        pair.diag = ConvertDiagnosisCode((int16_t)raw_pair.diag_code123, raw_pair.diag_code456);
        {
            uint16_t code123_remain = raw_pair.proc_code123;
            for (int j = 0; j < 3; j++) {
                pair.proc.str[2 - j] = (char)((code123_remain % 26) + 65);
                code123_remain /= 26;
            }
            snprintf(pair.proc.str + 3, K_SIZE(pair.proc.str) - 3, "%c%03u",
                     (raw_pair.proc_code456 / 1000 % 26) + 65,
                     raw_pair.proc_code456 % 1000);
        }

        out_pairs->Append(pair);
    }

    std::sort(out_pairs->begin() + start_len, out_pairs->end(),
              [](const mco_SrcPair &pair1, const mco_SrcPair &pair2) {
        return pair1.diag < pair2.diag;
    });

    out_pairs_guard.Disable();
    return true;
}

static bool ParseGhsMinorationTable(const uint8_t *file_data, const mco_TableInfo &table,
                                    HeapArray<mco_GhsCode> *out_minored_ghs)
{
    K_DEFER_NC(out_guard, len = out_minored_ghs->len) { out_minored_ghs->RemoveFrom(len); };

    FAIL_PARSE_IF(table.filename, table.sections.len != 1);
    FAIL_PARSE_IF(table.filename, table.sections[0].value_len != K_SIZE(int16_t));

    for (Size i = 0; i < table.sections[0].values_count; i++) {
        mco_GhsCode ghs = {};

        uint16_t raw_ghs;
        MemCpy(&raw_ghs, file_data + table.sections[0].raw_offset +
                                     i * K_SIZE(uint16_t), K_SIZE(uint16_t));
        ghs.number = BigEndian(raw_ghs);

        out_minored_ghs->Append(ghs);
    }

    out_guard.Disable();
    return true;
}

static bool ParsePriceTable(Span<const uint8_t> file_data, const mco_TableInfo &table,
                            double *out_ghs_coefficient,
                            HeapArray<mco_GhsPriceInfo> *out_ghs_prices,
                            mco_SupplementCounters<int32_t> *out_supplement_prices)
{
    K_DEFER_NC(out_guard, len = out_ghs_prices->len) { out_ghs_prices->RemoveFrom(len); };
    mco_SupplementCounters<int32_t> supplement_prices = {};

    double ghs_coefficient = 0.0;
    {
        StreamReader st(file_data, table.filename);
        IniParser ini(&st);
        bool valid = true;

        ini.PushLogFilter();
        K_DEFER { PopLogFilter(); };

        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                if (prop.key == "GhsCoefficient") {
                    char *end_ptr;
                    ghs_coefficient = strtod(prop.value.ptr, &end_ptr);
                    if (end_ptr == prop.value.ptr || end_ptr[0] ||
                            ghs_coefficient < 0.0 || ghs_coefficient > 1.0) {
                        LogError("Invalid GHS coefficient value %1", ghs_coefficient);
                        valid = false;
                    }
                }

                // Other attributes were used by earlier code to get information
                // about the period, don't error out for unknown attributes!
            } else if (prop.section == "Supplements") {
                do {
                    if (prop.key == "REA") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.rea);
                    } else if (prop.key == "STF") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.reasi);
                        supplement_prices.st.si = supplement_prices.st.reasi;
                    } else if (prop.key == "SRC") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.src);
                    } else if (prop.key == "NN1") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.nn1);
                    } else if (prop.key == "NN2") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.nn2);
                    } else if (prop.key == "NN3") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.nn3);
                    } else if (prop.key == "REP") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.rep);
                    } else if (prop.key == "ANT") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.ant);
                    } else if (prop.key == "RAP") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.rap);
                    } else if (prop.key == "SDC") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.sdc);
                    } else if (prop.key == "DIP") {
                        valid &= ParseInt(prop.value, &supplement_prices.st.dip);
                    } else if (prop.key == "TDE" || prop.key == "TSE") {
                        // Unsupported (for now)
                    } else {
                        LogError("Unknown supplement '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else {
                mco_GhsPriceInfo price_info = {};

                price_info.ghs = mco_GhsCode::Parse(prop.section);
                valid &= price_info.ghs.IsValid();

                do {
                    if (prop.key == "PriceCents") {
                        valid &= ParseInt(prop.value, &price_info.ghs_cents);
                    } else if (prop.key == "ExbThreshold" || prop.key == "ExbTreshold") {
                        valid &= ParseInt(prop.value, &price_info.exb_threshold);
                    } else if (prop.key == "ExbCents") {
                        valid &= ParseInt(prop.value, &price_info.exb_cents);
                    } else if (prop.key == "ExbType") {
                        if (prop.value == "Daily") {
                            price_info.flags &= (uint16_t)~(int)mco_GhsPriceInfo::Flag::ExbOnce;
                        } else if (prop.value == "Once") {
                            price_info.flags |= (uint16_t)mco_GhsPriceInfo::Flag::ExbOnce;
                        } else {
                            LogError("Invalid ExbType value '%1'", prop.value);
                            valid = false;
                        }
                    } else if (prop.key == "ExhThreshold" || prop.key == "ExhTreshold") {
                        valid &= ParseInt(prop.value, &price_info.exh_threshold);
                    } else if (prop.key == "ExhCents") {
                        valid &= ParseInt(prop.value, &price_info.exh_cents);
                    } else {
                        LogError("Unknown GHS price attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));

                if (!price_info.ghs_cents ||
                        (!price_info.exb_threshold != !price_info.exb_cents) ||
                        (!price_info.exh_threshold != !price_info.exh_cents)) {
                    LogError("Missing GHS price attributes");
                    valid = false;
                }

                // Special supplements
                if (price_info.ghs == mco_GhsCode(9614)) {
                    supplement_prices.st.ohb = price_info.ghs_cents;
                } else if (price_info.ghs == mco_GhsCode(9615)) {
                    supplement_prices.st.aph = price_info.ghs_cents;
                } else if (price_info.ghs == mco_GhsCode(9605)) {
                    supplement_prices.st.dia = price_info.ghs_cents;
                } else if (price_info.ghs == mco_GhsCode(20020)) {
                    supplement_prices.st.ent1 = price_info.ghs_cents;
                } else if (price_info.ghs == mco_GhsCode(20021)) {
                    supplement_prices.st.ent2 = price_info.ghs_cents;
                } else if (price_info.ghs == mco_GhsCode(20024)) {
                    supplement_prices.st.ent3 = price_info.ghs_cents;
                }

                out_ghs_prices->Append(price_info);
            }
        }
        if (!ini.IsValid() || !valid)
            return false;

        if (ghs_coefficient == 0.0) {
            LogError("GhsCoefficient is not set or equal to 0");
        }
    }

    out_guard.Disable();
    *out_ghs_coefficient = ghs_coefficient;
    *out_supplement_prices = supplement_prices;
    return true;
}

const mco_TableIndex *mco_TableSet::FindIndex(LocalDate date, bool valid_only) const
{
    for (Size i = indexes.len - 1; i >= 0; i--) {
        if (date.value && (date < indexes[i].limit_dates[0] || date >= indexes[i].limit_dates[1]))
            continue;
        if (valid_only && !indexes[i].valid)
            continue;

        return &indexes[i];
    }

    return nullptr;
}

bool mco_TableSetBuilder::LoadTab(StreamReader *st)
{
    HeapArray<uint8_t> raw_buf(&file_alloc);
    if (st->ReadAll(Megabytes(8), &raw_buf) < 0)
        return false;

    Size start_len = set.tables.len;
    if (!ParseTableHeaders(raw_buf, st->GetFileName(), &set.str_alloc, &set.tables))
        return false;

    Span<uint8_t> raw_data = raw_buf.Leak();
    for (Size i = start_len; i < set.tables.len; i++) {
        if (set.tables[i].type == mco_TableType::UnknownTable)
            continue;

        TableLoadInfo load_info;
        load_info.table_idx = i;
        load_info.raw_data = raw_data;
        load_info.prev_index_idx = -1;
        table_loads.Append(load_info);
    }

    return true;
}

bool mco_TableSetBuilder::LoadPrices(StreamReader *st)
{
    HeapArray<uint8_t> raw_buf(&file_alloc);
    if (st->ReadAll(Megabytes(2), &raw_buf) < 0)
        return false;

    mco_TableInfo table_info = {};
    TableLoadInfo load_info = {};
    {
        StreamReader mem_st(raw_buf, st->GetFileName());
        IniParser ini(&mem_st);

        ini.PushLogFilter();
        K_DEFER { PopLogFilter(); };

        IniProperty prop;
        bool valid = true;
        while (ini.Next(&prop) && !prop.section.len) {
            if (prop.key == "Date") {
                valid &= ParseDate(prop.value, &table_info.limit_dates[0]);
            } else if (prop.key == "End") {
                valid &= ParseDate(prop.value, &table_info.limit_dates[1]);
            } else if (prop.key == "Build") {
                valid &= ParseDate(prop.value, &table_info.build_date);
            } else if (prop.key == "Sector") {
                if (prop.value == "Public") {
                    table_info.type = mco_TableType::PriceTablePublic;
                    CopyString("PRICEPUB", table_info.raw_type);
                } else if (prop.value == "Private") {
                    table_info.type = mco_TableType::PriceTablePrivate;
                    CopyString("PRICEPRI", table_info.raw_type);
                } else {
                    LogError("Unknown sector type '%1'", prop.value);
                    valid = false;
                }
            }
        }
        if (!ini.IsValid() || !valid)
            return false;

        if (!table_info.limit_dates[0].value || !(int)table_info.type) {
            LogError("Missing mandatory header attributes");
            return false;
        }
        if (!table_info.limit_dates[1].value) {
            table_info.limit_dates[1] =
                LocalDate((int16_t)(table_info.limit_dates[0].st.year + 1), 3, 1);
        }
    }

    load_info.table_idx = set.tables.len;
    load_info.raw_data = raw_buf.Leak();
    load_info.prev_index_idx = -1;
    table_loads.Append(load_info);

    table_info.filename = DuplicateString(st->GetFileName(), &set.str_alloc).ptr;
    set.tables.Append(table_info);

    return true;
}

bool mco_TableSetBuilder::LoadFiles(Span<const char *const> filenames)
{
    bool success = true;

    for (const char *filename: filenames) {
        Span<const char> extension = GetPathExtension(filename);

        bool (mco_TableSetBuilder::*load_func)(StreamReader *st);
        if (extension == ".tab") {
            load_func = &mco_TableSetBuilder::LoadTab;
        } else if (extension == ".dpri") {
            load_func = &mco_TableSetBuilder::LoadPrices;
        } else {
            LogError("Cannot load table file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename);
        if (!st.IsValid()) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(&st);
    }

    return success;
}

void mco_TableSetBuilder::Finish(mco_TableSet *out_set)
{
    std::sort(table_loads.begin(), table_loads.end(),
              [&](const TableLoadInfo &tab_load_info1, const TableLoadInfo &tab_load_info2) {
        const mco_TableInfo &table_info1 = set.tables[tab_load_info1.table_idx];
        const mco_TableInfo &table_info2 = set.tables[tab_load_info2.table_idx];

        return MultiCmp(table_info1.limit_dates[0] - table_info2.limit_dates[0],
                        table_info1.version[0] - table_info2.version[0],
                        table_info1.version[1] - table_info2.version[1],
                        table_info1.build_date - table_info2.build_date) < 0;
    });

    TableLoadInfo *active_tables[K_LEN(mco_TableTypeNames)];
    Size active_count = 0;

    TableLoadInfo dummy_loads[K_LEN(mco_TableTypeNames)];
    for (Size i = 0; i < K_LEN(active_tables); i++) {
        dummy_loads[i].table_idx = -1;
        dummy_loads[i].prev_index_idx = -1;
        active_tables[i] = &dummy_loads[i];
    }

    LocalDate start_date = {};
    LocalDate end_date = {};
    for (TableLoadInfo &load_info: table_loads) {
        const mco_TableInfo &table_info = set.tables[load_info.table_idx];

        while (end_date.value && table_info.limit_dates[0] >= end_date) {
            CommitIndex(start_date, end_date, active_tables);

            start_date = {};
            LocalDate next_end_date = {};
            for (Size i = 0; i < K_LEN(active_tables); i++) {
                if (active_tables[i]->table_idx < 0)
                    continue;

                const mco_TableInfo &active_info = set.tables[active_tables[i]->table_idx];

                if (active_info.limit_dates[1] == end_date) {
                    active_tables[i] = &dummy_loads[i];
                    active_count--;
                } else if (!next_end_date.value || active_info.limit_dates[1] < next_end_date) {
                    next_end_date = active_info.limit_dates[1];
                }
            }

            start_date = table_info.limit_dates[0];
            end_date = next_end_date;
        }

        if (start_date.value) {
            if (table_info.limit_dates[0] > start_date) {
                CommitIndex(start_date, table_info.limit_dates[0], active_tables);
                start_date = table_info.limit_dates[0];
            }
        } else {
            start_date = table_info.limit_dates[0];
        }
        if (!end_date.value || table_info.limit_dates[1] < end_date) {
            end_date = table_info.limit_dates[1];
        }

        active_tables[(int)table_info.type] = &load_info;
        active_count++;
    }
    if (active_count) {
        CommitIndex(start_date, end_date, active_tables);
    }

    std::swap(*out_set, set);
}

static void BuildAdditionLists(const mco_TableIndex &index,
                               Span<const ProcedureAdditionInfo> additions,
                               HeapArray<mco_ProcedureLink> *out_links)
{
    int16_t next_addition_idx = 1;
    for (const ProcedureAdditionInfo &addition_info: additions) {
        int16_t addition_idx = 0;
        if (addition_info.activity2 >= 0 &&
                addition_info.activity2 < K_LEN(mco_ProcedureInfo::additions)) [[likely]] {
            mco_ProcedureInfo *proc_info =
                (mco_ProcedureInfo *)index.procedures_map->FindValue(addition_info.proc2, nullptr);

            if (proc_info) [[likely]] {
                bool new_match = false;
                do {
                    if (proc_info->phase == addition_info.phase2) {
                        if (!proc_info->additions[addition_info.activity2]) {
                            proc_info->additions[addition_info.activity2] = next_addition_idx;
                            new_match = true;
                        }
                        addition_idx = proc_info->additions[addition_info.activity2];
                    }
                } while (++proc_info < index.procedures.end() &&
                         proc_info->proc == addition_info.proc2);

                next_addition_idx += new_match;
            }
        }

        if (addition_idx) {
            mco_ProcedureInfo *proc_info =
                (mco_ProcedureInfo *)index.procedures_map->FindValue(addition_info.proc1, nullptr);

            if (proc_info) [[likely]] {
                bool match = false;
                int16_t offset = (int16_t)out_links->len;
                do {
                    if (proc_info->phase == addition_info.phase1) {
                        if (!proc_info->addition_list.len) {
                            proc_info->addition_list.offset = offset;
                        }
                        proc_info->addition_list.len++;

                        match = true;
                    }
                } while (++proc_info < index.procedures.end() &&
                         proc_info->proc == addition_info.proc1);

                if (match) {
                    mco_ProcedureLink link = {};
                    link.proc = addition_info.proc1;
                    link.phase = addition_info.phase1;
                    link.activity = addition_info.activity1;
                    link.addition_idx = addition_idx;
                    out_links->Append(link);
                }
            }
        }
    }
}

bool mco_TableSetBuilder::CommitIndex(LocalDate start_date, LocalDate end_date,
                                      mco_TableSetBuilder::TableLoadInfo *current_tables[])
{
    mco_TableIndex index = {};

    index.limit_dates[0] = start_date;
    index.limit_dates[1] = end_date;
    index.valid = true;

    // Some tables are used to modify existing tables (e.g. procedure extensions from
    // ccamdesc.tab are added to the ProcedureInfo table). Two consequences:
    // - when we load a new main table, we need to reload secondary tables,
    // - when we load a new secondary table, we need to make a new version of the main table.
    static const std::pair<mco_TableType, mco_TableType> TableDependencies[] = {
        { mco_TableType::ProcedureTable, mco_TableType::ProcedureAdditionTable },
        { mco_TableType::ProcedureTable, mco_TableType::ProcedureExtensionTable },
        { mco_TableType::PriceTablePublic, mco_TableType::GhsMinorationTable },
        { mco_TableType::PriceTablePrivate, mco_TableType::GhsMinorationTable }
    };
    HandleDependencies(current_tables, TableDependencies);

#define LOAD_TABLE(MemberName, LoadFunc, ...) \
        do { \
            if (load_info->prev_index_idx < 0) { \
                auto array = set.store.MemberName.AppendDefault(); \
                if (table_info) { \
                    valid &= LoadFunc(__VA_ARGS__, array); \
                } \
                index.MemberName = *array; \
            } else { \
                index.MemberName = set.indexes[load_info->prev_index_idx].MemberName; \
            } \
        } while (false)
#define BUILD_MAP(IndexName, MapPtrName, MapName) \
        do { \
            if (load_info->prev_index_idx < 0) { \
                auto map = set.maps.MapName.AppendDefault(); \
                for (auto &value: index.IndexName) { \
                    map->TrySet(&value); \
                } \
                index.MapPtrName = map; \
            } else { \
                index.MapPtrName = set.indexes[load_info->prev_index_idx].MapPtrName; \
            } \
        } while (false)

    // Load tables and build index
    for (Size i = 0; i < K_LEN(index.tables); i++) {
        bool valid = true;

        TableLoadInfo *load_info = current_tables[i];
        const mco_TableInfo *table_info = (load_info->table_idx >= 0) ?
                                          &set.tables[load_info->table_idx] : nullptr;

        switch ((mco_TableType)i) {
            case mco_TableType::GhmDecisionTree: {
                LOAD_TABLE(ghm_nodes, ParseGhmDecisionTree,
                           load_info->raw_data.ptr, *table_info);
            } break;

            case mco_TableType::DiagnosisTable: {
                LOAD_TABLE(diagnoses, ParseDiagnosisTable,
                           load_info->raw_data.ptr, *table_info);
                LOAD_TABLE(exclusions, ParseExclusionTable,
                           load_info->raw_data.ptr, *table_info);
                BUILD_MAP(diagnoses, diagnoses_map, diagnoses);
            } break;

            case mco_TableType::ProcedureTable: {
                LOAD_TABLE(procedures, ParseProcedureTable,
                           load_info->raw_data.ptr, *table_info);
                BUILD_MAP(procedures, procedures_map, procedures);
            } break;
            case mco_TableType::ProcedureAdditionTable: {
                static_assert((int)mco_TableType::ProcedureAdditionTable > (int)mco_TableType::ProcedureTable);

                if (load_info->prev_index_idx < 0) {
                    HeapArray<mco_ProcedureLink> *links = set.store.procedure_links.AppendDefault();

                    if (table_info) {
                        HeapArray<ProcedureAdditionInfo> additions;
                        valid &= ParseProcedureAdditionTable(load_info->raw_data.ptr, *table_info,
                                                             &additions);

                        // Probably redundant, but make sure for BuildAdditionLists()
                        std::sort(additions.begin(), additions.end(),
                                  [](const ProcedureAdditionInfo &addition_info1,
                                     const ProcedureAdditionInfo &addition_info2) {
                            return MultiCmp(addition_info1.proc1.value - addition_info2.proc1.value,
                                            addition_info1.phase1 - addition_info2.phase1) < 0;
                        });

                        BuildAdditionLists(index, additions, links);
                    }

                    index.procedure_links = *links;
                } else {
                    index.procedure_links = set.indexes[load_info->prev_index_idx].procedure_links;
                }
            } break;
            case mco_TableType::ProcedureExtensionTable: {
                static_assert((int)mco_TableType::ProcedureExtensionTable > (int)mco_TableType::ProcedureTable);

                if (table_info && load_info->prev_index_idx < 0) {
                    HeapArray<ProcedureExtensionInfo> extensions;
                    valid &= ParseProcedureExtensionTable(load_info->raw_data.ptr,
                                                          *table_info, &extensions);

                    for (const ProcedureExtensionInfo &ext_info: extensions) {
                        if (ext_info.extension >= K_SIZE(mco_ProcedureInfo::extensions) * 8) {
                            LogError("Procedure %1 extension value %2 > %3 cannot be used",
                                     ext_info.proc, ext_info.extension,
                                     K_SIZE(mco_ProcedureInfo::extensions) * 8 - 1);
                            continue;
                        }

                        mco_ProcedureInfo *proc_info =
                            (mco_ProcedureInfo *)index.procedures_map->FindValue(ext_info.proc, nullptr);
                        if (proc_info) [[likely]] {
                            do {
                                if (proc_info->phase == ext_info.phase) {
                                    proc_info->extensions |= 1ull << ext_info.extension;

                                    // XXX: Starting with FG 2020, extension codes have
                                    // validity dates... and we need to properly support them!
                                    // This does the job for almost all codes for now (except for ZZQX173).
                                    if (ext_info.limit_dates[1] < LocalDate(2020, 3, 1)) {
                                        proc_info->disabled_extensions |= 1ull << ext_info.extension;
                                    }
                                }
                            } while (++proc_info < index.procedures.end() &&
                                     proc_info->proc == ext_info.proc);
                        }
                    }
                }
            } break;

            case mco_TableType::GhmRootTable: {
                LOAD_TABLE(ghm_roots, ParseGhmRootTable,
                           load_info->raw_data.ptr, *table_info);
                BUILD_MAP(ghm_roots, ghm_roots_map, ghm_roots);
            } break;

            case mco_TableType::SeverityTable: {
                LOAD_TABLE(gnn_cells, ParseSeverityTable,
                           load_info->raw_data.ptr, *table_info, 0);
                LOAD_TABLE(cma_cells[0], ParseSeverityTable,
                           load_info->raw_data.ptr, *table_info, 1);
                LOAD_TABLE(cma_cells[1], ParseSeverityTable,
                           load_info->raw_data.ptr, *table_info, 2);
                LOAD_TABLE(cma_cells[2], ParseSeverityTable,
                           load_info->raw_data.ptr, *table_info, 3);
            } break;

            case mco_TableType::GhmToGhsTable: {
                LOAD_TABLE(ghs, ParseGhmToGhsTable, load_info->raw_data.ptr, *table_info);
                BUILD_MAP(ghs, ghm_to_ghs_map, ghm_to_ghs);
                BUILD_MAP(ghs, ghm_root_to_ghs_map, ghm_root_to_ghs);
            } break;

            case mco_TableType::AuthorizationTable: {
                LOAD_TABLE(authorizations, ParseAuthorizationTable,
                           load_info->raw_data.ptr, *table_info);
                BUILD_MAP(authorizations, authorizations_map, authorizations);
            } break;

            case mco_TableType::SrcPairTable: {
                LOAD_TABLE(src_pairs[0], ParseSrcPairTable,
                           load_info->raw_data.ptr, *table_info, 0);
                LOAD_TABLE(src_pairs[1], ParseSrcPairTable,
                           load_info->raw_data.ptr, *table_info, 1);
                BUILD_MAP(src_pairs[0], src_pairs_map[0], src_pairs);
                BUILD_MAP(src_pairs[1], src_pairs_map[1], src_pairs);
            } break;

            case mco_TableType::PriceTablePublic:
            case mco_TableType::PriceTablePrivate: {
                Size table_idx = i - (int)mco_TableType::PriceTablePublic;

                if (table_info) {
                    if (load_info->prev_index_idx < 0) {
                        auto array = set.store.ghs_prices[table_idx].AppendDefault();
                        valid &= ParsePriceTable(load_info->raw_data, *table_info,
                                                 &index.ghs_coefficient[table_idx], array,
                                                 &index.supplement_prices[table_idx]);
                        index.ghs_prices[table_idx] = *array;
                    } else {
                        index.ghs_coefficient[table_idx] = set.indexes[load_info->prev_index_idx].ghs_coefficient[table_idx];
                        index.ghs_prices[table_idx] = set.indexes[load_info->prev_index_idx].ghs_prices[table_idx];
                        index.supplement_prices[table_idx] = set.indexes[load_info->prev_index_idx].supplement_prices[table_idx];
                    }
                }

                BUILD_MAP(ghs_prices[table_idx], ghs_prices_map[table_idx], ghs_prices[table_idx]);
            } break;

            case mco_TableType::GhsMinorationTable: {
                if (table_info) {
                    HeapArray<mco_GhsCode> minored_ghs;
                    valid &= ParseGhsMinorationTable(load_info->raw_data.ptr, *table_info,
                                                     &minored_ghs);

                    for (int j = 0; j < 2; j++) {
                        for (mco_GhsCode ghs: minored_ghs) {
                            mco_GhsPriceInfo *price_info =
                                (mco_GhsPriceInfo *)index.ghs_prices_map[j]->FindValue(ghs, nullptr);
                            if (price_info) [[likely]] {
                                price_info->flags |= (int)mco_GhsPriceInfo::Flag::Minoration;
                            }
                        }
                    }
                }
            } break;

            case mco_TableType::UnknownTable: {} break;
        }

        if (valid) {
            index.tables[i] = table_info;
        }
        if (!set.indexes.len || load_info->prev_index_idx != set.indexes.len - 1) {
            index.changed_tables |= (uint32_t)(1 << i);
        }
        load_info->prev_index_idx = set.indexes.len;

        index.valid &= valid;
    }

    // Check index validity
    // XXX: Validate all tables (some were not always needed)
    index.valid &= index.ghm_nodes.len &&
                   index.diagnoses.len &&
                   index.procedures.len &&
                   index.ghm_roots.len &&
                   index.ghs.len &&
                   index.ghs_prices[0].len &&
                   index.ghs_prices[1].len;
    if (!index.valid) {
        LogDebug("Missing pieces for index: %1 to %2", start_date, end_date);
    }

#undef BUILD_MAP
#undef LOAD_TABLE

    set.indexes.Append(index);
    return index.valid;
}

void mco_TableSetBuilder::HandleDependencies(mco_TableSetBuilder::TableLoadInfo *current_tables[],
                                             Span<const std::pair<mco_TableType, mco_TableType>> pairs)
{
    for (const std::pair<mco_TableType, mco_TableType> &p: pairs) {
        mco_TableSetBuilder::TableLoadInfo *main_table = current_tables[(Size)p.first];
        mco_TableSetBuilder::TableLoadInfo *secondary_table = current_tables[(Size)p.second];

        if (secondary_table->table_idx >= 0 && secondary_table->prev_index_idx < 0) {
            main_table->prev_index_idx = -1;
        }
    }

    for (const std::pair<mco_TableType, mco_TableType> &p: pairs) {
        mco_TableSetBuilder::TableLoadInfo *main_table = current_tables[(Size)p.first];
        mco_TableSetBuilder::TableLoadInfo *secondary_table = current_tables[(Size)p.second];

        if (main_table->prev_index_idx < 0) {
            secondary_table->prev_index_idx = -1;
        }
    }
}

bool mco_LoadTableSet(Span<const char *const> table_directories,
                      Span<const char *const> table_filenames,
                      mco_TableSet *out_set)
{
    static const char *const directory_names[] = {
        "mco",
        "mco_tables"
    };

    BlockAllocator temp_alloc;

    HeapArray<const char *> filenames;
    {
        const auto enumerate_directory_files = [&](const char *dir) {
            EnumResult ret = EnumerateDirectory(dir, nullptr, 1024,
                                                [&](const char *basename, FileType file_type) {
                const char *filename = Fmt(&temp_alloc, "%1%/%2", dir, basename).ptr;

                CompressionType compression_type;
                Span<const char> ext = GetPathExtension(basename, &compression_type);

                if (file_type == FileType::Link) {
                    FileInfo file_info;
                    if (StatFile(filename, (int)StatFlag::FollowSymlink, &file_info) != StatResult::Success)
                        return true;
                    file_type = file_info.type;
                }

                if (file_type == FileType::File && (ext == ".tab" || ext == ".dpri")) {
                    filenames.Append(filename);
                }

                return true;
            });

            bool success = (ret == EnumResult::Success || ret == EnumResult::PartialEnum);
            return success;
        };

        bool success = true;
        for (const char *table_dir: table_directories) {
            for (const char *dir_name: directory_names) {
                const char *mco_dir = Fmt(&temp_alloc, "%1%/%2", table_dir, dir_name).ptr;
                if (TestFile(mco_dir, FileType::Directory)) {
                    success &= enumerate_directory_files(mco_dir);
                }
            }
        }
        filenames.Append(table_filenames);
        if (!success)
            return false;
    }

    if (!filenames.len) {
        LogError("No table specified or found");
    }

    // Load tables
    {
        mco_TableSetBuilder table_set_builder;
        if (!table_set_builder.LoadFiles(filenames))
            return false;
        table_set_builder.Finish(out_set);
    }

    return true;
}

template <typename T, typename U, typename Handler>
Span<const T> FindSpan(Span<const T> arr, const HashTable<U, const T *, Handler> *map, U code)
{
    Span<const T> ret = {};

    ret.ptr = map->FindValue(code, nullptr);
    if (ret.ptr) {
        const T *end_it = ret.ptr + 1;
        while (end_it < arr.end() && Handler::TestKeys(Handler::GetKey(end_it), code)) {
            end_it++;
        }
        ret.len = end_it - ret.ptr;
    }

    return ret;
}

Span<const mco_DiagnosisInfo> mco_TableIndex::FindDiagnosis(drd_DiagnosisCode diag) const
{
    return FindSpan(diagnoses, diagnoses_map, diag);
}

const mco_DiagnosisInfo *mco_TableIndex::FindDiagnosis(drd_DiagnosisCode diag, int sex) const
{
    const mco_DiagnosisInfo *diag_info = diagnoses_map->FindValue(diag, nullptr);

    if (diag_info) {
        if (diag_info->sexes & sex)
            return diag_info;
        if (++diag_info < diagnoses.end() && diag_info->sexes & sex)
            return diag_info;
    }

    return nullptr;
}

Span<const mco_ProcedureInfo> mco_TableIndex::FindProcedure(drd_ProcedureCode proc) const
{
    return FindSpan(procedures, procedures_map, proc);
}

const mco_ProcedureInfo *mco_TableIndex::FindProcedure(drd_ProcedureCode proc, int8_t phase, LocalDate date) const
{
    const mco_ProcedureInfo *proc_info = procedures_map->FindValue(proc, nullptr);

    if (proc_info) {
        do {
            if (proc_info->phase != phase)
                continue;
            if (date < proc_info->limit_dates[0] || date >= proc_info->limit_dates[1])
                continue;

            return proc_info;
        } while (++proc_info < procedures.end() && proc_info->proc == proc);
    }

    return nullptr;
}

const mco_GhmRootInfo *mco_TableIndex::FindGhmRoot(mco_GhmRootCode ghm_root) const
{
    return ghm_roots_map->FindValue(ghm_root, nullptr);
}

Span<const mco_GhmToGhsInfo> mco_TableIndex::FindCompatibleGhs(mco_GhmCode ghm) const
{
    return FindSpan(ghs, ghm_to_ghs_map, ghm);
}

Span<const mco_GhmToGhsInfo> mco_TableIndex::FindCompatibleGhs(mco_GhmRootCode ghm_root) const
{
    return FindSpan(ghs, ghm_root_to_ghs_map, ghm_root);
}

const mco_AuthorizationInfo *mco_TableIndex::FindAuthorization(mco_AuthorizationScope scope, int8_t type) const
{
    decltype(mco_AuthorizationInfo::type) key;
    key.st.scope = scope;
    key.st.code = type;

    return authorizations_map->FindValue(key.value, nullptr);
}

double mco_TableIndex::GhsCoefficient(drd_Sector sector) const
{
    return ghs_coefficient[(int)sector];
}

const mco_GhsPriceInfo *mco_TableIndex::FindGhsPrice(mco_GhsCode ghs, drd_Sector sector) const
{
    return ghs_prices_map[(int)sector]->FindValue(ghs, nullptr);
}

const mco_SupplementCounters<int32_t> &mco_TableIndex::SupplementPrices(drd_Sector sector) const
{
    return supplement_prices[(int)sector];
}

mco_ListSpecifier mco_ListSpecifier::FromString(const char *spec_str)
{
    mco_ListSpecifier spec;

    if (!spec_str[0] || !spec_str[1])
        goto error;

    switch (spec_str[0]) {
        case 'd': case 'D': { spec.table = mco_ListSpecifier::Table::Diagnoses; } break;
        case 'a': case 'A': { spec.table = mco_ListSpecifier::Table::Procedures; } break;

        default:
            goto error;
    }

    switch (spec_str[1]) {
        case '$': {
            const char *mask_str = spec_str + 2;
            if (mask_str[0] == '~') {
                spec.type = mco_ListSpecifier::Type::ReverseMask;
                mask_str++;
            } else {
                spec.type = mco_ListSpecifier::Type::Mask;
            }
            if (sscanf(mask_str, "%" SCNu8 ".%" SCNu8,
                       &spec.u.mask.offset, &spec.u.mask.mask) != 2)
                goto error;
        } break;

        case '-': {
            uint8_t cmd, jump;
            int ret = sscanf(spec_str + 2, "%02" SCNu8 "%02" SCNu8, &cmd, &jump);

            if (ret == 1) {
                spec.type = mco_ListSpecifier::Type::Cmd;
                spec.u.cmd = cmd;
            } else if (ret == 2) {
                spec.type = mco_ListSpecifier::Type::CmdJump;
                spec.u.cmd_jump.cmd = (int8_t)cmd;
                spec.u.cmd_jump.jump = (int8_t)jump;
            } else {
                goto error;
            }
        } break;

        default:
            goto error;
    }

    return spec;

error:
    LogError("Malformed list specifier '%1'", spec_str);
    spec.table = Table::Invalid;
    return spec;
}

#undef FAIL_PARSE_IF

}
