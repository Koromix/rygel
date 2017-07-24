#pragma once

#include "kutil.hh"
#include "fg_table.hh"

struct ClassifierSet {
    Date limit_dates[2];
    const TableInfo *tables[CountOf(TableTypeNames)];

    ArraySlice<DiagnosisInfo> diagnoses;
    ArraySlice<ProcedureInfo> procedures;
};

struct ClassifierStore {
    DynamicArray<TableInfo> tables;

    DynamicArray<ClassifierSet> sets;

    DynamicArray<DiagnosisInfo> diagnoses;
    DynamicArray<ProcedureInfo> procedures;
};

bool LoadClassifierFiles(ArrayRef<const char *const> filenames, ClassifierStore *store);
