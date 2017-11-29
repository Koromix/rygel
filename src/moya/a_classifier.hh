/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "d_authorizations.hh"
#include "d_stays.hh"
#include "d_prices.hh"
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
    int32_t rea;
    int32_t reasi;
    int32_t si;
    int32_t src;
    int32_t nn1;
    int32_t nn2;
    int32_t nn3;
    int32_t rep;
};

struct ClassifyResult {
    Span<const Stay> stays;
    int duration;

    GhmCode ghm;
    Span<int16_t> errors;

    GhsCode ghs;
    SupplementCounters supplements;
    int ghs_price_cents;
};

struct ClassifyResultSet {
    HeapArray<ClassifyResult> results;

    SupplementCounters supplements;
    int64_t ghs_total_cents;

    struct {
        HeapArray<int16_t> errors;
    } store;
};

Span<const Stay> Cluster(Span<const Stay> stays, ClusterMode mode,
                             Span<const Stay> *out_remainder);

GhmCode Aggregate(const TableSet &table_set, Span<const Stay> stays,
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

int PriceGhs(const GhsPricing &pricing, int duration, bool death);
int PriceGhs(const PricingSet &pricing_set, GhsCode ghs, Date date, int duration, bool death);

void Classify(const TableSet &table_set, const AuthorizationSet &authorization_set,
              const PricingSet &pricing_set, Span<const Stay> stays, ClusterMode cluster_mode,
              ClassifyResultSet *out_result_set);
