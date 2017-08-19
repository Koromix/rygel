#pragma once

#include "kutil.hh"
#include "stays.hh"
#include "tables.hh"

enum class ClusterMode {
    StayModes,
    BillId,
    Disable
};
static const char *const ClusterModeNames[] = {
    "Stay Modes",
    "Bill Id"
};

ArrayRef<const Stay> ClusterStays(ArrayRef<const Stay> stays, ClusterMode mode,
                                  ArrayRef<const Stay> *out_remainder);

struct ClassifyResult {
    GhmCode ghm;
    ArrayRef<int16_t> errors;
};

struct ClassifyResultSet {
    HeapArray<ClassifyResult> results;

    struct {
        HeapArray<int16_t> errors;
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

    GhmCode Init(const ClassifierSet &classifier_set, ArrayRef<const Stay> stays,
                 HeapArray<int16_t> *out_errors);
    GhmCode Run(HeapArray<int16_t> *out_errors);

    const Stay *FindMainStay();
    GhmCode RunGhmTree(HeapArray<int16_t> *out_errors);
    GhmCode RunGhmSeverity(GhmCode ghm, HeapArray<int16_t> *out_errors);

    int ExecuteGhmTest(const GhmDecisionNode &ghm_node, HeapArray<int16_t> *out_errors);

private:
    uint8_t GetDiagnosisByte(DiagnosisCode diag_code, uint8_t byte_idx);
    uint8_t GetProcedureByte(const Procedure &proc, uint8_t byte_idx);
    bool TestExclusion(const DiagnosisInfo &cma_diag_info, const DiagnosisInfo &main_diag_info);

    GhmCode AddError(HeapArray<int16_t> *out_errors, int16_t error);
};

GhmCode ClassifyCluster(const ClassifierSet &classifier_set, ArrayRef<const Stay> stays,
                        HeapArray<int16_t> *out_errors);

void Classify(const ClassifierSet &classifier_set, ArrayRef<const Stay> stays,
              ClusterMode cluster_mode, ClassifyResultSet *out_result_set);
