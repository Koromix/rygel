// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "mco_classifier.hh"

namespace RG {

struct RunGhmTreeContext {
    const mco_TableIndex *index;

    const mco_Stay *stay;
    const mco_PreparedStay *prep;

    // Keep a copy for DP - DR reversal (function 34)
    const mco_DiagnosisInfo *main_diag_info;
    const mco_DiagnosisInfo *linked_diag_info;
    int gnn;
};

static int16_t ComputeAge(Date date, Date birthdate)
{
    int16_t age = (int16_t)(date.st.year - birthdate.st.year);
    age -= (date.st.month < birthdate.st.month ||
            (date.st.month == birthdate.st.month && date.st.day < birthdate.st.day));

    return age;
}

static inline uint8_t GetDiagnosisByte(int8_t sex, const mco_DiagnosisInfo &diag_info, uint8_t byte_idx)
{
    RG_ASSERT(byte_idx < RG_SIZE(mco_DiagnosisInfo::attributes[0].raw));
    return diag_info.Attributes(sex).raw[byte_idx];
}

static inline bool TestDiagnosis(int8_t sex, const mco_DiagnosisInfo &diag_info, drd_ListMask mask)
{
    RG_ASSERT_DEBUG(mask.offset >= 0 && mask.offset <= UINT8_MAX);
    return GetDiagnosisByte(sex, diag_info, (uint8_t)mask.offset) & mask.value;
}
static inline bool TestDiagnosis(int8_t sex, const mco_DiagnosisInfo &diag_info,
                                 uint8_t offset, uint8_t value)
{
    return GetDiagnosisByte(sex, diag_info, offset) & value;
}

static inline uint8_t GetProcedureByte(const mco_ProcedureInfo &proc_info, int16_t byte_idx)
{
    RG_ASSERT(byte_idx >= 0 && byte_idx < RG_SIZE(mco_ProcedureInfo::bytes));
    return proc_info.bytes[byte_idx];
}

static inline bool TestProcedure(const mco_ProcedureInfo &proc_info, drd_ListMask mask)
{
    return GetProcedureByte(proc_info, mask.offset) & mask.value;
}
static inline bool TestProcedure(const mco_ProcedureInfo &proc_info, int16_t offset, uint8_t value)
{
    return GetProcedureByte(proc_info, offset) & value;
}

static const mco_PreparedStay *FindMainStay(Span<const mco_PreparedStay> mono_preps, int duration)
{
    RG_ASSERT_DEBUG(duration >= 0);

    int max_duration = -1;
    const mco_PreparedStay *zx_prep = nullptr;
    int zx_duration = -1;
    int proc_priority = 0;
    const mco_PreparedStay *trauma_prep = nullptr;
    const mco_PreparedStay *last_trauma_prep = nullptr;
    bool ignore_trauma = false;
    const mco_PreparedStay *score_prep = nullptr;
    int base_score = 0;
    int min_score = INT_MAX;

    for (const mco_PreparedStay &mono_prep: mono_preps) {
        const mco_Stay &mono_stay = *mono_prep.stay;

        int stay_score = base_score;

        proc_priority = 0;
        for (const mco_ProcedureInfo *proc_info: mono_prep.procedures) {
            if (proc_info->bytes[0] & 0x80 && !(proc_info->bytes[23] & 0x80))
                return &mono_prep;

            if (proc_priority < 3 && proc_info->bytes[38] & 0x2) {
                proc_priority = 3;
            } else if (proc_priority < 2 && duration <= 1 && proc_info->bytes[39] & 0x80) {
                proc_priority = 2;
            } else if (proc_priority < 1 && duration == 0 && proc_info->bytes[39] & 0x40) {
                proc_priority = 1;
            }
        }
        if (proc_priority == 3) {
            stay_score -= 999999;
        } else if (proc_priority == 2) {
            stay_score -= 99999;
        } else if (proc_priority == 1) {
            stay_score -= 9999;
        }

        if (mono_prep.duration > zx_duration && mono_prep.duration >= max_duration) {
            if (mono_stay.main_diagnosis.Matches("Z515") ||
                    mono_stay.main_diagnosis.Matches("Z502") ||
                    mono_stay.main_diagnosis.Matches("Z503")) {
                zx_prep = &mono_prep;
                zx_duration = mono_prep.duration;
            } else {
                zx_prep = nullptr;
            }
        }

        if (!ignore_trauma) {
            if (mono_prep.main_diag_info->Attributes(mono_stay.sex).raw[21] & 0x4) {
                last_trauma_prep = &mono_prep;
                if (mono_prep.duration > max_duration) {
                    trauma_prep = &mono_prep;
                }
            } else {
                ignore_trauma = true;
            }
        }

        if (mono_prep.main_diag_info->Attributes(mono_stay.sex).raw[21] & 0x20) {
            stay_score += 150;
        } else if (mono_prep.duration >= 2) {
            base_score += 100;
        }
        if (mono_prep.duration == 0) {
            stay_score += 2;
        } else if (mono_prep.duration == 1) {
            stay_score++;
        }
        if (mono_prep.main_diag_info->Attributes(mono_stay.sex).raw[21] & 0x2) {
            stay_score += 201;
        }

        if (stay_score < min_score) {
            score_prep = &mono_prep;
            min_score = stay_score;
        }

        if (mono_prep.duration > max_duration) {
            max_duration = mono_prep.duration;
        }
    }

    if (zx_prep)
        return zx_prep;
    if (last_trauma_prep >= score_prep)
        return trauma_prep;
    return score_prep;
}

static bool SetError(mco_ErrorSet *error_set, int16_t error, int16_t priority = 1)
{
    if (!error)
        return true;

    RG_ASSERT_DEBUG(error >= 0 && error < decltype(mco_ErrorSet::errors)::Bits);
    if (error_set) {
        if (priority >= 0 && (!error_set->main_error || priority > error_set->priority ||
                              (priority == error_set->priority && error < error_set->main_error))) {
            error_set->main_error = error;
            error_set->priority = priority;
        }
        error_set->errors.Set(error);
    }

    // For convenience
    return false;
}

static bool CheckDiagnosisErrors(const mco_PreparedStay &prep, const mco_DiagnosisInfo &diag_info,
                                 const int16_t error_codes[13], mco_ErrorSet *out_errors)
{
    // Inappropriate, imprecise warnings
    if (RG_UNLIKELY(diag_info.warnings & (1 << 9))) {
        SetError(out_errors, error_codes[8], -1);
    }
    if (RG_UNLIKELY(diag_info.warnings & (1 << 0))) {
        SetError(out_errors, error_codes[9], -1);
    }
    if (RG_UNLIKELY(diag_info.warnings & (1 << 10))) {
        SetError(out_errors, error_codes[10], -1);
    }

    // Sex warning
    {
        int sex_bit = 13 - prep.stay->sex;
        if (RG_UNLIKELY(diag_info.warnings & (1 << sex_bit))) {
            SetError(out_errors, error_codes[11], -1);
        }
    }

    // Age warning
    if (diag_info.warnings) {
        int age_bit;
        if (prep.age_days < 29) {
            age_bit = 4;
        } else if (!prep.age) {
            age_bit = 3;
        } else if (prep.age < (prep.stay->exit.date >= Date(2016, 3, 1) ? 8 : 10)) {
            age_bit = 5;
        } else if (prep.age < 20) {
            age_bit = 6;
        } else if (prep.age < 65) {
            age_bit = 7;
        } else {
            age_bit = 8;
        }

        if (RG_UNLIKELY(diag_info.warnings & (1 << age_bit))) {
            SetError(out_errors, error_codes[12], -1);
        }
    }

    const auto &diag_attr = diag_info.Attributes(prep.stay->sex);

    // Real errors
    if (RG_UNLIKELY(diag_attr.raw[5] & 2)) {
        return SetError(out_errors, error_codes[0]);
    } else if (RG_UNLIKELY(!diag_attr.raw[0])) {
        switch (diag_attr.raw[1]) {
            case 0: { return SetError(out_errors, error_codes[1]); } break;
            case 1: { return SetError(out_errors, error_codes[2]); } break;
            case 2: { return SetError(out_errors, error_codes[3]); } break;
            case 3: { return SetError(out_errors, error_codes[4]); } break;
        }
    } else if (RG_UNLIKELY(prep.stay->exit.date >= Date(2014, 3, 1) &&
                           diag_attr.raw[0] == 23 && diag_attr.raw[1] == 14)) {
        return SetError(out_errors, error_codes[5]);
    } else if (RG_UNLIKELY(diag_attr.raw[19] & 0x10 && prep.age < 9)) {
        return SetError(out_errors, error_codes[6]);
    } else if (RG_UNLIKELY(diag_attr.raw[19] & 0x8 && prep.age >= 2)) {
        return SetError(out_errors, error_codes[7]);
    }

    return true;
}

static bool AppendValidDiagnoses(mco_PreparedSet *out_prepared_set, mco_ErrorSet *out_errors)
{
    const mco_TableIndex &index = *out_prepared_set->index;
    mco_PreparedStay *out_prep = &out_prepared_set->prep;

    bool valid = true;

    static const int16_t main_diagnosis_errors[13] = {
        68, // Obsolete diagnosis
        113, 114, 115, 113, 180, // Imprecise, reserved for OMS use, etc.
        130, 133, // Age-related (O, P, Z37, Z38)
        88, 84, 87, 86, 85 // Warnings
    };
    static const int16_t linked_diagnosis_errors[13] = {
        95,
        116, 117, 118, 0, 181,
        131, 134,
        0, 96, 99, 98, 97
    };
    static const int16_t associate_diagnosis_errors[13] = {
        71,
        0, 0, 119, 0, 182,
        132, 135,
        0, 90, 93, 92, 91
    };

    // We cannot allow the HeapArray to move
    {
        Size diagnoses_count = 0;
        for (const mco_Stay &mono_stay: out_prepared_set->mono_stays) {
            diagnoses_count += mono_stay.other_diagnoses.len + 2;
        }

        out_prepared_set->store.diagnoses.RemoveFrom(0);
        out_prepared_set->store.diagnoses.Grow(diagnoses_count);
    }

    for (mco_PreparedStay &mono_prep: out_prepared_set->mono_preps) {
        const mco_Stay &mono_stay = *mono_prep.stay;

        mono_prep.diagnoses.ptr = out_prepared_set->store.diagnoses.end();
        for (drd_DiagnosisCode diag: mono_stay.other_diagnoses) {
            if (diag.Matches("Z37")) {
                out_prep->markers |= (int)mco_PreparedStay::Marker::ChildbirthDiagnosis;
                mono_prep.markers |= (int)mco_PreparedStay::Marker::ChildbirthDiagnosis;
            }
            if (diag.Matches("O8") && (diag.str[2] == '0' || diag.str[2] == '1' ||
                                       diag.str[2] == '2' || diag.str[2] == '3' ||
                                       diag.str[2] == '4')) {
                out_prep->markers |= (int)mco_PreparedStay::Marker::ChildbirthType;
                mono_prep.markers |= (int)mco_PreparedStay::Marker::ChildbirthType;
            }

            const mco_DiagnosisInfo *diag_info = index.FindDiagnosis(diag);
            if (RG_LIKELY(diag_info)) {
                out_prepared_set->store.diagnoses.Append(diag_info);
                mono_prep.diagnoses.len++;

                valid &= CheckDiagnosisErrors(*out_prep, *diag_info,
                                              associate_diagnosis_errors, out_errors);
            } else {
                valid &= SetError(out_errors, 70);
            }
        }

        // Main diagnosis is valid (checks are done in CheckMainError)
        mono_prep.main_diag_info = index.FindDiagnosis(mono_stay.main_diagnosis);
        if (RG_LIKELY(mono_prep.main_diag_info)) {
            out_prepared_set->store.diagnoses.Append(mono_prep.main_diag_info);
            mono_prep.diagnoses.len++;

            valid &= CheckDiagnosisErrors(*out_prep, *mono_prep.main_diag_info,
                                          main_diagnosis_errors, out_errors);
        } else {
            valid &= SetError(out_errors, 67);
        }

        if (mono_stay.linked_diagnosis.IsValid()) {
            mono_prep.linked_diag_info = index.FindDiagnosis(mono_stay.linked_diagnosis);
            if (RG_LIKELY(mono_prep.linked_diag_info)) {
                out_prepared_set->store.diagnoses.Append(mono_prep.linked_diag_info);
                mono_prep.diagnoses.len++;

                valid &= CheckDiagnosisErrors(*out_prep, *mono_prep.linked_diag_info,
                                              linked_diagnosis_errors, out_errors);
            } else {
                valid &= SetError(out_errors, 94);
            }
        }
    }

    // We don't deduplicate diagnoses anymore (we used to)
    out_prepared_set->prep.diagnoses = out_prepared_set->store.diagnoses;

    return valid;
}

static bool AppendValidProcedures(mco_PreparedSet *out_prepared_set, unsigned int flags,
                                  mco_ErrorSet *out_errors)
{
    const mco_TableIndex &index = *out_prepared_set->index;
    mco_PreparedStay *out_prep = &out_prepared_set->prep;
    const mco_Stay &stay = *out_prep->stay;

    bool valid = true;

    Size max_pointers_count = 0;
    Size max_procedures_count = 0;
    for (const mco_Stay &mono_stay: out_prepared_set->mono_stays) {
        max_pointers_count += mono_stay.procedures.len;
        for (const mco_ProcedureRealisation &proc: mono_stay.procedures) {
            max_procedures_count += proc.count;
        }
    }

    // We cannot allow the HeapArray to move
    out_prepared_set->store.procedures.RemoveFrom(0);
    out_prepared_set->store.procedures.Grow(max_pointers_count + max_procedures_count);
    out_prepared_set->store.procedures.len = max_pointers_count;

    Size pointers_count = 0;
    for (mco_PreparedStay &mono_prep: out_prepared_set->mono_preps) {
        const mco_Stay &mono_stay = *mono_prep.stay;

        // Reuse for performance
        static RG_THREAD_LOCAL Bitset<512> additions;
        int additions_mismatch = 0;
        uint8_t proc_activities = 0;

        mono_prep.procedures.ptr = out_prepared_set->store.procedures.end();
        for (const mco_ProcedureRealisation &proc: mono_stay.procedures) {
            if (RG_UNLIKELY(!proc.count)) {
                valid &= SetError(out_errors, 52);
            }
            if (RG_UNLIKELY(!proc.activity)) {
                valid &= SetError(out_errors, 103);
            }
            if (RG_UNLIKELY(proc.doc && (!IsAsciiAlphaOrDigit(proc.doc) ||
                                         proc.doc == 'I' || proc.doc == 'O'))) {
                valid &= SetError(out_errors, 173);
            }

            const mco_ProcedureInfo *proc_info = index.FindProcedure(proc.proc, proc.phase,
                                                                     mono_stay.exit.date);
            if (RG_LIKELY(proc_info)) {
                if (RG_UNLIKELY((proc_info->bytes[43] & 0x40) && mono_stay.sex == 2)) {
                    SetError(out_errors, 148, -1);
                }
                if (RG_UNLIKELY((out_prep->age || out_prep->age_days > 28) &&
                                (proc_info->bytes[44] & 0x20) && (!stay.newborn_weight ||
                                                                  stay.newborn_weight >= 3000))) {
                    valid &= SetError(out_errors, 149);
                }

                if (proc_info->bytes[41] & 0x2) {
                    out_prep->markers |= (int)mco_PreparedStay::Marker::ChildbirthProcedure;
                    mono_prep.markers |= (int)mco_PreparedStay::Marker::ChildbirthProcedure;
                }

                if (RG_UNLIKELY(!proc.date.IsValid() ||
                                proc.date < mono_stay.entry.date ||
                                proc.date > mono_stay.exit.date)) {
                    if (proc_info->bytes[41] & 0x2) {
                        valid &= SetError(out_errors, 142);
                    } else if (proc.date.value) {
                        // NOTE: I don't know if I'm supposed to ignore this procedure in this
                        // case. I need to test how the official classifier deals with this.
                        SetError(out_errors, 102, -1);
                    }
                } else if (proc_info->bytes[41] & 0x2) {
                    if (!out_prep->childbirth_date.value) {
                        out_prep->childbirth_date = proc.date;
                    }
                    if (!mono_prep.childbirth_date.value) {
                        mono_prep.childbirth_date = proc.date;
                    }
                }

                if (RG_UNLIKELY(!(flags & (int)mco_ClassifyFlag::IgnoreProcedureExtension) &&
                                stay.exit.date >= Date(2016, 3, 1) &&
                                !(proc_info->extensions & (1u << proc.extension)))) {
                    if (stay.exit.date >= Date(2019, 3, 1) && !proc.extension) {
                        SetError(out_errors, 192, 0);
                    } else if (stay.exit.date >= Date(2017, 3, 1)) {
                        valid &= SetError(out_errors, 186);
                    } else {
                        SetError(out_errors, 186, 0);
                    }
                }

                uintptr_t global_ptr_mask = 0;
                if (!TestStr(MakeSpan(proc.proc.str, 4), "YYYY")) {
                    if (RG_UNLIKELY(!(proc_info->activities & (1 << proc.activity)))) {
                        if (proc.activity == 4) {
                            valid &= SetError(out_errors, 110);
                        } else if (proc.activity < 1 || proc.activity > 5) {
                            valid &= SetError(out_errors, 103);
                        } else {
                            SetError(out_errors, 111, 0);
                        }
                    }

                    if (RG_UNLIKELY(stay.exit.date >= Date(2013, 3, 1) &&
                                    proc.activity == 4 && !proc.doc)) {
                        SetError(out_errors, 170, 0);
                    }

                    // We use the pointer's LSB as a flag which is set to 1 when the procedure
                    // requires activity 1. Combined with a pointer-based sort this allows
                    // us to trivially detect when activity 1 is missing for a given procedure
                    // in the deduplication phase below (error 167).
                    RG_STATIC_ASSERT(std::alignment_of<mco_ProcedureInfo>::value >= 2);
                    if (proc.activity != 1 && !(proc_info->bytes[42] & 0x2)) {
                        global_ptr_mask = 0x1;
                    }
                }

                uintptr_t mono_ptr_mask = 0;
                if (!(flags & (int)mco_ClassifyFlag::IgnoreProcedureAddition)) {
                    RG_STATIC_ASSERT(std::alignment_of<mco_ProcedureInfo>::value >= 8);

                    if ((proc_info->bytes[32] & 0x8) &&
                            proc.activity >= 0 && proc.activity < RG_LEN(proc_info->additions) &&
                            proc_info->additions[proc.activity]) {
                        additions_mismatch += !additions.TestAndSet(proc_info->additions[proc.activity]);
                    }
                    if (proc_info->bytes[32] & 0x4) {
                        mono_ptr_mask = proc.activity & 0x7;
                    }
                }

                out_prepared_set->store.procedures.ptr[pointers_count++] =
                    (const mco_ProcedureInfo *)((uintptr_t)proc_info | global_ptr_mask);
                for (Size i = 0; i < proc.count; i++) {
                    const mco_ProcedureInfo *masked_ptr =
                        (const mco_ProcedureInfo *)((uintptr_t)proc_info | mono_ptr_mask);
                    out_prepared_set->store.procedures.Append(masked_ptr);
                }
                mono_prep.procedures.len += proc.count;

                proc_activities |= 1 << proc.activity;
            } else {
                Span <const mco_ProcedureInfo> compatible_procs = index.FindProcedure(proc.proc);
                bool valid_proc = std::any_of(compatible_procs.begin(), compatible_procs.end(),
                                              [&](const mco_ProcedureInfo &proc_info) {
                    return proc_info.phase == proc.phase;
                });
                if (valid_proc) {
                    if (!TestStr(MakeSpan(proc.proc.str, 4), "YYYY")) {
                        if (mono_stay.exit.date < compatible_procs[0].limit_dates[0]) {
                            valid &= SetError(out_errors, 79);
                        } else if (mono_stay.entry.date >= compatible_procs[compatible_procs.len - 1].limit_dates[1]) {
                            valid &= SetError(out_errors, 78);
                        }
                    }
                } else {
                    valid &= SetError(out_errors, 73);
                }
            }
        }

        if (!(flags & (int)mco_ClassifyFlag::IgnoreProcedureAddition)) {
            for (Size i = 0; i < mono_prep.procedures.len; i++) {
                const mco_ProcedureInfo *proc_info =
                    (const mco_ProcedureInfo *)((uintptr_t)mono_prep.procedures[i] & ~(uintptr_t)0x7);
                int8_t activity = (uintptr_t)mono_prep.procedures[i] & 0x7;

                if (activity) {
                    for (Size i = 0; additions_mismatch && i < proc_info->addition_list.len; i++) {
                        const mco_ProcedureLink &link =
                            index.procedure_links[proc_info->addition_list.offset + i];

                        if (activity == link.activity) {
                            additions_mismatch -= additions.TestAndSet(link.addition_idx, false);
                        }
                    }
                }

                mono_prep.procedures[i] = proc_info;
            }
            if (additions_mismatch) {
                SetError(out_errors, 112, 0);
            }

            additions.Clear();
        }

        out_prep->proc_activities |= proc_activities;
        mono_prep.proc_activities = proc_activities;
    }

    // Deduplicate procedures
    // TODO: Warn when we deduplicate procedures with different attributes,
    // such as when the two procedures fall into different date ranges / limits.
    if (pointers_count) {
        Span<const mco_ProcedureInfo *> procedures =
            out_prepared_set->store.procedures.Take(0, pointers_count);

        std::sort(procedures.begin(), procedures.end());

        // Need to treat index 0 specially for the loop to work correctly
        if (RG_UNLIKELY((uintptr_t)procedures[0] & 0x1)) {
            procedures[0] = (const mco_ProcedureInfo *)((uintptr_t)procedures[0] ^ 0x1);
            valid &= SetError(out_errors, 167);
        }

        Size j = 0;
        for (Size i = 0; i < procedures.len; i++) {
            const mco_ProcedureInfo *proc_info = procedures[i];
            if ((uintptr_t)proc_info & 0x1) {
                proc_info = (const mco_ProcedureInfo *)((uintptr_t)proc_info ^ 0x1);
                if (proc_info != procedures[j]) {
                    procedures[++j] = proc_info;
                    valid &= SetError(out_errors, 167);
                }
            } else if (proc_info != procedures[j]) {
                procedures[++j] = proc_info;
            }
        }

        out_prep->procedures = procedures.Take(0, j + 1);
    } else {
        out_prep->procedures = {};
    }

    return valid;
}

static bool CheckDateErrors(bool malformed_flag, Date date, const int16_t error_codes[3],
                            mco_ErrorSet *out_errors)
{
    if (RG_UNLIKELY(malformed_flag)) {
        return SetError(out_errors, error_codes[0]);
    } else if (RG_UNLIKELY(!date.value)) {
        return SetError(out_errors, error_codes[1]);
    } else if (RG_UNLIKELY(!date.IsValid())) {
        return SetError(out_errors, error_codes[2]);
    }

    return true;
}

static bool CheckDataErrors(Span<const mco_Stay> mono_stays, mco_ErrorSet *out_errors)
{
    bool valid = true;

    // Bill id
    if (RG_UNLIKELY(mono_stays[0].errors & (int)mco_Stay::Error::MalformedBillId)) {
        static bool warned = false;
        if (!warned) {
            LogError("Non-numeric RSS identifiers are not supported");
            warned = true;
        }
        valid &= SetError(out_errors, 61);
    } else if (RG_UNLIKELY(!mono_stays[0].bill_id)) {
        valid &= SetError(out_errors, 11);
    }

    for (const mco_Stay &mono_stay: mono_stays) {
        // Sex
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedSex)) {
            valid &= SetError(out_errors, 17);
        } else if (RG_UNLIKELY(mono_stay.sex != 1 && mono_stay.sex != 2)) {
            valid &= SetError(out_errors, mono_stay.sex ? 17 : 16);
        }

