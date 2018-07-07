// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_authorizations.hh"
#include "mco_stays.hh"
#include "mco_tables.hh"

enum class mco_ClassifyFlag {
    Mono = 1 << 0,
    MonoOriginalStay = 1 << 1,
    IgnoreConfirmation = 1 << 2,
    IgnoreProcedureDoc = 1 << 3,
    IgnoreProcedureExtension = 1 << 4
};
static const OptionDesc mco_ClassifyFlagOptions[] = {
    {"mono", "Perform mono-stay classification"},
    {"mono_orig_stay", "Use original stays in mono algorithm"},
    {"ignore_confirm", "Ignore RSS confirmation flag"},
    {"ignore_proc_doc", "Ignore procedure documentation check"},
    {"ignore_proc_ext", "Ignore ATIH procedure extension check"}
};

struct mco_PreparedStay {
    enum class Marker {
        ChildbirthDiagnosis = 1 << 0,
        ChildbirthProcedure = 1 << 1,
        Childbirth = (1 << 0) | (1 << 1),
        ChildbirthType = 1 << 2
    };

    const mco_Stay *stay;
    int duration;
    int age;
    int age_days;

    const mco_DiagnosisInfo *main_diag_info;
    const mco_DiagnosisInfo *linked_diag_info;
    Span<const mco_DiagnosisInfo *> diagnoses;

    Span<const mco_ProcedureInfo *> procedures;
    uint8_t proc_activities;

    uint16_t markers;
    Date childbirth_date;
};

struct mco_PreparedSet {
    const mco_TableIndex *index;

    Span<const mco_Stay> mono_stays;
    HeapArray<mco_PreparedStay> mono_preps;
    const mco_PreparedStay *main_prep;
    mco_Stay stay;
    mco_PreparedStay prep;

    struct {
        HeapArray<const mco_DiagnosisInfo *> diagnoses;
        HeapArray<const mco_ProcedureInfo *> procedures;
    } store;
};

struct mco_ErrorSet {
    int16_t main_error = 0;
    int priority = 0;
    Bitset<512> errors;
};

struct mco_Result {
    Span<const mco_Stay> stays;

    const mco_TableIndex *index;
    Size main_stay_idx;
    int duration;
    mco_GhmCode ghm;
    int16_t main_error;

    mco_GhsCode ghs;
    int ghs_duration;

    mco_SupplementCounters<int16_t> supplement_days;
};

Span<const mco_Stay> mco_Split(Span<const mco_Stay> mono_stays,
                               Span<const mco_Stay> *out_remainder = nullptr);

mco_GhmCode mco_Prepare(const mco_TableSet &table_set, Span<const mco_Stay> mono_stays,
                        unsigned int flags, mco_PreparedSet *out_prepared_set,
                        mco_ErrorSet *out_errors);

int mco_GetMinimalDurationForSeverity(int severity);
int mco_LimitSeverityWithDuration(int severity, int duration);
bool mco_TestGhmRootExclusion(int8_t sex, const mco_DiagnosisInfo &cma_diag_info,
                              const mco_GhmRootInfo &ghm_root_info);
bool mco_TestDiagnosisExclusion(const mco_TableIndex &index,
                                const mco_DiagnosisInfo &cma_diag_info,
                                const mco_DiagnosisInfo &main_diag_info);
bool mco_TestExclusion(const mco_TableIndex &index, int8_t sex, int age,
                       const mco_DiagnosisInfo &cma_diag_info,
                       const mco_GhmRootInfo &ghm_root_info,
                       const mco_DiagnosisInfo &main_diag_info,
                       const mco_DiagnosisInfo *linked_diag_info = nullptr);

mco_GhmCode mco_PickGhm(const mco_TableIndex &index,
                        const mco_PreparedStay &prep, Span<const mco_PreparedStay> mono_preps,
                        unsigned int flags, mco_ErrorSet *out_errors);
mco_GhsCode mco_PickGhs(const mco_TableIndex &index, const mco_AuthorizationSet &authorization_set,
                        const mco_PreparedStay &prep, Span<const mco_PreparedStay> mono_preps,
                        mco_GhmCode ghm, unsigned int flags, int *out_ghs_duration = nullptr);
void mco_CountSupplements(const mco_TableIndex &index, const mco_AuthorizationSet &authorization_set,
                          const mco_PreparedStay &prep, Span<const mco_PreparedStay> mono_preps,
                          mco_GhmCode ghm, mco_GhsCode ghs, unsigned int flags,
                          mco_SupplementCounters<int16_t> *out_counters,
                          Strider<mco_SupplementCounters<int16_t>> out_mono_counters = {});

void mco_Classify(const mco_TableSet &table_set, const mco_AuthorizationSet &authorization_set,
                  Span<const mco_Stay> stays, unsigned int flags, HeapArray<mco_Result> *out_results,
                  HeapArray<mco_Result> *out_mono_results = nullptr);
void mco_ClassifyParallel(const mco_TableSet &table_set, const mco_AuthorizationSet &authorization_set,
                          Span<const mco_Stay> stays, unsigned int flags,
                          HeapArray<mco_Result> *out_results,
                          HeapArray<mco_Result> *out_mono_results = nullptr);
