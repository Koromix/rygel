/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

struct RunGhmTreeContext {
    const TableIndex *index;
    const StayAggregate *agg;

    ArrayRef<const DiagnosisCode> diagnoses;
    ArrayRef<const Procedure> procedures;

    // Keep a copy for DP - DR reversal (function 34)
    DiagnosisCode main_diagnosis;
    DiagnosisCode linked_diagnosis;

    // Lazy values
    struct {
        int gnn;
    } cache;
};

struct SummarizeResult {
    ArrayRef<const Stay> cluster;
    const TableIndex *index;
    StayAggregate agg;

    GhmCode ghm;
    ArrayRef<int16_t> errors;
    GhsCode ghs;
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

int GetMinimalDurationForSeverity(int severity);
int LimitSeverityWithDuration(int severity, int duration);

int ExecuteGhmTest(RunGhmTreeContext &ctx, const GhmDecisionNode &ghm_node,
                   HeapArray<int16_t> *out_errors);
GhmCode RunGhmSeverity(const TableIndex &index, const StayAggregate &agg,
                       ArrayRef<const DiagnosisCode> diagnoses,
                       GhmCode ghm, HeapArray<int16_t> *out_errors);
GhmCode RunGhmTree(const TableIndex &index, const StayAggregate &agg,
                   ArrayRef<const DiagnosisCode> diagnoses,
                   ArrayRef<const Procedure> procedures,
                   HeapArray<int16_t> *out_errors);
GhmCode Classify(const TableIndex &index, const StayAggregate &agg,
                 ArrayRef<const DiagnosisCode> diagnoses, ArrayRef<const Procedure> procedures,
                 HeapArray<int16_t> *out_errors);

GhsCode PickGhs(const TableIndex &index, const AuthorizationSet &authorization_set,
                ArrayRef<const Stay> stays, const StayAggregate &agg,
                ArrayRef<const DiagnosisCode> diagnoses, ArrayRef<const Procedure> procedures);

void Summarize(const TableSet &table_set, const AuthorizationSet &authorization_set,
               ArrayRef<const Stay> stays, ClusterMode cluster_mode,
               SummarizeResultSet *out_result_set);

struct GhmConstraint {
    GhmCode ghm_code;

    uint64_t duration_mask;

    HASH_SET_HANDLER(GhmConstraint, ghm_code);
};

bool ComputeGhmConstraints(const TableIndex &index,
                           HashSet<GhmCode, GhmConstraint> *out_constraints);