        // Entry mode and origin
        if (RG_UNLIKELY(mono_stay.errors & ((int)mco_Stay::Error::MalformedEntryMode |
                                            (int)mco_Stay::Error::MalformedEntryOrigin))) {
            valid &= SetError(out_errors, 25);
        }

        // Exit mode and destination
        if (RG_UNLIKELY(mono_stay.errors & ((int)mco_Stay::Error::MalformedExitMode |
                                            (int)mco_Stay::Error::MalformedExitDestination))) {
            valid &= SetError(out_errors, 34);
        }

        // Sessions
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedSessionCount)) {
            valid &= SetError(out_errors, 36);
        }

        // Gestational age
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedGestationalAge)) {
            valid &= SetError(out_errors, 125);
        }

        // Menstrual period
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedLastMenstrualPeriod)) {
            valid &= SetError(out_errors, 160);
        } else if (RG_UNLIKELY(mono_stay.last_menstrual_period.value &&
                               !mono_stay.last_menstrual_period.IsValid())) {
            valid &= SetError(out_errors, 161);
        }

        // IGS2
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedIgs2)) {
            valid &= SetError(out_errors, 169);
        }

        // Flags
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedConfirmation)) {
            SetError(out_errors, 121, -1);
        }
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedRAAC)) {
            valid &= SetError(out_errors, 188);
        }

        // Diagnoses
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedMainDiagnosis)) {
           valid &= SetError(out_errors, 41);
        } else if (RG_UNLIKELY(!mono_stay.main_diagnosis.IsValid())) {
            valid &= SetError(out_errors, 40);
        }
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedLinkedDiagnosis)) {
            valid &= SetError(out_errors, 51);
        }
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MissingOtherDiagnosesCount)) {
            valid &= SetError(out_errors, 55);
        } else if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedOtherDiagnosesCount)) {
            valid &= SetError(out_errors, 56);
        } else if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedOtherDiagnosis)) {
            valid &= SetError(out_errors, 42);
        }

        // Procedures
        if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MissingProceduresCount)) {
            valid &= SetError(out_errors, 57);
        } else if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedProceduresCount)) {
            valid &= SetError(out_errors, 58);
        } else {
            if (RG_UNLIKELY(mono_stay.errors & (int)mco_Stay::Error::MalformedProcedureCode)) {
                valid &= SetError(out_errors, 43);
            }
            if (RG_UNLIKELY(mono_stays[mono_stays.len - 1].exit.date >= Date(2016, 3, 1) &&
                            mono_stay.errors & (int)mco_Stay::Error::MalformedProcedureExtension)) {
                valid &= SetError(out_errors, 185);
            }
        }
    }

    // Coherency checks
    for (Size i = 1; i < mono_stays.len; i++) {
        if (RG_UNLIKELY(mono_stays[i].sex != mono_stays[i - 1].sex &&
                        (mono_stays[i].sex == 1 || mono_stays[i].sex == 2))) {
            valid &= SetError(out_errors, 46);
        }

        if (RG_UNLIKELY(mono_stays[i].birthdate != mono_stays[i - 1].birthdate &&
                        mono_stays[i].birthdate.IsValid())) {
            valid &= SetError(out_errors, 45);
        }
    }

    return valid;
}

