/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "data.hh"
#include "tables.hh"

enum class ClusterMode {
    StayModes,
    BillId,
    Disable
};

struct ClassifyAggregate {
    ArrayRef<const Stay> stays;

    const TableIndex *index;

    Stay stay;
    HeapArray<DiagnosisCode> diagnoses;
    HeapArray<ProcedureRealisation> procedures;

    int age;
    int duration;
};

struct SummarizeResult {
    ArrayRef<const Stay> stays;

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

GhmCode Aggregate(const TableSet &table_set, ArrayRef<const Stay> stays,
                  ClassifyAggregate *out_agg, HeapArray<int16_t> *out_errors);

int GetMinimalDurationForSeverity(int severity);
int LimitSeverityWithDuration(int severity, int duration);

GhmCode RunGhmTree(const ClassifyAggregate &agg, HeapArray<int16_t> *out_errors);
GhmCode RunGhmSeverity(const ClassifyAggregate &agg, GhmCode ghm,
                       HeapArray<int16_t> *out_errors);
GhmCode ClassifyGhm(const ClassifyAggregate &agg, HeapArray<int16_t> *out_errors);

GhsCode ClassifyGhs(const ClassifyAggregate &agg, const AuthorizationSet &authorization_set,
                    GhmCode ghm);

void Summarize(const TableSet &table_set, const AuthorizationSet &authorization_set,
               ArrayRef<const Stay> stays, ClusterMode cluster_mode,
               SummarizeResultSet *out_result_set);
