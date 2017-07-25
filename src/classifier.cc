#include "kutil.hh"
#include "classifier.hh"
#include "data_fg.hh"

struct TableData {
    size_t table_idx;
    const char *filename;
    ArrayRef<uint8_t> raw_data;
    bool loaded;
};

static bool CommitClassifierSet(ClassifierStore *store, Date start_date, Date end_sate,
                                TableData *current_tables[])
{
    bool success = true;

    ClassifierSet set = {};

    set.limit_dates[0] = start_date;
    set.limit_dates[1] = end_sate;

#define LOAD_TABLE(Type, MemberName, LoadFunc) \
        case TableType::Type: { \
            if (!table->loaded) { \
                set.MemberName.offset = store->MemberName.len; \
                success &= LoadFunc(table->raw_data.ptr, table->filename, \
                                    table_info, &store->MemberName); \
                set.MemberName.len = store->MemberName.len - set.MemberName.offset; \
            } else { \
                set.MemberName = store->sets[store->sets.len - 1].MemberName; \
            } \
        } break

    size_t active_count = 0;
    for (size_t i = 0; i < CountOf(TableTypeNames); i++) {
        if (!current_tables[i])
            continue;

        TableData *table = current_tables[i];
        const TableInfo &table_info = store->tables[table->table_idx];

        switch ((TableType)i) {
            LOAD_TABLE(GhmDecisionTree, ghm_nodes, ParseGhmDecisionTree);
            LOAD_TABLE(DiagnosisTable, diagnoses, ParseDiagnosisTable);
            LOAD_TABLE(ProcedureTable, procedures, ParseProcedureTable);
            LOAD_TABLE(GhmRootTable, ghm_roots, ParseGhmRootTable);

            case TableType::ChildbirthTable:
            case TableType::GhsDecisionTree:
            case TableType::AuthorizationTable:
            case TableType::DiagnosisProcedureTable:
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

bool LoadClassifierFiles(ArrayRef<const char *const> filenames, ClassifierStore *store)
{
    Assert(!store->tables.len);
    Assert(!store->sets.len);

    bool success = true;

    Allocator file_alloc;

    DynamicArray<TableData> tables;
    for (const char *filename: filenames) {
        ArrayRef<uint8_t> raw_data;
        if (!ReadFile(&file_alloc, filename, Megabytes(8), &raw_data)) {
            success = false;
            continue;
        }

        size_t start_len = store->tables.len;
        if (!ParseTableHeaders(raw_data, filename, &store->tables)) {
            success = false;
            continue;
        }
        for (size_t i = start_len; i < store->tables.len; i++) {
            if (store->tables[i].type == TableType::UnknownTable)
                continue;

            TableData table = {};
            table.table_idx = i;
            table.filename = filename;
            table.raw_data = raw_data;
            tables.Append(table);
        }
    }

    std::sort(tables.begin(), tables.end(),
              [&](const TableData &table1, const TableData &table2) {
        const TableInfo &table_info1 = store->tables[table1.table_idx];
        const TableInfo &table_info2 = store->tables[table2.table_idx];

        if (table_info1.limit_dates[0] < table_info2.limit_dates[0]) {
            return true;
        } else if (table_info1.limit_dates[0] == table_info2.limit_dates[0]) {
            return table_info1.build_date < table_info2.build_date;
        } else {
            return false;
        }
    });

    TableData *active_tables[CountOf(TableTypeNames)] = {};
    Date start_date = {}, end_date = {};
    for (TableData &table: tables) {
        const TableInfo &table_info = store->tables[table.table_idx];

        while (end_date.value && table_info.limit_dates[0] >= end_date) {
            success &= CommitClassifierSet(store, start_date, end_date, active_tables);

            start_date = {};
            Date next_end_date = {};
            for (size_t i = 0; i < CountOf(active_tables); i++) {
                if (!active_tables[i])
                    continue;

                const TableInfo &active_info = store->tables[active_tables[i]->table_idx];

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
                success &= CommitClassifierSet(store, start_date, table_info.limit_dates[0],
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

        // TODO: Warn if identical versions, etc.
    }
    success &= CommitClassifierSet(store, start_date, end_date, active_tables);

    return success;
}