static bool CheckAggregateErrors(const mco_PreparedStay &prep,
                                 Span<const mco_PreparedStay> mono_preps, mco_ErrorSet *out_errors)
{
    const mco_Stay &stay = *prep.stay;

    bool valid = true;

    // Check PIE, mutations, RAAC
    if (stay.entry.mode == '0' || stay.exit.mode == '0') {
        if (RG_UNLIKELY(stay.exit.mode != stay.entry.mode)) {
            valid &= SetError(out_errors, 26);
            SetError(out_errors, 35);
        } else if (RG_UNLIKELY(prep.duration > 1)) {
            valid &= SetError(out_errors, 50);
        }
    } else {
        if (RG_UNLIKELY(stay.entry.mode == '6' && stay.entry.origin == '1')) {
            valid &= SetError(out_errors, 26);
        }
        if (RG_UNLIKELY(stay.exit.mode == '6' && stay.exit.destination == '1')) {
            valid &= SetError(out_errors, 35);
        }
        if (RG_UNLIKELY((stay.flags & (int)mco_Stay::Flag::RAAC) &&
                        (stay.exit.mode == '9' || (stay.exit.mode == '7' &&
                                                   stay.exit.destination == '1')))) {
            valid &= SetError(out_errors, 189);
        }
    }

    for (const mco_PreparedStay &mono_prep: mono_preps) {
        const mco_Stay &mono_stay = *mono_prep.stay;

        // Dates
        if (RG_UNLIKELY(mono_stay.entry.date.st.year < 1985 && mono_stay.entry.date.IsValid())) {
            SetError(out_errors, 77, -1);
        }

        // Entry mode and origin
        switch (mono_stay.entry.mode) {
            case '0':
            case '6': {
                if (RG_UNLIKELY(mono_stay.entry.mode == '0' && mono_stay.entry.origin == '6')) {
                    valid &= SetError(out_errors, 25);
                }
                if (RG_UNLIKELY(mono_stay.entry.mode == '6' && mono_stay.entry.origin == 'R')) {
                    valid &= SetError(out_errors, 25);
                }
            } RG_FALLTHROUGH;
            case '7': {
                switch (mono_stay.entry.origin) {
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '6':
                    case 'R': { /* Valid origin */ } break;

                    case 0: { valid &= SetError(out_errors, 53); } break;
                    default: { valid &= SetError(out_errors, 25); } break;
                }
            } break;

            case '8': {
                switch (mono_stay.entry.origin) {
                    case 0:
                    case '5':
                    case '7': { /* Valid origin */ } break;

                    default: { valid &= SetError(out_errors, 25); } break;
                }
            } break;

            case 'N': {
                if (stay.exit.date < Date(2019, 3, 1) || mono_stay.entry.origin) {
                    valid &= SetError(out_errors, 25);
                }
            } break;

            case 0: { valid &= SetError(out_errors, 24); } break;
            default: { valid &= SetError(out_errors, 25); } break;
        }

        // Exit mode and destination
        switch (mono_stay.exit.mode) {
            case '0':
            case '6':
            case '7': {
                switch (mono_stay.exit.destination) {
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '6': { /* Valid destination */ } break;

                    case 0: { valid &= SetError(out_errors, 54); } break;
                    default: { valid &= SetError(out_errors, 34); } break;
                }
            } break;

            case '8': {
                switch (mono_stay.exit.destination) {
                    case 0:
                    case '7': { /* Valid destination */ } break;

                    default: { valid &= SetError(out_errors, 34); } break;
                }
            } break;

            case '9': {
                if (RG_UNLIKELY(mono_stay.exit.destination)) {
                    valid &= SetError(out_errors, 34);
                }
            } break;

            case 0: { valid &= SetError(out_errors, 33); } break;
            default: { valid &= SetError(out_errors, 34); } break;
        }

        // Sessions
        if (RG_UNLIKELY(mono_preps.len > 1 && mono_stay.session_count > 0)) {
            valid &= SetError(out_errors, 37);
        }
        if (RG_UNLIKELY(mono_stay.session_count < 0 || mono_stay.session_count >= 32)) {
            SetError(out_errors, 66, -1);
        }

        // Gestational age
        if (mono_stay.gestational_age) {
            if (RG_UNLIKELY(mono_stay.gestational_age > 44 ||
                            (mono_stay.gestational_age < 22 && stay.exit.mode != '9' && !prep.age))) {
                valid &= SetError(out_errors, 127);
            } else if (RG_UNLIKELY(stay.newborn_weight &&
                                   ((mono_stay.gestational_age >= 37 && stay.newborn_weight < 1000 && !mono_stay.main_diagnosis.Matches("P95")) ||
                                    (mono_stay.gestational_age < 33 && stay.newborn_weight > 4000) ||
                                    (mono_stay.gestational_age < 28 && stay.newborn_weight > 2500)))) {
                valid &= SetError(out_errors, 129);
            }
        }

        // Menstrual period
        if (RG_UNLIKELY(mono_stay.last_menstrual_period.value &&
                        mono_stay.last_menstrual_period != stay.last_menstrual_period)) {
            valid &= SetError(out_errors, 163);
        }

        // Stillborn
        if (RG_UNLIKELY(mono_stay.main_diagnosis.Matches("P95"))) {
            if (RG_UNLIKELY(mono_stay.exit.mode != '9')) {
                valid &= SetError(out_errors, 143);
                SetError(out_errors, 147);
            } else if (RG_UNLIKELY(mono_preps.len > 1 || !mono_stay.newborn_weight ||
                                   (mono_stay.entry.mode != '8' && mono_stay.entry.mode != 'N') ||
                                   mono_stay.birthdate != mono_stay.entry.date ||
                                   mono_stay.exit.date != mono_stay.entry.date)) {
                valid &= SetError(out_errors, 147);
            }
        }
    }

    // Continuity checks
    for (Size i = 1; i < mono_preps.len; i++) {
        const mco_Stay &prev_mono_stay = *mono_preps[i - 1].stay;
        const mco_Stay &mono_stay = *mono_preps[i].stay;

        if (prev_mono_stay.exit.mode == '0' && mono_stay.entry.mode == '0') {
            if (RG_UNLIKELY(mono_stay.entry.date != prev_mono_stay.exit.date &&
                         mono_stay.entry.date - prev_mono_stay.exit.date != 1)) {
                valid &= SetError(out_errors, 50);
            }
        } else {
            if (RG_UNLIKELY(prev_mono_stay.exit.mode == '0' ||
                            mono_stay.entry.mode != '6' ||
                            mono_stay.entry.origin != '1')) {
                valid &= SetError(out_errors, 27);
            }
            if (RG_UNLIKELY(mono_stay.entry.mode == '0' ||
                            prev_mono_stay.exit.mode != '6' ||
                            prev_mono_stay.exit.destination != '1')) {
                valid &= SetError(out_errors, 49);
            }
            if (RG_UNLIKELY(mono_stay.entry.date != prev_mono_stay.exit.date)) {
                valid &= SetError(out_errors, 23);
            }
        }
    }

    // Sessions
    if (prep.main_diag_info && (prep.main_diag_info->Attributes(stay.sex).raw[8] & 0x2)) {
        if (RG_UNLIKELY(!prep.duration && !stay.session_count)) {
            bool tolerate = std::any_of(prep.procedures.begin(), prep.procedures.end(),
                                        [](const mco_ProcedureInfo *proc_info) {
                return (proc_info->bytes[44] & 0x40);
            });
            if (!tolerate) {
                // According to the manual, this is a blocking error but the
                // official classifier does not actually enforce it.
                SetError(out_errors, 145, 0);
            }
        } else if (RG_UNLIKELY(stay.session_count > prep.duration + 1)) {
            SetError(out_errors, 146, -1);
        }
    }

    // Gestation and newborn
    if (RG_UNLIKELY(!stay.gestational_age &&
                    ((prep.markers & (int)mco_PreparedStay::Marker::Childbirth) ||
                     stay.birthdate == stay.entry.date))) {
        valid &= SetError(out_errors, 126);
    }
    if (RG_UNLIKELY(stay.errors & (int)mco_Stay::Error::MalformedNewbornWeight)) {
        valid &= SetError(out_errors, 82);
    } else {
        if (RG_UNLIKELY(prep.age_days < 29 && !stay.newborn_weight)) {
            valid &= SetError(out_errors, 168);
        } else if (RG_UNLIKELY(stay.newborn_weight > 0 && stay.newborn_weight < 100)) {
            valid &= SetError(out_errors, 128);
        }
    }
    if (RG_UNLIKELY(stay.exit.date >= Date(2013, 3, 1) &&
                    (prep.markers & (int)mco_PreparedStay::Marker::ChildbirthProcedure) &&
                    stay.gestational_age < 22)) {
        valid &= SetError(out_errors, 174);
    }

    // Menstruation
    if (RG_UNLIKELY((prep.markers & (int)mco_PreparedStay::Marker::Childbirth) &&
                    !stay.last_menstrual_period.value)) {
        valid &= SetError(out_errors, 162);
    }
    if (RG_UNLIKELY(stay.sex == 1 && stay.last_menstrual_period.value)) {
        SetError(out_errors, 164, -1);
    }
    if (stay.last_menstrual_period.value) {
        if (RG_UNLIKELY(stay.last_menstrual_period > stay.entry.date)) {
            if (stay.exit.date >= Date(2016, 3, 1)) {
                valid &= SetError(out_errors, 165);
            } else {
                SetError(out_errors, 165, -1);
            }
        } else if (RG_UNLIKELY(stay.entry.date - stay.last_menstrual_period > 305)) {
            SetError(out_errors, 166, -1);
        }
    }

    // Newborn entry
    if (stay.exit.date >= Date(2019, 3, 1) && stay.entry.mode == 'N') {
        if (RG_UNLIKELY(stay.entry.date != stay.birthdate)) {
            valid &= SetError(out_errors, 190);
        }
        if (RG_UNLIKELY(mono_preps[0].stay->main_diagnosis.Matches("Z762"))) {
            valid &= SetError(out_errors, 191);
        }
    }

    return valid;
}

