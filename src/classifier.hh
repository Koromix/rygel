#pragma once

#include "kutil.hh"
#include "stays.hh"
#include "tables.hh"

enum class AggregateMode {
    StayModes,
    BillId,
    Disable
};
static const char *const AggregateModeNames[] = {
    "Stay Modes",
    "Bill Id"
};

ArrayRef<const Stay> Aggregate(ArrayRef<const Stay> stays, AggregateMode group_mode,
                               ArrayRef<const Stay> *out_remainder);

struct ClassifyError {
    int16_t code;
    char ref_str[14];
};

struct ClassifyResult {
    GhmCode ghm;
    ArrayRef<ClassifyError> errors;
};

struct ClassifyResultSet {
    HeapArray<ClassifyResult> results;

    struct {
        HeapArray<ClassifyError> errors;
    } store;
};

class Classifier {
    const ClassifierIndex *index;

    ArrayRef<const Stay> stays;
    struct {
        Stay stay;
        int age;
        int duration;
    } agg;

    // Deduplicated diagnoses and procedures
    HeapArray<DiagnosisCode> diagnoses;
    HeapArray<Procedure> procedures;

    // Keep a copy for DP - DR reversal during GHM tree traversal (function 34)
    DiagnosisCode main_diagnosis;
    DiagnosisCode linked_diagnosis;

    struct {
        int gnn = 0;
    } lazy;

public:
    Classifier() = default;

    bool Init(const ClassifierSet &classifier_set, ArrayRef<const Stay> stays);

    GhmCode Run(HeapArray<ClassifyError> *out_errors);

    const Stay *FindMainStay();
    GhmCode RunGhmTree(HeapArray<ClassifyError> *out_errors);
    GhmCode RunGhmSeverity(GhmCode ghm);

    int ExecuteGhmTest(const GhmDecisionNode &ghm_node);

private:
    uint8_t GetDiagnosisByte(DiagnosisCode diag_code, uint8_t byte_idx);
    uint8_t GetProcedureByte(const Procedure &proc, uint8_t byte_idx);
    bool TestExclusion(const DiagnosisInfo &cma_diag_info, const DiagnosisInfo &main_diag_info);
};

bool ClassifyAggregates(const ClassifierSet &classifier_set, ArrayRef<const Stay> stays,
                        AggregateMode agg_mode, ClassifyResultSet *out_result_set);
