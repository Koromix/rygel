#pragma once

#include "kutil.hh"
#include "data_fg.hh"

struct ClassifierSet {
    Date limit_dates[2];
    const TableInfo *tables[CountOf(TableTypeNames)];

    ArraySlice<GhmDecisionNode> ghm_nodes;
    ArraySlice<DiagnosisInfo> diagnoses;
    ArraySlice<ProcedureInfo> procedures;
    ArraySlice<GhmRootInfo> ghm_roots;
};

struct ClassifierStore {
    DynamicArray<TableInfo> tables;

    DynamicArray<ClassifierSet> sets;

    DynamicArray<GhmDecisionNode> ghm_nodes;
    DynamicArray<DiagnosisInfo> diagnoses;
    DynamicArray<ProcedureInfo> procedures;
    DynamicArray<GhmRootInfo> ghm_roots;
};

bool LoadClassifierFiles(ArrayRef<const char *const> filenames, ClassifierStore *store);