static bool InitCriticalData(const mco_TableSet &table_set, Span<const mco_Stay> mono_stays,
                             mco_PreparedSet *out_prepared_set, mco_ErrorSet *out_errors)
{
    // Malformed, missing, incoherent (e.g. 2001/02/29)
    static const int16_t birthdate_errors[3] = {14, 13, 39};
    static const int16_t entry_date_errors[3] = {20, 19, 21};
    static const int16_t exit_date_errors[3] = {29, 28, 30};

    bool valid = true;

    bool exit_date_valid = false;
    int total_duration = 0;
    for (const mco_Stay &mono_stay: mono_stays) {
        mco_PreparedStay mono_prep = {};

        mono_prep.stay = &mono_stay;

        bool birthdate_valid = CheckDateErrors(mono_stay.errors & (int)mco_Stay::Error::MalformedBirthdate,
                                               mono_stay.birthdate, birthdate_errors, out_errors);
        bool entry_date_valid = CheckDateErrors(mono_stay.errors & (int)mco_Stay::Error::MalformedEntryDate,
                                                mono_stay.entry.date, entry_date_errors, out_errors);
        exit_date_valid = CheckDateErrors(mono_stay.errors & (int)mco_Stay::Error::MalformedExitDate,
                                          mono_stay.exit.date, exit_date_errors, out_errors);

        if (RG_LIKELY(birthdate_valid && entry_date_valid)) {
            mono_prep.age = std::max(ComputeAge(mono_stay.entry.date, mono_stay.birthdate), (int16_t)0);
            mono_prep.age_days = std::max(mono_stay.entry.date - mono_stay.birthdate, 0);
        } else {
            mono_prep.age = -1;
            mono_prep.age_days = -1;
        }
        if (RG_LIKELY(entry_date_valid && exit_date_valid)) {
            int duration = mono_prep.stay->exit.date - mono_prep.stay->entry.date;
            if (RG_LIKELY(duration >= 0)) {
                mono_prep.duration = (int16_t)duration;
                total_duration += duration;
            } else {
                mono_prep.duration = -1;
                total_duration = INT_MIN;
            }
        } else {
            mono_prep.duration = -1;
            total_duration = INT_MIN;
        }

        valid &= birthdate_valid && entry_date_valid && exit_date_valid;

        if (RG_UNLIKELY(birthdate_valid && entry_date_valid &&
                        (mono_stay.birthdate > mono_stay.entry.date ||
                         mono_stay.entry.date.st.year - mono_stay.birthdate.st.year > 140))) {
            valid &= SetError(out_errors, 15);
        }
        if (RG_UNLIKELY(entry_date_valid && exit_date_valid &&
                        mono_stay.exit.date < mono_stay.entry.date)) {
            valid &= SetError(out_errors, 32);
        }

        out_prepared_set->mono_preps.Append(mono_prep);
    }

    out_prepared_set->prep.stay = &out_prepared_set->stay;
    out_prepared_set->prep.duration = (int16_t)std::max(total_duration, -1);
    out_prepared_set->prep.age = out_prepared_set->mono_preps[0].age;
    out_prepared_set->prep.age_days =out_prepared_set->mono_preps[0].age_days;

    if (RG_LIKELY(exit_date_valid)) {
        out_prepared_set->index = table_set.FindIndex(mono_stays[mono_stays.len - 1].exit.date);
    }

    return valid;
}

mco_GhmCode mco_Prepare(const mco_TableSet &table_set, Span<const mco_Stay> mono_stays,
                        unsigned int flags, mco_PreparedSet *out_prepared_set,
                        mco_ErrorSet *out_errors)
{
    RG_ASSERT_DEBUG(mono_stays.len > 0);

    // Reset prepared data
    out_prepared_set->index = nullptr;
    out_prepared_set->mono_stays = mono_stays;
    out_prepared_set->mono_preps.RemoveFrom(0);
    out_prepared_set->prep = {};
    out_prepared_set->main_prep = nullptr;

    // Aggregate mono_stays into stay
    out_prepared_set->stay = mono_stays[0];
    out_prepared_set->mono_stays = mono_stays;
    for (const mco_Stay &mono_stay: mono_stays) {
        if (mono_stay.gestational_age > 0) {
            out_prepared_set->stay.gestational_age = mono_stay.gestational_age;
        }
        if (mono_stay.last_menstrual_period.value &&
                !out_prepared_set->stay.last_menstrual_period.value) {
            out_prepared_set->stay.last_menstrual_period = mono_stay.last_menstrual_period;
        }
        if (mono_stay.igs2 > out_prepared_set->stay.igs2) {
            out_prepared_set->stay.igs2 = mono_stay.igs2;
        }
    }
    out_prepared_set->stay.exit = mono_stays[mono_stays.len - 1].exit;
    out_prepared_set->stay.flags = mono_stays[mono_stays.len - 1].flags &
                                   ((int)mco_Stay::Flag::Confirmed | (int)mco_Stay::Flag::RAAC);
    out_prepared_set->stay.other_diagnoses = {};
    out_prepared_set->stay.procedures = {};

    // Too critical to even try anything (all data is invalid)
    if (RG_UNLIKELY(mono_stays[0].errors & (int)mco_Stay::Error::UnknownRumVersion)) {
        RG_ASSERT_DEBUG(mono_stays.len == 1);

        out_prepared_set->prep.duration = -1;
        out_prepared_set->prep.age = -1;
        out_prepared_set->prep.age_days = -1;
        out_prepared_set->mono_preps.Append(out_prepared_set->prep);

        SetError(out_errors, 59);
        return mco_GhmCode::FromString("90Z00Z");
    }

    bool valid = true;

    valid &= InitCriticalData(table_set, mono_stays, out_prepared_set, out_errors);
    valid &= CheckDataErrors(mono_stays, out_errors);

    if (RG_LIKELY(valid)) {
        if (RG_UNLIKELY(!out_prepared_set->index)) {
            SetError(out_errors, 502, 2);
            return mco_GhmCode::FromString("90Z03Z");
        }

        // Aggregate diagnoses and procedures
        valid &= AppendValidDiagnoses(out_prepared_set, out_errors);
        valid &= AppendValidProcedures(out_prepared_set, flags, out_errors);

        // Pick main stay
        {
            const mco_PreparedStay *main_prep;
            if (mono_stays.len > 1) {
                main_prep = FindMainStay(out_prepared_set->mono_preps, out_prepared_set->prep.duration);

                out_prepared_set->stay.main_diagnosis = main_prep->main_diag_info->diag;
                if (main_prep->linked_diag_info) {
                    out_prepared_set->stay.linked_diagnosis = main_prep->linked_diag_info->diag;
                } else {
                    out_prepared_set->stay.linked_diagnosis = {};
                }
            } else {
                main_prep = &out_prepared_set->mono_preps[0];
            }

            out_prepared_set->main_prep = main_prep;
            out_prepared_set->prep.main_diag_info = main_prep->main_diag_info;
            out_prepared_set->prep.linked_diag_info = main_prep->linked_diag_info;
        }
    }

    // Some of these checks require diagnosis and/or procedure information
    valid &= CheckAggregateErrors(out_prepared_set->prep, out_prepared_set->mono_preps, out_errors);

    if (RG_UNLIKELY(!valid))
        return mco_GhmCode::FromString("90Z00Z");

    return {};
}

