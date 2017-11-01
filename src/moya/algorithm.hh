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
    ArrayRef<DiagnosisCode> diagnoses;
    ArrayRef<ProcedureRealisation> procedures;

    int age;
    int duration;
};

struct SupplementCounters {
    int16_t rea;
    int16_t reasi;
    int16_t si;
    int16_t src;
    int16_t nn1;
    int16_t nn2;
    int16_t nn3;
    int16_t rep;
};

struct ClassifyResult {
    ArrayRef<const Stay> stays;

    GhmCode ghm;
    ArrayRef<int16_t> errors;

    GhsCode ghs;
    SupplementCounters supplements;
};

struct ClassifyResultSet {
    HeapArray<ClassifyResult> results;

    struct {
        HeapArray<int16_t> errors;
    } store;
};

ArrayRef<const Stay> Cluster(ArrayRef<const Stay> stays, ClusterMode mode,
                             ArrayRef<const Stay> *out_remainder);

GhmCode Aggregate(const TableSet &table_set, ArrayRef<const Stay> stays,
                  ClassifyAggregate *out_agg,
                  HeapArray<DiagnosisCode> *out_diagnoses,
                  HeapArray<ProcedureRealisation> *out_procedures,
                  HeapArray<int16_t> *out_errors);

int GetMinimalDurationForSeverity(int severity);
int LimitSeverityWithDuration(int severity, int duration);

GhmCode RunGhmTree(const ClassifyAggregate &agg, HeapArray<int16_t> *out_errors);
GhmCode RunGhmSeverity(const ClassifyAggregate &agg, GhmCode ghm,
                       HeapArray<int16_t> *out_errors);
GhmCode ClassifyGhm(const ClassifyAggregate &agg, HeapArray<int16_t> *out_errors);

GhsCode ClassifyGhs(const ClassifyAggregate &agg, const AuthorizationSet &authorization_set,
                    GhmCode ghm);
void CountSupplements(const ClassifyAggregate &agg, GhsCode ghs,
                      SupplementCounters *out_counters);

void Classify(const TableSet &table_set, const AuthorizationSet &authorization_set,
              ArrayRef<const Stay> stays, ClusterMode cluster_mode,
              ClassifyResultSet *out_result_set);
