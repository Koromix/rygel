// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"
#include "mco_authorization.hh"
#include "mco_stay.hh"
#include "mco_table.hh"

namespace RG {

enum class mco_ClassifyFlag {
    MonoOriginalStay = 1 << 0,
    IgnoreConfirmation = 1 << 1,
    IgnoreProcedureAddition = 1 << 2,
    IgnoreProcedureExtension = 1 << 3
};
static const OptionDesc mco_ClassifyFlagOptions[] = {
    {"MonoOrigStay",   "Use original stays in mono algorithm"},
    {"IgnoreConfirm",  "Ignore RSS confirmation flag"},
    {"IgnoreCompProc", "Ignore complementary procedure check"},
    {"IgnoreProcExt",  "Ignore ATIH procedure extension check"}
};

struct mco_PreparedStay {
    enum class Marker {
        PartialUnit = 1 << 0,
        MixedUnit = 1 << 1,

        ChildbirthDiagnosis = 1 << 2,
        ChildbirthProcedure = 1 << 3,
        Childbirth = (1 << 2) | (1 << 3),
        ChildbirthType = 1 << 4
    };

    const mco_Stay *stay;
    int16_t duration;
    int16_t age;
    int32_t age_days;

    const mco_DiagnosisInfo *main_diag_info;
    const mco_DiagnosisInfo *linked_diag_info;
    Span<const mco_DiagnosisInfo *> diagnoses;

    Span<const mco_ProcedureInfo *> procedures;
    uint8_t proc_activities;

    int8_t auth_type;
    uint16_t markers;
    LocalDate childbirth_date;
};

struct mco_PreparedSet {
    const mco_TableIndex *index;

    Span<const mco_Stay> mono_stays;
    mco_Stay stay;

    HeapArray<mco_PreparedStay> mono_preps;
    mco_PreparedStay prep;
    const mco_PreparedStay *main_prep;

    struct {
        HeapArray<const mco_DiagnosisInfo *> diagnoses;
        HeapArray<const mco_ProcedureInfo *> procedures;
    } store;
};

struct mco_ErrorSet {
    int16_t main_error = 0;
    int16_t priority = 0;
    Bitset<512> errors;
};

struct mco_Result {
    Span<const mco_Stay> stays;

    const mco_TableIndex *index;
    int16_t main_stay_idx;
    int16_t duration;
    int16_t age;
    mco_GhmCode ghm;
    mco_GhmCode ghm_for_ghs;
    int16_t main_error;

    drd_Sector sector;
    mco_GhsCode ghs;
    int16_t ghs_duration;

    mco_SupplementCounters<int16_t> supplement_days;
};

mco_GhmCode mco_Prepare(const mco_TableSet &table_set,
                        const mco_AuthorizationSet &authorization_set,
                        Span<const mco_Stay> mono_stays, unsigned int flags,
                        mco_PreparedSet *out_prepared_set, mco_ErrorSet *out_errors);

int mco_GetMinimalDurationForSeverity(int severity);
int mco_LimitSeverity(int severity, int duration);
bool mco_TestGhmRootExclusion(const mco_DiagnosisInfo &cma_diag_info,
                              const mco_GhmRootInfo &ghm_root_info);
bool mco_TestDiagnosisExclusion(const mco_TableIndex &index,
                                const mco_DiagnosisInfo &cma_diag_info,
                                const mco_DiagnosisInfo &main_diag_info);
bool mco_TestExclusion(const mco_TableIndex &index, int age,
                       const mco_DiagnosisInfo &cma_diag_info,
                       const mco_GhmRootInfo &ghm_root_info,
                       const mco_DiagnosisInfo &main_diag_info,
                       const mco_DiagnosisInfo *linked_diag_info);

mco_GhmCode mco_PickGhm(const mco_TableIndex &index,
                        const mco_PreparedStay &prep, Span<const mco_PreparedStay> mono_preps,
                        unsigned int flags, mco_ErrorSet *out_errors, mco_GhmCode *out_ghm_for_ghs = nullptr);
mco_GhsCode mco_PickGhs(const mco_TableIndex &index, const mco_AuthorizationSet &authorization_set,
                        drd_Sector sector, const mco_PreparedStay &prep,
                        Span<const mco_PreparedStay> mono_preps, mco_GhmCode ghm,
                        unsigned int flags, int16_t *out_ghs_duration = nullptr);
void mco_CountSupplements(const mco_TableIndex &index,
                          const mco_PreparedStay &prep, Span<const mco_PreparedStay> mono_preps,
                          mco_GhmCode ghm, mco_GhsCode ghs, unsigned int flags,
                          mco_SupplementCounters<int16_t> *out_counters,
                          Strider<mco_SupplementCounters<int16_t>> out_mono_counters = {});

Size mco_Classify(const mco_TableSet &table_set, const mco_AuthorizationSet &authorization_set,
                  drd_Sector sector, Span<const mco_Stay> stays, unsigned int flags,
                  HeapArray<mco_Result> *out_results, HeapArray<mco_Result> *out_mono_results = nullptr);

}