static int ExecuteGhmTest(RunGhmTreeContext &ctx, const mco_GhmDecisionNode &ghm_node,
                          mco_ErrorSet *out_errors)
{
    RG_ASSERT_DEBUG(ghm_node.type == mco_GhmDecisionNode::Type::Test);

    switch (ghm_node.u.test.function) {
        case 0:
        case 1: {
            return GetDiagnosisByte(ctx.stay->sex, *ctx.main_diag_info,
                                    ghm_node.u.test.params[0]);
        } break;

        case 2: {
            for (const mco_ProcedureInfo *proc_info: ctx.prep->procedures) {
                if (TestProcedure(*proc_info, ghm_node.u.test.params[0], ghm_node.u.test.params[1]))
                    return 1;
            }
            return 0;
        } break;

        case 3: {
            if (ghm_node.u.test.params[1] == 1) {
                return (ctx.prep->age_days > ghm_node.u.test.params[0]);
            } else {
                return (ctx.prep->age > ghm_node.u.test.params[0]);
            }
        } break;

        case 5: {
            return TestDiagnosis(ctx.stay->sex, *ctx.main_diag_info,
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
        } break;

        case 6: {
            // NOTE: Incomplete, should behave differently when params[0] >= 128,
            // but it's probably relevant only for FG 9 and 10 (CMAs)
            for (const mco_DiagnosisInfo *diag_info: ctx.prep->diagnoses) {
                if (TestDiagnosis(ctx.stay->sex, *diag_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]) &&
                        diag_info != ctx.main_diag_info && diag_info != ctx.linked_diag_info)
                    return 1;
            }
            return 0;
        } break;

        case 7: {
            for (const mco_DiagnosisInfo *diag_info: ctx.prep->diagnoses) {
                if (TestDiagnosis(ctx.stay->sex, *diag_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]))
                    return 1;
            }
            return 0;
        } break;

        case 9: {
            int result = 0;
            for (const mco_ProcedureInfo *proc_info: ctx.prep->procedures) {
                if (proc_info->bytes[0] & 0x80) {
                    if (TestProcedure(*proc_info,
                                      ghm_node.u.test.params[0], ghm_node.u.test.params[1])) {
                        result = 1;
                    } else {
                        return 0;
                    }
                }
            }
            return result;
        } break;

        case 10: {
            Size matches = 0;
            // ctx.info->procedures is always sorted (when RunGhmTree() is expected
            // to run on it) but not always deduplicated, that's why we need to check
            // against prev_proc_info.
            const mco_ProcedureInfo *prev_proc_info = nullptr;
            for (const mco_ProcedureInfo *proc_info: ctx.prep->procedures) {
                if (TestProcedure(*proc_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]) &&
                        proc_info != prev_proc_info) {
                    matches++;
                    if (matches >= 2)
                        return 1;
                }
                prev_proc_info = proc_info;
            }
            return 0;
        } break;

        case 13: {
            uint8_t diag_byte = GetDiagnosisByte(ctx.stay->sex, *ctx.main_diag_info,
                                                 ghm_node.u.test.params[0]);
            return (diag_byte == ghm_node.u.test.params[1]);
        } break;

        case 14: {
            return ((int)ctx.stay->sex == ghm_node.u.test.params[0] - 48);
        } break;

        case 18: {
            // This test is rare, we can afford a few allocations
            HashSet<drd_DiagnosisCode> handled_codes;
            Size special_matches = 0;
            for (const mco_DiagnosisInfo *diag_info: ctx.prep->diagnoses) {
                if (TestDiagnosis(ctx.stay->sex, *diag_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]) &&
                        handled_codes.Append(diag_info->diag).second) {
                    special_matches += (diag_info == ctx.main_diag_info ||
                                        diag_info == ctx.linked_diag_info);
                    if (handled_codes.table.count >= 2 && handled_codes.table.count > special_matches)
                        return 1;
                }
            }
            return 0;
        } break;

        case 19: {
            switch (ghm_node.u.test.params[1]) {
                case 0: {
                    return (ctx.stay->exit.mode == '0' + ghm_node.u.test.params[0]);
                } break;
                case 1: {
                    return (ctx.stay->exit.destination == '0' + ghm_node.u.test.params[0]);
                } break;
                case 2: {
                    return (ctx.stay->entry.mode == '0' + ghm_node.u.test.params[0]);
                } break;
                case 3: {
                    return (ctx.stay->entry.origin == '0' + ghm_node.u.test.params[0]);
                } break;
            }
        } break;

        case 20: {
            return 0;
        } break;

        case 22: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.prep->duration < param);
        } break;

        case 26: {
            if (ctx.linked_diag_info) {
                return TestDiagnosis(ctx.stay->sex, *ctx.linked_diag_info,
                                     ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            } else {
                return 0;
            }
        } break;

        case 28: {
            SetError(out_errors, ghm_node.u.test.params[0]);
            return 0;
        } break;

        case 29: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.prep->duration == param);
        } break;

        case 30: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.stay->session_count == param);
        } break;

        case 33: {
            return !!(ctx.prep->proc_activities & (1 << ghm_node.u.test.params[0]));
        } break;

        case 34: {
            if (ctx.linked_diag_info && ctx.linked_diag_info == ctx.prep->linked_diag_info) {
                uint8_t cmd = ctx.linked_diag_info->Attributes(ctx.stay->sex).cmd;
                uint8_t jump = ctx.linked_diag_info->Attributes(ctx.stay->sex).jump;
                if (cmd || jump != 3) {
                    std::swap(ctx.main_diag_info, ctx.linked_diag_info);
                }
            }

            return 0;
        } break;

        case 35: {
            return (ctx.linked_diag_info != ctx.prep->linked_diag_info);
        } break;

        case 36: {
            for (const mco_DiagnosisInfo *diag_info: ctx.prep->diagnoses) {
                if (TestDiagnosis(ctx.stay->sex, *diag_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]) &&
                        diag_info != ctx.linked_diag_info)
                    return 1;
            }
            return 0;
        } break;

        case 38: {
            return (ctx.gnn >= ghm_node.u.test.params[0] &&
                    ctx.gnn <= ghm_node.u.test.params[1]);
        } break;

        case 39: {
            if (!ctx.gnn) {
                int gestational_age = ctx.stay->gestational_age;
                if (!gestational_age) {
                    gestational_age = 99;
                }

                for (const mco_ValueRangeCell<2> &cell: ctx.index->gnn_cells) {
                    if (cell.Test(0, ctx.stay->newborn_weight) && cell.Test(1, gestational_age)) {
                        ctx.gnn = cell.value;
                        break;
                    }
                }
            }

            return 0;
        } break;

        case 41: {
            for (const mco_DiagnosisInfo *diag_info: ctx.prep->diagnoses) {
                uint8_t cmd = diag_info->Attributes(ctx.stay->sex).cmd;
                uint8_t jump = diag_info->Attributes(ctx.stay->sex).jump;
                if (cmd == ghm_node.u.test.params[0] && jump == ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;

        case 42: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.stay->newborn_weight && ctx.stay->newborn_weight < param);
        } break;

        case 43: {
            for (const mco_DiagnosisInfo *diag_info: ctx.prep->diagnoses) {
                uint8_t cmd = diag_info->Attributes(ctx.stay->sex).cmd;
                uint8_t jump = diag_info->Attributes(ctx.stay->sex).jump;
                if (cmd == ghm_node.u.test.params[0] && jump == ghm_node.u.test.params[1] &&
                        diag_info != ctx.linked_diag_info)
                    return 1;
            }

            return 0;
        } break;
    }

    LogError("Unknown test %1 or invalid arguments", ghm_node.u.test.function);
    return -1;
}

static bool CheckConfirmation(const mco_PreparedStay &prep, mco_GhmCode ghm,
                              const mco_GhmRootInfo &ghm_root_info, mco_ErrorSet *out_errors)
{
    const mco_Stay &stay = *prep.stay;

    bool valid = true;

    bool confirm = false;
    if (RG_UNLIKELY(prep.duration >= 365)) {
        confirm = true;
    } else if (prep.duration < ghm_root_info.confirm_duration_treshold &&
               stay.exit.mode != '9' && stay.exit.mode != '0' &&
               (stay.exit.mode != '7' || stay.exit.destination != '1') &&
               !(stay.flags & (int)mco_Stay::Flag::RAAC)) {
        confirm = true;
    } else if (prep.markers & ((int)mco_PreparedStay::Marker::Childbirth |
                             (int)mco_PreparedStay::Marker::ChildbirthType)) {
        // I don't really know the rational behind these tests, to be honest. It's
        // what the official classifier does.
        switch (ghm.parts.cmd) {
            case 12:
            case 14:
            case 22:
            case 25:
            case 26:
            case 27: { /* No need */ } break;

            case 1: {
                confirm |= !((ghm.parts.type == 'C' && ghm.parts.seq == 3) ||
                             (ghm.parts.type == 'C' && ghm.parts.seq == 4) ||
                             (ghm.parts.type == 'C' && ghm.parts.seq == 5) ||
                             (ghm.parts.type == 'C' && ghm.parts.seq == 6) ||
                             (ghm.parts.type == 'C' && ghm.parts.seq == 10) ||
                             (ghm.parts.type == 'C' && ghm.parts.seq == 11) ||
                             (ghm.parts.type == 'C' && ghm.parts.seq == 12) ||
                             (ghm.parts.type == 'K' && ghm.parts.seq == 7) ||
                             (ghm.parts.type == 'M' && ghm.parts.seq == 13) ||
                             (ghm.parts.type == 'M' && ghm.parts.seq == 18) ||
                             (ghm.parts.type == 'M' && ghm.parts.seq == 19) ||
                             (ghm.parts.type == 'M' && ghm.parts.seq == 24) ||
                             (ghm.parts.type == 'M' && ghm.parts.seq == 25) ||
                             (ghm.parts.type == 'M' && ghm.parts.seq == 30) ||
                             (ghm.parts.type == 'M' && ghm.parts.seq == 31));
            } break;
            case 7: {
                confirm |= !(ghm.parts.type == 'C' &&
                             ghm.parts.seq >= 9 && ghm.parts.seq <= 14);
            } break;
            case 23: {
                confirm |= !(ghm.parts.type == 'Z' && ghm.parts.seq == 2);
            } break;

            default: { confirm = true; } break;
        }
    }

    if (stay.flags & (int)mco_Stay::Flag::Confirmed) {
        if (confirm) {
            SetError(out_errors, 223, 0);
        } else if (prep.duration >= ghm_root_info.confirm_duration_treshold) {
            valid &= SetError(out_errors, 124);
        }
    } else if (confirm) {
        valid &= SetError(out_errors, 120);
    }

    return valid;
}

static bool CheckGhmErrors(const mco_PreparedStay &prep, Span<const mco_PreparedStay> mono_preps,
                           mco_GhmCode ghm, mco_ErrorSet *out_errors)
{
    const mco_Stay &stay = *prep.stay;

    bool valid = true;

    // Sessions
    if (ghm.parts.cmd == 28) {
        if (RG_UNLIKELY(mono_preps.len > 1)) {
            valid &= SetError(out_errors, 150);
        }
        if (RG_UNLIKELY(stay.exit.date >= Date(2016, 3, 1) &&
                        stay.main_diagnosis.Matches("Z511") &&
                        !stay.linked_diagnosis.IsValid())) {
            valid &= SetError(out_errors, 187);
        }
    }

    // Menstruation
    if (RG_UNLIKELY(ghm.parts.cmd == 14 &&
                    ghm.Root() != mco_GhmRootCode(14, 'C', 4) &&
                    ghm.Root() != mco_GhmRootCode(14, 'M', 2) &&
                    !stay.last_menstrual_period.value)) {
        valid &= SetError(out_errors, 162);
    }

    // Abortion
    if (RG_UNLIKELY(stay.exit.date >= Date(2016, 3, 1) && ghm.Root() == mco_GhmRootCode(14, 'Z', 8))) {
        bool type_present = std::any_of(prep.procedures.begin(), prep.procedures.end(),
                                        [](const mco_ProcedureInfo *proc_info) {
            static drd_ProcedureCode proc1 = drd_ProcedureCode::FromString("JNJD002");
            static drd_ProcedureCode proc2 = drd_ProcedureCode::FromString("JNJP001");

            return proc_info->proc == proc1 || proc_info->proc == proc2;
        });
        if (!type_present) {
            SetError(out_errors, 179, -1);
        }
    }

    return valid;
}

