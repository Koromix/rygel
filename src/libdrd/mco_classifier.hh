// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_authorizations.hh"
#include "mco_stays.hh"
#include "mco_tables.hh"

enum class mco_ClassifyFlag {
    IgnoreConfirmation = 1 << 0,
    IgnoreProcedureDoc = 1 << 1,
    IgnoreProcedureExtension = 1 << 2
};
static const OptionDesc mco_ClassifyFlagOptions[] = {
    {"ignore_confirm", "Ignore RSS confirmation flag"},
    {"ignore_proc_doc", "Ignore procedure documentation check"},
    {"ignore_proc_ext", "Ignore ATIH procedure extension check"}
};

struct mco_Aggregate {
    enum class Flag {
        ChildbirthDiagnosis = 1 << 0,
        ChildbirthProcedure = 1 << 1,
        Childbirth = (1 << 0) | (1 << 1),
        ChildbirthType = 1 << 2
    };

    Span<const mco_Stay> stays;

    const mco_TableIndex *index;

    mco_Stay stay;
    const mco_DiagnosisInfo *main_diag_info;
    const mco_DiagnosisInfo *linked_diag_info;
    Span<const mco_DiagnosisInfo *> diagnoses;
    Span<const mco_ProcedureInfo *> procedures;
    uint8_t proc_activities;

    uint16_t flags;
    int age;
    int age_days;
    int duration;

    const mco_Stay *main_stay;
};

struct mco_ErrorSet {
    int16_t main_error = 0;
    int priority = 0;
    Bitset<512> errors;
};

struct mco_Result {
    Span<const mco_Stay> stays;
    Size main_stay_idx;

    mco_GhmCode ghm;
    int16_t main_error;

    mco_GhsCode ghs;
    int ghs_price_cents;
    mco_SupplementCounters<int16_t> supplement_days;
    mco_SupplementCounters<int32_t> supplement_cents;
    int price_cents;
};

struct mco_Summary {
    Size results_count = 0;
    Size stays_count = 0;
    Size failures_count = 0;

    int64_t ghs_total_cents = 0;
    mco_SupplementCounters<int32_t> supplement_days;
    mco_SupplementCounters<int64_t> supplement_cents;
    int64_t total_cents = 0;

    mco_Summary &operator+=(const mco_Summary &other)
    {
        results_count += other.results_count;
        stays_count += other.stays_count;
        failures_count += other.failures_count;

        ghs_total_cents += other.ghs_total_cents;
        supplement_days += other.supplement_days;
        supplement_cents += other.supplement_cents;
        total_cents += other.total_cents;

        return *this;
    }
    mco_Summary operator+(const mco_Summary &other)
    {
        mco_Summary copy = *this;
        copy += other;
        return copy;
    }
};

Span<const mco_Stay> mco_Split(Span<const mco_Stay> stays,
                               Span<const mco_Stay> *out_remainder = nullptr);

mco_GhmCode mco_Prepare(const mco_TableSet &table_set, Span<const mco_Stay> stays,
                        unsigned int flags, mco_Aggregate *out_agg,
                        HeapArray<const mco_DiagnosisInfo *> *out_diagnoses,
                        HeapArray<const mco_ProcedureInfo *> *out_procedures,
                        mco_ErrorSet *out_errors);

int mco_GetMinimalDurationForSeverity(int severity);
int mco_LimitSeverityWithDuration(int severity, int duration);

mco_GhmCode mco_ClassifyGhm(const mco_Aggregate &agg, unsigned int flags, mco_ErrorSet *out_errors);

mco_GhsCode mco_ClassifyGhs(const mco_Aggregate &agg, const mco_AuthorizationSet &authorization_set,
                            mco_GhmCode ghm, unsigned int flags);
void mco_CountSupplements(const mco_Aggregate &agg, const mco_AuthorizationSet &authorization_set,
                          mco_GhsCode ghs, unsigned int flags,
                          mco_SupplementCounters<int16_t> *out_counters);

int mco_PriceGhs(const mco_GhsPriceInfo &price_info, int duration, bool death);
int mco_PriceGhs(const mco_Aggregate &agg, mco_GhsCode ghs);
int mco_PriceSupplements(const mco_TableIndex &index, const mco_SupplementCounters<int16_t> &days,
                         mco_SupplementCounters<int32_t> *out_prices);

Size mco_ClassifyRaw(const mco_TableSet &table_set, const mco_AuthorizationSet &authorization_set,
                     Span<const mco_Stay> stays, unsigned int flags, mco_Result out_results[]);
void mco_Classify(const mco_TableSet &table_set, const mco_AuthorizationSet &authorization_set,
                  Span<const mco_Stay> stays, unsigned int flags, HeapArray<mco_Result> *out_results);
void mco_ClassifyParallel(const mco_TableSet &table_set, const mco_AuthorizationSet &authorization_set,
                          Span<const mco_Stay> stays, unsigned int flags,
                          HeapArray<mco_Result> *out_results);

void mco_Summarize(Span<const mco_Result> results, mco_Summary *out_summary);
