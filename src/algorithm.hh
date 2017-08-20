#pragma once

#include "kutil.hh"
#include "stays.hh"
#include "tables.hh"

enum class ClusterMode {
    StayModes,
    BillId,
    Disable
};

struct StayAggregate {
    Stay stay;
    int duration;
    int age;
};

struct SummarizeResult {
    ArrayRef<const Stay> cluster;
    const TableIndex *index;
    StayAggregate agg;

    GhmCode ghm;
    ArrayRef<int16_t> errors;
};

struct SummarizeResultSet {
    HeapArray<SummarizeResult> results;

    struct {
        HeapArray<int16_t> errors;
    } store;
};

ArrayRef<const Stay> Cluster(ArrayRef<const Stay> stays, ClusterMode mode,
                             ArrayRef<const Stay> *out_remainder);
GhmCode PrepareIndex(const TableSet &table_set, ArrayRef<const Stay> cluster_stays,
                     const TableIndex **out_index, HeapArray<int16_t> *out_errors);
GhmCode Aggregate(const TableIndex &index, ArrayRef<const Stay> stays,
                  StayAggregate *out_agg,
                  HeapArray<DiagnosisCode> *out_diagnoses, HeapArray<Procedure> *out_procedures,
                  HeapArray<int16_t> *out_errors);
GhmCode Classify(const TableIndex &index, const StayAggregate &agg,
                 ArrayRef<const DiagnosisCode> diagnoses, ArrayRef<const Procedure> procedures,
                 HeapArray<int16_t> *out_errors);

void Summarize(const TableSet &table_set, ArrayRef<const Stay> stays,
               ClusterMode cluster_mode, SummarizeResultSet *out_result_set);