static mco_GhmCode RunGhmTree(const mco_TableIndex &index, const mco_PreparedStay &prep,
                              mco_ErrorSet *out_errors)
{
    mco_GhmCode ghm = {};

    RunGhmTreeContext ctx = {};
    ctx.index = &index;
    ctx.stay = prep.stay;
    ctx.prep = &prep;
    ctx.main_diag_info = prep.main_diag_info;
    ctx.linked_diag_info = prep.linked_diag_info;

    Size ghm_node_idx = 0;
    for (Size i = 0;; i++) {
        if (RG_UNLIKELY(i >= index.ghm_nodes.len)) {
            LogError("Empty GHM tree or infinite loop (%2)", index.ghm_nodes.len);
            SetError(out_errors, 4, 2);
            return mco_GhmCode::FromString("90Z03Z");
        }

        RG_ASSERT_DEBUG(ghm_node_idx < index.ghm_nodes.len);
        const mco_GhmDecisionNode &ghm_node = index.ghm_nodes[ghm_node_idx];

        switch (ghm_node.type) {
            case mco_GhmDecisionNode::Type::Test: {
                int function_ret = ExecuteGhmTest(ctx, ghm_node, out_errors);
                if (RG_UNLIKELY(function_ret < 0 || function_ret >= ghm_node.u.test.children_count)) {
                    LogError("Result for GHM tree test %1 out of range (%2 - %3)",
                             ghm_node.u.test.function, 0, ghm_node.u.test.children_count);
                    SetError(out_errors, 4, 2);
                    return mco_GhmCode::FromString("90Z03Z");
                }

                ghm_node_idx = ghm_node.u.test.children_idx + function_ret;
            } break;

            case mco_GhmDecisionNode::Type::Ghm: {
                ghm = ghm_node.u.ghm.ghm;
                if (ghm_node.u.ghm.error && out_errors) {
                    SetError(out_errors, ghm_node.u.ghm.error);
                }
                return ghm;
            } break;
        }
    }
}

int mco_GetMinimalDurationForSeverity(int severity)
{
    RG_ASSERT_DEBUG(severity >= 0 && severity < 4);
    return severity ? (severity + 2) : 0;
}

int mco_LimitSeverity(int severity, int duration)
{
    RG_ASSERT_DEBUG(severity >= 0 && severity < 4);
    return duration >= 3 ? std::min(duration - 2, severity) : 0;
}

static int LimitSeverity(const mco_PreparedStay &prep, const mco_GhmRootInfo &ghm_root_info,
                         int severity)
{
    bool raac = (prep.stay->flags & (int)mco_Stay::Flag::RAAC) && ghm_root_info.allow_raac;
    return raac ? severity : mco_LimitSeverity(severity, prep.duration);
}

bool mco_TestGhmRootExclusion(int8_t sex, const mco_DiagnosisInfo &cma_diag_info,
                              const mco_GhmRootInfo &ghm_root_info)
{
    RG_ASSERT(ghm_root_info.cma_exclusion_mask.offset < RG_SIZE(mco_DiagnosisInfo::attributes[0].raw));
    return (cma_diag_info.Attributes(sex).raw[ghm_root_info.cma_exclusion_mask.offset] &
            ghm_root_info.cma_exclusion_mask.value);
}

bool mco_TestDiagnosisExclusion(const mco_TableIndex &index,
                                const mco_DiagnosisInfo &cma_diag_info,
                                const mco_DiagnosisInfo &main_diag_info)
{
    RG_ASSERT(cma_diag_info.exclusion_set_idx < index.exclusions.len);
    const mco_ExclusionInfo *excl = &index.exclusions[cma_diag_info.exclusion_set_idx];
    if (RG_UNLIKELY(!excl))
        return false;

    RG_ASSERT(main_diag_info.cma_exclusion_mask.offset < RG_SIZE(excl->raw));
    return (excl->raw[main_diag_info.cma_exclusion_mask.offset] &
            main_diag_info.cma_exclusion_mask.value);
}

// Don't forget to update drdR::mco_exclusions() if this changes
bool mco_TestExclusion(const mco_TableIndex &index, int8_t sex, int age,
                       const mco_DiagnosisInfo &cma_diag_info,
                       const mco_GhmRootInfo &ghm_root_info,
                       const mco_DiagnosisInfo &main_diag_info,
                       const mco_DiagnosisInfo *linked_diag_info)
{
    if (age < cma_diag_info.Attributes(sex).cma_minimum_age)
        return true;
    if (cma_diag_info.Attributes(sex).cma_maximum_age &&
            age >= cma_diag_info.Attributes(sex).cma_maximum_age)
        return true;

    if (mco_TestGhmRootExclusion(sex, cma_diag_info, ghm_root_info))
        return true;

    if (mco_TestDiagnosisExclusion(index, cma_diag_info, main_diag_info))
        return true;
    if (linked_diag_info && mco_TestDiagnosisExclusion(index, cma_diag_info, *linked_diag_info))
        return true;

    return false;
}

static mco_GhmCode RunGhmSeverity(const mco_TableIndex &index, const mco_PreparedStay &prep,
                                  mco_GhmCode ghm, const mco_GhmRootInfo &ghm_root_info)
{
    const mco_Stay &stay = *prep.stay;

    // Ambulatory and / or short duration GHM
    if (ghm_root_info.allow_ambulatory && !prep.duration) {
        ghm.parts.mode = 'J';
    } else if (ghm_root_info.short_duration_treshold &&
               prep.duration < ghm_root_info.short_duration_treshold) {
        ghm.parts.mode = 'T';
    } else if (ghm.parts.mode >= 'A' && ghm.parts.mode < 'E') {
        int severity = ghm.parts.mode - 'A';

        if (ghm_root_info.childbirth_severity_list) {
            RG_ASSERT(ghm_root_info.childbirth_severity_list > 0 &&
                   ghm_root_info.childbirth_severity_list <= RG_SIZE(index.cma_cells));
            Span<const mco_ValueRangeCell<2>> cma_cells =
                index.cma_cells[ghm_root_info.childbirth_severity_list - 1];
            for (const mco_ValueRangeCell<2> &cell: cma_cells) {
                if (cell.Test(0, stay.gestational_age) && cell.Test(1, severity)) {
                    severity = cell.value;
                    break;
                }
            }
        }

        severity = LimitSeverity(prep, ghm_root_info, severity);

        ghm.parts.mode = (char)('A' + severity);
    } else if (!ghm.parts.mode) {
        int severity = 0;

        for (const mco_DiagnosisInfo *diag_info: prep.diagnoses) {
            if (diag_info == prep.main_diag_info || diag_info == prep.linked_diag_info)
                continue;

            // We wouldn't have gotten here if main_diagnosis was missing from the index
            int new_severity = diag_info->Attributes(stay.sex).severity;
            if (new_severity > severity) {
                bool excluded = mco_TestExclusion(index, stay.sex, prep.age,
                                                  *diag_info, ghm_root_info,
                                                  *prep.main_diag_info, prep.linked_diag_info);
                if (!excluded) {
                    severity = new_severity;
                }
            }
        }

        if (prep.age >= ghm_root_info.old_age_treshold &&
                severity < ghm_root_info.old_severity_limit) {
            severity++;
        } else if (prep.age < ghm_root_info.young_age_treshold &&
                   severity < ghm_root_info.young_severity_limit) {
            severity++;
        } else if (stay.exit.mode == '9' && !severity) {
            severity = 1;
        }

        severity = LimitSeverity(prep, ghm_root_info, severity);

        ghm.parts.mode = (char)('1' + severity);
    }

    return ghm;
}

mco_GhmCode mco_PickGhm(const mco_TableIndex &index,
                        const mco_PreparedStay &prep, Span<const mco_PreparedStay> mono_preps,
                        unsigned int flags, mco_ErrorSet *out_errors)
{
    mco_GhmCode ghm;

    ghm = RunGhmTree(index, prep, out_errors);

    const mco_GhmRootInfo *ghm_root_info = index.FindGhmRoot(ghm.Root());
    if (RG_UNLIKELY(!ghm_root_info)) {
        LogError("Unknown GHM root '%1'", ghm.Root());
        SetError(out_errors, 4, 2);
        return mco_GhmCode::FromString("90Z03Z");
    }

    if (RG_UNLIKELY(!CheckGhmErrors(prep, mono_preps, ghm, out_errors)))
        return mco_GhmCode::FromString("90Z00Z");
    if (RG_UNLIKELY(!(flags & (int)mco_ClassifyFlag::IgnoreConfirmation) &&
                    !CheckConfirmation(prep, ghm, *ghm_root_info, out_errors)))
        return mco_GhmCode::FromString("90Z00Z");

    ghm = RunGhmSeverity(index, prep, ghm, *ghm_root_info);

    return ghm;
}

static bool TestGhs(const mco_PreparedStay &prep, Span<const mco_PreparedStay> mono_preps,
                    const mco_AuthorizationSet &authorization_set,
                    const mco_GhmToGhsInfo &ghm_to_ghs_info)
{
    const mco_Stay &stay = *prep.stay;

    if (ghm_to_ghs_info.minimum_age && prep.age < ghm_to_ghs_info.minimum_age)
        return false;

    int duration;
    if (ghm_to_ghs_info.unit_authorization) {
        duration = 0;
        bool authorized = false;
        for (const mco_PreparedStay &mono_prep: mono_preps) {
            const mco_Stay &mono_stay = *mono_prep.stay;
            if (authorization_set.TestAuthorization(mono_stay.unit, mono_stay.exit.date,
                                                    ghm_to_ghs_info.unit_authorization)) {
                duration += std::max((int16_t)1, mono_prep.duration);
                authorized = true;
            }
        }
        if (!authorized)
            return false;
    } else {
        duration = prep.duration;
    }
    if (ghm_to_ghs_info.bed_authorization) {
        bool test = std::any_of(mono_preps.begin(), mono_preps.end(),
                                [&](const mco_PreparedStay &mono_prep) {
            return mono_prep.stay->bed_authorization == ghm_to_ghs_info.bed_authorization;
        });
        if (!test)
            return false;
    }
    if (ghm_to_ghs_info.minimum_duration && duration < ghm_to_ghs_info.minimum_duration)
        return false;
    switch (ghm_to_ghs_info.special_mode) {
        case mco_GhmToGhsInfo::SpecialMode::None: {} break;
        case mco_GhmToGhsInfo::SpecialMode::Diabetes: {
            if (prep.duration < ghm_to_ghs_info.special_duration &&
                    stay.entry.mode == '8' && stay.entry.origin != '5' && stay.exit.mode == '8' &&
                    (TestDiagnosis(stay.sex, *prep.main_diag_info, 32, 0x20) ||
                     (prep.linked_diag_info && TestDiagnosis(stay.sex, *prep.linked_diag_info, 32, 0x20)))) {
                bool auth62 = std::any_of(mono_preps.begin(), mono_preps.end(),
                                          [&](const mco_PreparedStay &mono_prep) {
                    const mco_Stay &mono_stay = *mono_prep.stay;
                    return authorization_set.TestAuthorization(mono_stay.unit, mono_stay.exit.date, 62);
                });

                if (auth62)
                    return false;
            }
        } break;
    }

    if (ghm_to_ghs_info.main_diagnosis_mask.value) {
        if (!TestDiagnosis(stay.sex, *prep.main_diag_info, ghm_to_ghs_info.main_diagnosis_mask))
            return false;
    }
    if (ghm_to_ghs_info.diagnosis_mask.value) {
        bool test = std::any_of(prep.diagnoses.begin(), prep.diagnoses.end(),
                                [&](const mco_DiagnosisInfo *diag_info) {
            return TestDiagnosis(stay.sex, *diag_info, ghm_to_ghs_info.diagnosis_mask);
        });
        if (!test)
            return false;
    }
    for (const drd_ListMask &mask: ghm_to_ghs_info.procedure_masks) {
        bool test = std::any_of(prep.procedures.begin(), prep.procedures.end(),
                                [&](const mco_ProcedureInfo *proc_info) {
            return TestProcedure(*proc_info, mask);
        });
        if (!test)
            return false;
    }

    return true;
}

