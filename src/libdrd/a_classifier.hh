// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "d_authorizations.hh"
#include "d_stays.hh"
#include "d_tables.hh"

enum class ClusterMode {
    StayModes,
    BillId,
    Disable
};

struct ClassifyAggregate {
    Span<const Stay> stays;

    const TableIndex *index;

    Stay stay;
    Span<DiagnosisCode> diagnoses;
    Span<ProcedureRealisation> procedures;

    int age;
    int duration;
};

struct SupplementCounters {
    int32_t rea = 0;
    int32_t reasi = 0;
    int32_t si = 0;
    int32_t src = 0;
    int32_t nn1 = 0;
    int32_t nn2 = 0;
    int32_t nn3 = 0;
    int32_t rep = 0;
};

struct ClassifyErrorSet {
    int16_t main_error = 0;
    Bitset<512> errors;
};

struct ClassifyResult {
    Span<const Stay> stays;
    int duration;

    GhmCode ghm;
    int16_t main_error;

    GhsCode ghs;
    SupplementCounters supplements;
    int ghs_price_cents;
};

struct ClassifyResultSet {
    HeapArray<ClassifyResult> results;

    SupplementCounters supplements;
    int64_t ghs_total_cents = 0;
};

Span<const Stay> Cluster(Span<const Stay> stays, ClusterMode mode,
                             Span<const Stay> *out_remainder);

GhmCode Aggregate(const TableSet &table_set, Span<const Stay> stays,
                  ClassifyAggregate *out_agg,
                  HeapArray<DiagnosisCode> *out_diagnoses,
                  HeapArray<ProcedureRealisation> *out_procedures,
                  ClassifyErrorSet *out_errors);

int GetMinimalDurationForSeverity(int severity);
int LimitSeverityWithDuration(int severity, int duration);

GhmCode RunGhmTree(const ClassifyAggregate &agg, ClassifyErrorSet *out_errors);
GhmCode RunGhmSeverity(const ClassifyAggregate &agg, GhmCode ghm,
                       ClassifyErrorSet *out_errors);
GhmCode ClassifyGhm(const ClassifyAggregate &agg, ClassifyErrorSet *out_errors);

GhsCode ClassifyGhs(const ClassifyAggregate &agg, const AuthorizationSet &authorization_set,
                    GhmCode ghm);
void CountSupplements(const ClassifyAggregate &agg, GhsCode ghs,
                      SupplementCounters *out_counters);
int PriceGhs(const GhsPriceInfo &price_info, int duration, bool death);
int PriceGhs(const ClassifyAggregate &agg, GhsCode ghs);

void Classify(const TableSet &table_set, const AuthorizationSet &authorization_set,
              Span<const Stay> stays, ClusterMode cluster_mode,
              ClassifyResultSet *out_result_set);