mco_GhsCode mco_PickGhs(const mco_TableIndex &index, const mco_AuthorizationSet &authorization_set,
                        drd_Sector sector, const mco_PreparedStay &prep,
                        Span<const mco_PreparedStay> mono_preps, mco_GhmCode ghm,
                        unsigned int /*flags*/, int16_t *out_ghs_duration)
{
    const mco_Stay &stay = *prep.stay;

    mco_GhsCode ghs = mco_GhsCode(9999);
    int ghs_duration = prep.duration;

    if (RG_LIKELY(ghm.IsValid() && !ghm.IsError())) {
        // Deal with UHCD-only stays
        if (prep.duration > 0 && stay.entry.mode == '8' && stay.exit.mode == '8') {
            bool uhcd = std::all_of(mono_preps.begin(), mono_preps.end(),
                                    [&](const mco_PreparedStay &mono_prep) {
                const mco_Stay &mono_stay = *mono_prep.stay;
                int8_t auth_type = authorization_set.GetAuthorizationType(mono_stay.unit,
                                                                          mono_stay.exit.date);
                return (auth_type == 7);
            });

            if (uhcd) {
                ghs_duration = 0;

                mco_PreparedStay prep0 = prep;
                prep0.duration = 0;

                // Don't run ClassifyGhm() because that would test the confirmation flag,
                // which makes no sense when duration is forced to 0.
                ghm = RunGhmTree(index, prep0, nullptr);
                const mco_GhmRootInfo *ghm_root_info = index.FindGhmRoot(ghm.Root());
                if (RG_LIKELY(ghm_root_info)) {
                    ghm = RunGhmSeverity(index, prep0, ghm, *ghm_root_info);
                }
            }
        }

        Span<const mco_GhmToGhsInfo> compatible_ghs = index.FindCompatibleGhs(ghm);

        for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
            if (TestGhs(prep, mono_preps, authorization_set, ghm_to_ghs_info)) {
                ghs = ghm_to_ghs_info.Ghs(sector);
                break;
            }
        }
    }

    if (out_ghs_duration) {
        *out_ghs_duration = ghs_duration;
    }
    return ghs;
}

static bool TestSupplementRea(const mco_PreparedStay &prep, const mco_PreparedStay &mono_prep,
                              Size list2_treshold)
{
    if (mono_prep.stay->igs2 >= 15 || prep.age < 18) {
        Size list2_matches = 0;
        for (const mco_ProcedureInfo *proc_info: mono_prep.procedures) {
            if (proc_info->bytes[27] & 0x10)
                return true;
            if (proc_info->bytes[27] & 0x8) {
                list2_matches++;
                if (list2_matches >= list2_treshold)
                    return true;
            }
        }
    }

    return false;
}

static bool TestSupplementSrc(const mco_TableIndex &index, const mco_PreparedStay &prep,
                              const mco_PreparedStay &mono_prep, int16_t igs2_src_adjust,
                              bool prev_reanimation, const mco_PreparedStay *prev_mono_prep)
{
    const mco_Stay &stay = *prep.stay;

    if (prev_reanimation)
        return true;
    if (prep.age >= 18 && mono_prep.stay->igs2 - igs2_src_adjust >= 15)
        return true;

    HeapArray<drd_ProcedureCode> src_procedures;

    if (mono_prep.stay->igs2 - igs2_src_adjust >= 7 || prep.age < 18) {
        for (const mco_DiagnosisInfo *diag_info: mono_prep.diagnoses) {
            if (diag_info->Attributes(stay.sex).raw[21] & 0x10)
                return true;
            if (diag_info->Attributes(stay.sex).raw[21] & 0x8) {
                const mco_SrcPair *pair = index.src_pairs_map[0]->FindValue(diag_info->diag, nullptr);
                if (pair) {
                    do {
                        src_procedures.Append(pair->proc);
                    } while (++pair < index.src_pairs[0].end() && pair->diag == diag_info->diag);
                }
            }
        }
    }
    if (prep.age < 18) {
        for (const mco_DiagnosisInfo *diag_info: mono_prep.diagnoses) {
            if (diag_info->Attributes(stay.sex).raw[22] & 0x80)
                return true;
            if (diag_info->Attributes(stay.sex).raw[22] & 0x40) {
                const mco_SrcPair *pair = index.src_pairs_map[1]->FindValue(diag_info->diag, nullptr);
                if (pair) {
                    do {
                        src_procedures.Append(pair->proc);
                    } while (++pair < index.src_pairs[1].end() && pair->diag == diag_info->diag);
                }
            }
        }
    }
    for (const mco_ProcedureInfo *proc_info: mono_prep.procedures) {
        for (drd_ProcedureCode diag_proc: src_procedures) {
            if (diag_proc == proc_info->proc)
                return true;
        }
    }

    for (const mco_ProcedureInfo *proc_info: mono_prep.procedures) {
        if (proc_info->bytes[38] & 0x1)
            return true;
    }
    if (prev_mono_prep) {
        for (const mco_ProcedureInfo *proc_info: prev_mono_prep->procedures) {
            if (proc_info->bytes[38] & 0x1)
                return true;
        }
    }

    return false;
}

// TODO: Count correctly when authorization date is too early (REA)
void mco_CountSupplements(const mco_TableIndex &index, const mco_AuthorizationSet &authorization_set,
                          const mco_PreparedStay &prep, Span<const mco_PreparedStay> mono_preps,
                          mco_GhmCode ghm, mco_GhsCode ghs, unsigned int /*flags*/,
                          mco_SupplementCounters<int16_t> *out_counters,
                          Strider<mco_SupplementCounters<int16_t>> out_mono_counters)
{
    const mco_Stay &stay = *prep.stay;

    if (ghs == mco_GhsCode(9999))
        return;

    int16_t igs2_src_adjust;
    if (prep.age >= 80) {
        igs2_src_adjust = 18;
    } else if (prep.age >= 75) {
        igs2_src_adjust = 16;
    } else if (prep.age >= 70) {
        igs2_src_adjust = 15;
    } else if (prep.age >= 60) {
        igs2_src_adjust = 12;
    } else if (prep.age >= 40) {
        igs2_src_adjust = 7;
    } else {
        igs2_src_adjust = 0;
    }
    bool prev_reanimation = (stay.entry.mode == '7' && stay.entry.origin == 'R');

    bool test_ohb = (ghm != mco_GhmCode(28, 'Z', 15, 'Z'));
    bool test_aph = (ghm != mco_GhmCode(28, 'Z', 16, 'Z'));
    bool test_dia = (ghm != mco_GhmCode(28, 'Z', 1, 'Z') &&
                     ghm != mco_GhmCode(28, 'Z', 2, 'Z') &&
                     ghm != mco_GhmCode(28, 'Z', 3, 'Z') &&
                     ghm != mco_GhmCode(28, 'Z', 4, 'Z') &&
                     ghm != mco_GhmCode(11, 'K', 2, 'J'));
    bool test_ent3 = (stay.exit.date >= Date(2014, 3, 1) && test_dia);
    bool test_sdc = (stay.exit.date >= Date(2017, 3, 1) && ghm.Root() != mco_GhmRootCode(5, 'C', 19));

    Size ambu_stay_idx = -1;
    int ambu_priority = 0;
    int16_t ambu_type = -1;

    const auto AddToCounter = [&](Size stay_idx, int type, int count) {
        out_counters->values[type] += count;
        if (out_mono_counters.ptr) {
            out_mono_counters[stay_idx].values[type] += count;
        }
    };

    for (Size i = 0; i < mono_preps.len; i++) {
        const mco_PreparedStay &mono_prep = mono_preps[i];
        const mco_Stay &mono_stay = *mono_prep.stay;

        int8_t auth_type = authorization_set.GetAuthorizationType(mono_stay.unit, mono_stay.exit.date);
        const mco_AuthorizationInfo *auth_info =
            index.FindAuthorization(mco_AuthorizationScope::Unit, auth_type);

        bool reanimation = false;
        int type = -1;
        int priority = 0;
        switch (auth_info ? auth_info->function : 0) {
            case 1: {
                if (RG_LIKELY(prep.age < 2) && ghs != mco_GhsCode(5903)) {
                    type = (int)mco_SupplementType::Nn1;
                    priority = 1;
                }
            } break;

            case 2: {
                if (RG_LIKELY(prep.age < 2) && ghs != mco_GhsCode(5903)) {
                    type = (int)mco_SupplementType::Nn2;
                    priority = 3;
                }
            } break;

            case 3: {
                if (RG_LIKELY(prep.age < 2) && ghs != mco_GhsCode(5903)) {
                    if (TestSupplementRea(prep, mono_prep, 1)) {
                        type = (int)mco_SupplementType::Nn3;
                        priority = 6;
                        reanimation = true;
                    } else {
                        type = (int)mco_SupplementType::Nn2;
                        priority = 3;
                    }
                }
            } break;

            case 4: {
                if (TestSupplementRea(prep, mono_prep, 3)) {
                    type = (int)mco_SupplementType::Rea;
                    priority = 7;
                    reanimation = true;
                } else {
                    type = (int)mco_SupplementType::Reasi;
                    priority = 5;
                }
            } break;

            case 6: {
                const mco_PreparedStay *prev_mono_prep = i ? &mono_preps[i - 1] : nullptr;
                if (TestSupplementSrc(index, prep, mono_prep, igs2_src_adjust,
                                      prev_reanimation, prev_mono_prep)) {
                    type = (int)mco_SupplementType::Src;
                    priority = 2;
                }
            } break;

            case 8: {
                type = (int)mco_SupplementType::Si;
                priority = 4;
            } break;

            case 9: {
                if (ghs != mco_GhsCode(5903)) {
                    if (prep.age < 18) {
                        if (TestSupplementRea(prep, mono_prep, 1)) {
                            type = (int)mco_SupplementType::Rep;
                            priority = 8;
                            reanimation = true;
                        } else {
                            type = (int)mco_SupplementType::Reasi;
                            priority = 5;
                        }
                    } else {
                        if (TestSupplementRea(prep, mono_prep, 3)) {
                            type = (int)mco_SupplementType::Rea;
                            priority = 7;
                            reanimation = true;
                        } else {
                            type = (int)mco_SupplementType::Reasi;
                            priority = 5;
                        }
                    }
                }
            } break;
        }

        prev_reanimation = reanimation;

        if (mono_prep.duration) {
            if (ambu_stay_idx >= 0 && ambu_priority >= priority) {
                if (type >= 0) {
                    int16_t days = mono_prep.duration + (mono_stay.exit.mode == '9') - 1;
                    AddToCounter(i, type, days);
                }
                AddToCounter(ambu_stay_idx, ambu_type, 1);
            } else if (type >= 0) {
                int16_t days = mono_prep.duration + (mono_stay.exit.mode == '9');
                AddToCounter(i, type, days);
            }
            ambu_stay_idx = -1;
            ambu_priority = 0;
        } else if (priority > ambu_priority) {
            ambu_stay_idx = i;
            ambu_priority = priority;
            ambu_type = type;
        }

        for (const mco_ProcedureInfo *proc_info: mono_prep.procedures) {
            AddToCounter(i, (int)mco_SupplementType::Ohb, test_ohb && (proc_info->bytes[31] & 0x20));
            AddToCounter(i, (int)mco_SupplementType::Aph, test_aph && (proc_info->bytes[38] & 0x8));
            AddToCounter(i, (int)mco_SupplementType::Rap, prep.age < 18 && ((proc_info->bytes[27] & 0x80) |
                                                                            (proc_info->bytes[22] & 0x4) |
                                                                            (proc_info->bytes[39] & 0x10) |
                                                                            (proc_info->bytes[41] & 0xF0) |
                                                                            (proc_info->bytes[40] & 0x7)));
            AddToCounter(i, (int)mco_SupplementType::Dia, test_dia && !!(proc_info->bytes[32] & 0x2));
            AddToCounter(i, (int)mco_SupplementType::Ent1, test_dia && !!(proc_info->bytes[23] & 0x1));
            AddToCounter(i, (int)mco_SupplementType::Ent2, test_dia && !!(proc_info->bytes[24] & 0x80));
            AddToCounter(i, (int)mco_SupplementType::Ent3, test_ent3 && !!(proc_info->bytes[30] & 0x4));
            if (test_sdc && (proc_info->bytes[24] & 0x2)) {
                AddToCounter(i, (int)mco_SupplementType::Sdc, 1);
                test_sdc = false;
            }
        }
    }
    if (ambu_stay_idx >= 0) {
        AddToCounter(ambu_stay_idx, ambu_type, 1);
    }

    if (prep.markers & (int)mco_PreparedStay::Marker::ChildbirthProcedure) {
        bool ant_diag = std::any_of(prep.diagnoses.begin(), prep.diagnoses.end(),
                                      [&](const mco_DiagnosisInfo *diag_info) {
            return diag_info->Attributes(stay.sex).raw[25] & 0x40;
        });

        if (ant_diag) {
            int16_t ant_days = prep.childbirth_date - stay.entry.date - 2;
            for (Size i = 0; ant_days > 0; i++) {
                int mono_ant_days = std::min(mono_preps[i].duration, ant_days);
                AddToCounter(i, (int)mco_SupplementType::Ant, mono_ant_days);
                ant_days -= mono_ant_days;
            }
        }
    }

    AddToCounter(0, (int)mco_SupplementType::Dip, stay.dip_count);
}

static mco_Stay FixMonoStayForClassifier(mco_Stay mono_stay)
{
    mono_stay.entry.mode = '8';
    mono_stay.entry.origin = 0;
    mono_stay.exit.mode = '8';
    mono_stay.exit.destination = 0;

    return mono_stay;
}

static Size RunClassifier(const mco_TableSet &table_set,
                          const mco_AuthorizationSet &authorization_set, drd_Sector sector,
                          Span<const mco_Stay> mono_stays, unsigned int flags,
                          mco_Result out_results[], Strider<mco_Result> out_mono_results)
{
    // Reuse for performance
    mco_PreparedSet prepared_set;
    mco_ErrorSet errors;
    mco_ErrorSet mono_errors;

    Size i = 0, j = 0;
    while (mono_stays.len) {
        mco_Result result = {};

        // Prepare
        errors.main_error = 0;
        result.stays = mco_Split(mono_stays, 1, &mono_stays);
        result.ghm = mco_Prepare(table_set, result.stays, flags, &prepared_set, &errors);
        result.index = prepared_set.index;
        result.age = prepared_set.prep.age;
        result.duration = prepared_set.prep.duration;
        result.sector = sector;

        // Classify GHM
        if (RG_LIKELY(!result.ghm.IsError())) {
            result.main_stay_idx = (int16_t)(prepared_set.main_prep - prepared_set.mono_preps.ptr);
            result.ghm = mco_PickGhm(*prepared_set.index,
                                     prepared_set.prep, prepared_set.mono_preps, flags, &errors);
        }
        result.main_error = errors.main_error;
        RG_ASSERT_DEBUG(result.ghm.IsValid());

        // Classify GHS
        result.ghs = mco_PickGhs(*prepared_set.index, authorization_set, sector,
                                 prepared_set.prep, prepared_set.mono_preps, result.ghm,
                                 flags, &result.ghs_duration);

        if (out_mono_results.IsValid()) {
            Strider<mco_SupplementCounters<int16_t>> mono_supplement_days =
                MakeStrider(&out_mono_results[j].supplement_days, out_mono_results.stride);

            // Perform mono-stay classifications
            if (result.stays.len == 1) {
                out_mono_results[j] = result;
            } else {
                for (Size k = 0; k < result.stays.len; k++) {
                    mco_PreparedStay &mono_prep = prepared_set.mono_preps[k];
                    mco_Result *mono_result = &out_mono_results[j + k];

                    *mono_result = {};
                    mono_result->stays = *mono_prep.stay;
                    mono_result->main_stay_idx = 0;
                    mono_result->index = prepared_set.index;
                    mono_result->age = mono_prep.age;
                    mono_result->duration = mono_prep.duration;
                    mono_result->sector = sector;

                    if (!result.ghm.IsError()) {
                        // Some tests in RunGhmTree() need this to avoid counting duplicates,
                        // it's done for the global prep in AppendValidProcedures(),
                        // but not for individual mono_preps for performance reason.
                        std::sort(mono_prep.procedures.begin(), mono_prep.procedures.end());

                        int mono_flags = flags | (int)mco_ClassifyFlag::IgnoreConfirmation;

                        mono_errors.main_error = 0;
                        if (flags & (int)mco_ClassifyFlag::MonoOriginalStay) {
                            mono_result->ghm = RunGhmTree(*prepared_set.index, mono_prep, &mono_errors);
                            const mco_GhmRootInfo *ghm_root_info =
                                prepared_set.index->FindGhmRoot(mono_result->ghm.Root());
                            if (RG_LIKELY(ghm_root_info)) {
                                mono_result->ghm = RunGhmSeverity(*prepared_set.index, mono_prep,
                                                                  mono_result->ghm, *ghm_root_info);
                            }
                            mono_result->ghs = mco_PickGhs(*prepared_set.index, authorization_set,
                                                           sector, mono_prep, mono_prep, mono_result->ghm,
                                                           mono_flags, &mono_result->ghs_duration);
                        } else {
                            RG_DEFER_C(prev_stay = mono_prep.stay) { mono_prep.stay = prev_stay; };
                            mco_Stay fixed_mono_stay = FixMonoStayForClassifier(*mono_prep.stay);
                            mono_prep.stay = &fixed_mono_stay;

                            mono_result->ghm = mco_PickGhm(*prepared_set.index, mono_prep,
                                                           mono_prep, mono_flags, &mono_errors);
                            mono_result->ghs = mco_PickGhs(*prepared_set.index, authorization_set,
                                                           sector, mono_prep, mono_prep, mono_result->ghm,
                                                           mono_flags, &mono_result->ghs_duration);
                        }
                        mono_result->main_error = mono_errors.main_error;
                    } else {
                        mono_result->ghs = mco_GhsCode(9999);
                    }
                }
            }

            // Count supplements days
            mco_CountSupplements(*prepared_set.index, authorization_set,
                                 prepared_set.prep, prepared_set.mono_preps, result.ghm, result.ghs,
                                 flags, &result.supplement_days, mono_supplement_days);
        } else {
            // Count supplements days
            mco_CountSupplements(*prepared_set.index, authorization_set,
                                 prepared_set.prep, prepared_set.mono_preps, result.ghm, result.ghs,
                                 flags, &result.supplement_days);
        }

        // Commit result
        out_results[i++] = result;
        j += result.stays.len;
    }

    return i;
}

Size mco_Classify(const mco_TableSet &table_set, const mco_AuthorizationSet &authorization_set,
                  drd_Sector sector, Span<const mco_Stay> mono_stays, unsigned int flags,
                  HeapArray<mco_Result> *out_results, HeapArray<mco_Result> *out_mono_results)
{
    static const int task_size = 2048;

    // Pessimistic assumption (no multi-stay), but we cannot resize the
    // buffer as we go because the worker threads will fill it directly.
    out_results->Grow(mono_stays.len);
    if (out_mono_results) {
        out_mono_results->Grow(mono_stays.len);
    }

    const auto run_classifier_task = [&](Span<const mco_Stay> task_stays, Size results_offset) {
        mco_Result *task_results = out_results->ptr + results_offset;

        if (out_mono_results) {
            mco_Result *task_mono_results = out_mono_results->end() +
                                            (task_stays.ptr - mono_stays.ptr);
            return RunClassifier(table_set, authorization_set, sector, task_stays, flags,
                                 task_results, task_mono_results);
        } else {
            return RunClassifier(table_set, authorization_set, sector, task_stays, flags,
                                 task_results, nullptr);
        }
    };

    Size results_count;
    // Counting results on the main thread costs us some performance. If the caller is
    // already parallelizing (drdR for example) don't do it.
    if (!Async::IsTaskRunning()) {
        if (!mono_stays.len)
            return 0;

        Async async;

        results_count = 1;
        Size results_offset = out_results->len;
        Span<const mco_Stay> task_stays = mono_stays[0];
        for (Size i = 1; i < mono_stays.len; i++) {
            if (mco_SplitTest(mono_stays[i - 1].bill_id, mono_stays[i].bill_id)) {
                if (results_count % task_size == 0) {
                    async.AddTask([&, task_stays, results_offset]() mutable {
                        run_classifier_task(task_stays, results_offset);
                        return true;
                    });
                    results_offset += task_size;
                    task_stays = MakeSpan(&mono_stays[i], 0);
                }
                results_count++;
            }
            task_stays.len++;
        }
        async.AddTask([&, task_stays, results_offset]() mutable {
            run_classifier_task(task_stays, results_offset);
            return true;
        });

        async.Sync();
    } else {
        results_count = run_classifier_task(mono_stays, out_results->len);
    }

    out_results->len += results_count;
    if (out_mono_results) {
        out_mono_results->len += mono_stays.len;
    }

    return results_count;
}

}
