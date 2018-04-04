// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "a_classifier.hh"
#include "d_stays.hh"
#include "d_tables.hh"

struct RunGhmTreeContext {
    const ClassifyAggregate *agg;

    // Keep a copy for DP - DR reversal (function 34)
    const DiagnosisInfo *main_diagnosis;
    const DiagnosisInfo *linked_diagnosis;
    int gnn;
};

static int ComputeAge(Date date, Date birthdate)
{
    int age = date.st.year - birthdate.st.year;
    age -= (date.st.month < birthdate.st.month ||
            (date.st.month == birthdate.st.month && date.st.day < birthdate.st.day));
    return age;
}

static inline uint8_t GetDiagnosisByte(int8_t sex, const DiagnosisInfo &diag_info, uint8_t byte_idx)
{
    Assert(byte_idx < SIZE(DiagnosisInfo::attributes[0].raw));
    return diag_info.Attributes(sex).raw[byte_idx];
}
static inline uint8_t GetDiagnosisByte(const TableIndex &index, int8_t sex,
                                       DiagnosisCode diag, uint8_t byte_idx)
{
    const DiagnosisInfo *diag_info = index.FindDiagnosis(diag);
    if (UNLIKELY(!diag_info))
        return 0;

    return GetDiagnosisByte(sex, *diag_info, byte_idx);
}

static inline bool TestDiagnosis(int8_t sex, const DiagnosisInfo &diag_info, ListMask mask)
{
    DebugAssert(mask.offset >= 0 && mask.offset <= UINT8_MAX);
    return GetDiagnosisByte(sex, diag_info, (uint8_t)mask.offset) & mask.value;
}
static inline bool TestDiagnosis(int8_t sex, const DiagnosisInfo &diag_info,
                                 uint8_t offset, uint8_t value)
{
    return GetDiagnosisByte(sex, diag_info, offset) & value;
}
static inline bool TestDiagnosis(const TableIndex &index, int8_t sex,
                                 DiagnosisCode diag, ListMask mask)
{
    DebugAssert(mask.offset >= 0 && mask.offset <= UINT8_MAX);
    return GetDiagnosisByte(index, sex, diag, (uint8_t)mask.offset) & mask.value;
}
static inline bool TestDiagnosis(const TableIndex &index, int8_t sex,
                                 DiagnosisCode diag, uint8_t offset, uint8_t value)
{
    return GetDiagnosisByte(index, sex, diag, offset) & value;
}

static inline uint8_t GetProcedureByte(const ProcedureInfo &proc_info, int16_t byte_idx)
{
    Assert(byte_idx >= 0 && byte_idx < SIZE(ProcedureInfo::bytes));
    return proc_info.bytes[byte_idx];
}
static inline uint8_t GetProcedureByte(const TableIndex &index, Date exit_date,
                                       const ProcedureRealisation &proc, int16_t byte_idx)
{
    const ProcedureInfo *proc_info = index.FindProcedure(proc.proc, proc.phase, exit_date);
    if (UNLIKELY(!proc_info))
        return 0;

    return GetProcedureByte(*proc_info, byte_idx);
}

static inline bool TestProcedure(const ProcedureInfo &proc_info, ListMask mask)
{
    return GetProcedureByte(proc_info, mask.offset) & mask.value;
}
static inline bool TestProcedure(const ProcedureInfo &proc_info, int16_t offset, uint8_t value)
{
    return GetProcedureByte(proc_info, offset) & value;
}
static inline bool TestProcedure(const TableIndex &index, Date exit_date,
                                 const ProcedureRealisation &proc, ListMask mask)
{
    return GetProcedureByte(index, exit_date, proc, mask.offset) & mask.value;
}
static inline bool TestProcedure(const TableIndex &index, Date exit_date,
                                 const ProcedureRealisation &proc, int16_t offset, uint8_t value)
{
    return GetProcedureByte(index, exit_date, proc, offset) & value;
}

Span<const Stay> Cluster(Span<const Stay> stays, Span<const Stay> *out_remainder)
{
    DebugAssert(stays.len > 0);

    Size agg_len = 1;
    if (LIKELY(!(stays[0].error_mask & (int)Stay::Error::UnknownRumVersion) && stays[0].bill_id)) {
        while (agg_len < stays.len && stays[agg_len].bill_id == stays[agg_len - 1].bill_id) {
            agg_len++;
        }
    }

    if (out_remainder) {
        *out_remainder = stays.Take(agg_len, stays.len - agg_len);
    }
    return stays.Take(0, agg_len);
}

static const Stay *FindMainStay(const TableIndex &index, Span<const Stay> stays, int duration)
{
    DebugAssert(duration >= 0);

    int max_duration = -1;
    const Stay *zx_stay = nullptr;
    int zx_duration = -1;
    int proc_priority = 0;
    const Stay *trauma_stay = nullptr;
    const Stay *last_trauma_stay = nullptr;
    bool ignore_trauma = false;
    const Stay *score_stay = nullptr;
    int base_score = 0;
    int min_score = INT_MAX;

    for (const Stay &stay: stays) {
        int stay_duration = stay.exit.date - stay.entry.date;
        int stay_score = base_score;

        proc_priority = 0;
        for (const ProcedureRealisation &proc: stay.procedures) {
            const ProcedureInfo *proc_info =
                index.FindProcedure(proc.proc, proc.phase, stay.exit.date);
            if (UNLIKELY(!proc_info))
                continue;

            if (proc_info->bytes[0] & 0x80 && !(proc_info->bytes[23] & 0x80))
                return &stay;

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

        if (stay_duration > zx_duration && stay_duration >= max_duration) {
            if (stay.main_diagnosis.Matches("Z515") ||
                    stay.main_diagnosis.Matches("Z502") ||
                    stay.main_diagnosis.Matches("Z503")) {
                zx_stay = &stay;
                zx_duration = stay_duration;
            } else {
                zx_stay = nullptr;
            }
        }

        if (!ignore_trauma) {
            if (TestDiagnosis(index, stay.sex, stay.main_diagnosis, 21, 0x4)) {
                last_trauma_stay = &stay;
                if (stay_duration > max_duration) {
                    trauma_stay = &stay;
                }
            } else {
                ignore_trauma = true;
            }
        }

        if (TestDiagnosis(index, stay.sex, stay.main_diagnosis, 21, 0x20)) {
            stay_score += 150;
        } else if (stay_duration >= 2) {
            base_score += 100;
        }
        if (stay_duration == 0) {
            stay_score += 2;
        } else if (stay_duration == 1) {
            stay_score++;
        }
        if (TestDiagnosis(index, stay.sex, stay.main_diagnosis, 21, 0x2)) {
            stay_score += 201;
        }

        if (stay_score < min_score) {
            score_stay = &stay;
            min_score = stay_score;
        }

        if (stay_duration > max_duration) {
            max_duration = stay_duration;
        }
    }

    if (zx_stay)
        return zx_stay;
    if (last_trauma_stay >= score_stay)
        return trauma_stay;
    return score_stay;
}

static bool SetError(ClassifyErrorSet *error_set, int16_t error, int priority = 1)
{
    if (!error)
        return true;

    DebugAssert(error >= 0 && error < decltype(ClassifyErrorSet::errors)::Bits);
    if (error_set) {
        if (priority >= 0 && (!error_set->main_error || priority > error_set->priority ||
                              error < error_set->main_error)) {
            error_set->main_error = error;
            error_set->priority = priority;
        }
        error_set->errors.Set(error);
    }

    // For convenience
    return false;
}

static bool CheckDiagnosisErrors(const ClassifyAggregate &agg, const DiagnosisInfo &diag_info,
                                 const int16_t error_codes[13], ClassifyErrorSet *out_errors)
{
    // Inappropriate, imprecise warnings
    if (UNLIKELY(diag_info.warnings & (1 << 9))) {
        SetError(out_errors, error_codes[8], -1);
    }
    if (UNLIKELY(diag_info.warnings & (1 << 0))) {
        SetError(out_errors, error_codes[9], -1);
    }
    if (UNLIKELY(diag_info.warnings & (1 << 10))) {
        SetError(out_errors, error_codes[10], -1);
    }

    // Sex warning
    {
        int sex_bit = 13 - agg.stay.sex;
        if (UNLIKELY(diag_info.warnings & (1 << sex_bit))) {
            SetError(out_errors, error_codes[11], -1);
        }
    }

    // Age warning
    {
        int age_bit;
        if (agg.age_days < 29) {
            age_bit = 4;
        } else if (!agg.age) {
            age_bit = 3;
        } else if (agg.age < 10) {
            age_bit = 5;
        } else if (agg.age < 20) {
            age_bit = 6;
        } else if (agg.age < 65) {
            age_bit = 7;
        } else {
            age_bit = 8;
        }

        if (UNLIKELY(diag_info.warnings & (1 << age_bit))) {
            SetError(out_errors, error_codes[12], -1);
        }
    }

    const auto &diag_attr = diag_info.Attributes(agg.stay.sex);

    // Real errors
    if (UNLIKELY(diag_attr.raw[5] & 2)) {
        return SetError(out_errors, error_codes[0]);
    } else if (UNLIKELY(!diag_attr.raw[0])) {
        switch (diag_attr.raw[1]) {
            case 0: { return SetError(out_errors, error_codes[1]); } break;
            case 1: { return SetError(out_errors, error_codes[2]); } break;
            case 2: { return SetError(out_errors, error_codes[3]); } break;
            case 3: { return SetError(out_errors, error_codes[4]); } break;
        }
    } else if (UNLIKELY(diag_attr.raw[0] == 23 && diag_attr.raw[1] == 14)) {
        return SetError(out_errors, error_codes[5]);
    } else if (UNLIKELY(diag_attr.raw[19] & 0x10 && agg.age < 9)) {
        return SetError(out_errors, error_codes[6]);
    } else if (UNLIKELY(diag_attr.raw[19] & 0x8 && agg.age >= 2)) {
        return SetError(out_errors, error_codes[7]);
    }

    return true;
}

static bool AppendValidDiagnoses(ClassifyAggregate *out_agg,
                                 HeapArray<const DiagnosisInfo *> *out_diagnoses,
                                 ClassifyErrorSet *out_errors)
{
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

    for (const Stay &stay: out_agg->stays) {
        // Main diagnosis is valid (checks are done in CheckMainError)
        const DiagnosisInfo *diag_info = out_agg->index->FindDiagnosis(stay.main_diagnosis);
        if (LIKELY(diag_info)) {
            out_diagnoses->Append(diag_info);
            valid &= CheckDiagnosisErrors(*out_agg, *diag_info, main_diagnosis_errors, out_errors);
        } else {
            valid &= SetError(out_errors, 67);
        }

        if (stay.linked_diagnosis.IsValid()) {
            const DiagnosisInfo *diag_info = out_agg->index->FindDiagnosis(stay.linked_diagnosis);
            if (LIKELY(diag_info)) {
                out_diagnoses->Append(diag_info);
                valid &= CheckDiagnosisErrors(*out_agg, *diag_info,
                                              linked_diagnosis_errors, out_errors);
            } else {
                valid &= SetError(out_errors, 94);
            }
        }

        for (DiagnosisCode diag: stay.diagnoses) {
            if (diag == stay.main_diagnosis || diag == stay.linked_diagnosis)
                continue;

            if (diag.Matches("Z37")) {
                out_agg->flags |= (int)ClassifyAggregate::Flag::ChildbirthDiagnosis;
            }
            if (diag.Matches("O8") && (diag.str[2] == '0' || diag.str[2] == '1' ||
                                       diag.str[2] == '2' || diag.str[2] == '3' ||
                                       diag.str[2] == '4')) {
                out_agg->flags |= (int)ClassifyAggregate::Flag::ChildbirthType;
            }

            const DiagnosisInfo *diag_info = out_agg->index->FindDiagnosis(diag);
            if (LIKELY(diag_info)) {
                out_diagnoses->Append(diag_info);
                valid &= CheckDiagnosisErrors(*out_agg, *diag_info,
                                              associate_diagnosis_errors, out_errors);
            } else {
                valid &= SetError(out_errors, 70);
            }
        }
    }

    // Deduplicate diagnoses
    std::sort(out_diagnoses->begin(), out_diagnoses->end());
    if (out_diagnoses->len) {
        Span<const DiagnosisInfo *> diagnoses = *out_diagnoses;

        Size j = 0;
        for (Size i = 1; i < diagnoses.len; i++) {
            if (diagnoses[i] != diagnoses[j]) {
                diagnoses[++j] = diagnoses[i];
            }
        }
        out_diagnoses->RemoveFrom(j + 1);
    }
    out_agg->diagnoses = *out_diagnoses;

    return valid;
}

static bool AppendValidProcedures(ClassifyAggregate *out_agg,
                                  HeapArray<const ProcedureInfo *> *out_procedures,
                                  ClassifyErrorSet *out_errors)
{
    bool valid = true;

    out_agg->proc_activities = 0;
    for (const Stay &stay: out_agg->stays) {
        for (const ProcedureRealisation &proc: stay.procedures) {
            if (UNLIKELY(!proc.count)) {
                valid &= SetError(out_errors, 52);
            }
            if (UNLIKELY(!proc.activities)) {
                valid &= SetError(out_errors, 103);
            }
            if (UNLIKELY(proc.doc && (!IsAsciiAlphaOrDigit(proc.doc) ||
                                      proc.doc == 'I' || proc.doc == 'O'))) {
                valid &= SetError(out_errors, 173);
            }

            const ProcedureInfo *proc_info =
                out_agg->index->FindProcedure(proc.proc, proc.phase, stay.exit.date);

            if (LIKELY(proc_info)) {
                if (UNLIKELY((proc_info->bytes[43] & 0x40) && stay.sex == 2)) {
                    SetError(out_errors, 148, -1);
                }
                if (UNLIKELY((out_agg->age || out_agg->age_days > 28) &&
                             (proc_info->bytes[44] & 0x20) && (!out_agg->stay.newborn_weight ||
                                                               out_agg->stay.newborn_weight >= 3000))) {
                    valid &= SetError(out_errors, 149);
                }

                if (proc_info->bytes[41] & 0x2) {
                    out_agg->flags |= (int)ClassifyAggregate::Flag::ChildbirthProcedure;
                }

                if (UNLIKELY(!proc.date.IsValid() ||
                             proc.date < stay.entry.date ||
                             proc.date > stay.exit.date)) {
                    if (proc_info->bytes[41] & 0x2) {
                        valid &= SetError(out_errors, 142);
                    } else if (proc.date.value) {
                        // NOTE: I don't know if I'm supposed to ignore this procedure in this
                        // case. I need to test how the official classifier deals with this.
                        SetError(out_errors, 102, -1);
                    }
                }

                if (!TestStr(MakeSpan(proc.proc.str, 4), "YYYY")) {
                    uint8_t extra_activities = (uint8_t)(proc.activities & ~proc_info->activities);
                    if (UNLIKELY(extra_activities)) {
                        if (extra_activities & ~0x3E) {
                            valid &= SetError(out_errors, 103);
                        }
                        extra_activities &= 0x3E;

                        if (extra_activities & (1 << 4)) {
                            valid &= SetError(out_errors, 110);
                        }
                        if (extra_activities & ~(1 << 4)) {
                            SetError(out_errors, 111, 0);
                        }
                    }

                    // We use the pointer's LSB as a flag which is set to 1 when the procedure
                    // requires activity 1. Combined with a pointer-based sort this allows
                    // us to trivially detect when activity 1 is missing for a given procedure
                    // in the deduplication phase below (error 167).
                    StaticAssert(std::alignment_of<ProcedureInfo>::value >= 2);
                    if (!(proc.activities & (1 << 1)) && !(proc_info->bytes[42] & 0x2)) {
                        proc_info = (const ProcedureInfo *)((uintptr_t)proc_info | 0x1);
                    }
                }

                out_procedures->Append(proc_info);
                out_agg->proc_activities |= proc.activities;
            } else {
                Span <const ProcedureInfo> compatible_procs =
                    out_agg->index->FindProcedure(proc.proc);
                bool valid_proc = std::any_of(compatible_procs.begin(), compatible_procs.end(),
                                              [&](const ProcedureInfo &proc_info) {
                    return proc_info.phase == proc.phase;
                });
                if (valid_proc) {
                    if (!TestStr(MakeSpan(proc.proc.str, 4), "YYYY")) {
                        if (stay.exit.date < compatible_procs[0].limit_dates[0]) {
                            valid &= SetError(out_errors, 79);
                        } else if (stay.entry.date >= compatible_procs[compatible_procs.len - 1].limit_dates[1]) {
                            valid &= SetError(out_errors, 78);
                        }
                    }
                } else {
                    valid &= SetError(out_errors, 73);
                }
            }
        }
    }

    // Deduplicate procedures
    // TODO: Warn when we deduplicate procedures with different attributes,
    // such as when the two procedures fall into different date ranges / limits.
    std::sort(out_procedures->begin(), out_procedures->end());
    if (out_procedures->len) {
        Span<const ProcedureInfo *> procedures = *out_procedures;

        Size j = 0;
        for (Size i = 0; i < procedures.len; i++) {
            if ((uintptr_t)procedures[i] & 0x1) {
                const ProcedureInfo *proc_info =
                    (const ProcedureInfo *)((uintptr_t)procedures[i] ^ 0x1);
                if (proc_info != procedures[j]) {
                    procedures[++j] = proc_info;
                    valid &= SetError(out_errors, 167);
                }
            } else if (procedures[i] != procedures[j]) {
                procedures[++j] = procedures[i];
            }
        }
        out_procedures->RemoveFrom(j + 1);
    }
    out_agg->procedures = *out_procedures;

    return valid;
}

static bool CheckDateErrors(bool malformed_flag, Date date,
                            const int16_t error_codes[3], ClassifyErrorSet *out_errors)
{
    if (UNLIKELY(malformed_flag)) {
        return SetError(out_errors, error_codes[0]);
    } else if (UNLIKELY(!date.value)) {
        return SetError(out_errors, error_codes[1]);
    } else if (UNLIKELY(!date.IsValid())) {
        return SetError(out_errors, error_codes[2]);
    }

    return true;
}

static bool CheckDataErrors(Span<const Stay> stays, ClassifyErrorSet *out_errors)
{
    bool valid = true;

    // Bill id
    if (UNLIKELY(stays[0].error_mask & (int)Stay::Error::MalformedBillId)) {
        static bool warned = false;
        if (!warned) {
            LogError("Non-numeric RSS identifiers are not supported");
            warned = true;
        }
        valid &= SetError(out_errors, 61);
    } else if (UNLIKELY(!stays[0].bill_id)) {
        valid &= SetError(out_errors, 11);
    }

    for (const Stay &stay: stays) {
        // Sex
        if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedSex)) {
            valid &= SetError(out_errors, 17);
        } else if (UNLIKELY(stay.sex != 1 && stay.sex != 2)) {
            valid &= SetError(out_errors, stay.sex ? 17 : 16);
        }

        // Dates
        {
            // Malformed, missing, incoherent (e.g. 2001/02/29)
            static const int16_t birthdate_errors[3] = {14, 13, 39};
            static const int16_t entry_date_errors[3] = {20, 19, 21};
            static const int16_t exit_date_errors[3] = {29, 28, 30};

            bool birthdate_valid = CheckDateErrors(stay.error_mask & (int)Stay::Error::MalformedBirthdate,
                                                   stay.birthdate, birthdate_errors, out_errors);
            bool entry_date_valid = CheckDateErrors(stay.error_mask & (int)Stay::Error::MalformedEntryDate,
                                                    stay.entry.date, entry_date_errors, out_errors);
            bool exit_date_valid = CheckDateErrors(stay.error_mask & (int)Stay::Error::MalformedExitDate,
                                                   stay.exit.date, exit_date_errors, out_errors);

            if (UNLIKELY(birthdate_valid && entry_date_valid &&
                         (stay.birthdate > stay.entry.date ||
                          stay.entry.date.st.year - stay.birthdate.st.year > 140))) {
                valid &= SetError(out_errors, 15);
            }
            if (UNLIKELY(entry_date_valid && exit_date_valid &&
                         stay.exit.date < stay.entry.date)) {
                valid &= SetError(out_errors, 32);
            }

            valid &= birthdate_valid && entry_date_valid && exit_date_valid;
        }

        // Entry mode and origin
        if (UNLIKELY(stay.error_mask & ((int)Stay::Error::MalformedEntryMode |
                                        (int)Stay::Error::MalformedEntryOrigin))) {
            valid &= SetError(out_errors, 25);
        }

        // Exit mode and destination
        if (UNLIKELY(stay.error_mask & ((int)Stay::Error::MalformedExitMode |
                                        (int)Stay::Error::MalformedExitDestination))) {
            valid &= SetError(out_errors, 34);
        }

        // Sessions
        if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedSessionCount)) {
            valid &= SetError(out_errors, 36);
        }

        // Gestational age
        if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedGestationalAge)) {
            valid &= SetError(out_errors, 125);
        }

        // Menstrual period
        if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedLastMenstrualPeriod)) {
            valid &= SetError(out_errors, 160);
        } else if (UNLIKELY(stay.last_menstrual_period.value &&
                            !stay.last_menstrual_period.IsValid())) {
            valid &= SetError(out_errors, 161);
        }

        // IGS2
        if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedIgs2)) {
            valid &= SetError(out_errors, 169);
        }

        // Diagnoses
        if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedMainDiagnosis)) {
           valid &= SetError(out_errors, 41);
        } else if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
            valid &= SetError(out_errors, 40);
        }
        if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedLinkedDiagnosis)) {
            valid &= SetError(out_errors, 51);
        }
        if (UNLIKELY(stay.error_mask & (int)Stay::Error::MissingOtherDiagnosesCount)) {
            valid &= SetError(out_errors, 55);
        } else if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedOtherDiagnosesCount)) {
            valid &= SetError(out_errors, 56);
        } else if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedOtherDiagnosis)) {
            valid &= SetError(out_errors, 42);
        }

        // Procedures
        if (UNLIKELY(stay.error_mask & (int)Stay::Error::MissingProceduresCount)) {
            valid &= SetError(out_errors, 57);
        } else if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedProceduresCount)) {
            valid &= SetError(out_errors, 58);
        } else if (UNLIKELY(stay.error_mask & (int)Stay::Error::MalformedProcedureCode)) {
            valid &= SetError(out_errors, 43);
        }
    }

    // Coherency checks
    for (Size i = 1; i < stays.len; i++) {
        if (UNLIKELY(stays[i].sex != stays[i - 1].sex &&
                     (stays[i].sex == 1 || stays[i].sex == 2))) {
            valid &= SetError(out_errors, 46);
        }

        if (UNLIKELY(stays[i].birthdate != stays[i - 1].birthdate &&
                     stays[i].birthdate.IsValid())) {
            valid &= SetError(out_errors, 45);
        }
    }

    return valid;
}

static bool CheckAggregateErrors(const ClassifyAggregate &agg, ClassifyErrorSet *out_errors)
{
    bool valid = true;

    if (agg.stay.entry.mode == '0' || agg.stay.exit.mode == '0') {
        if (UNLIKELY(agg.stay.exit.mode != agg.stay.entry.mode)) {
            valid &= SetError(out_errors, 26);
            SetError(out_errors, 35);
        } else if (UNLIKELY(agg.duration > 1)) {
            valid &= SetError(out_errors, 50);
        }
    } else {
        if (UNLIKELY(agg.stay.entry.mode == '6' && agg.stay.entry.origin == '1')) {
            valid &= SetError(out_errors, 26);
        }
        if (UNLIKELY(agg.stay.exit.mode == '6' && agg.stay.exit.destination == '1')) {
            valid &= SetError(out_errors, 35);
        }
    }

    for (const Stay &stay: agg.stays) {
        // Dates
        if (UNLIKELY(stay.entry.date.st.year < 1985 && stay.entry.date.IsValid())) {
            SetError(out_errors, 77, -1);
        }

        // Entry mode and origin
        switch (stay.entry.mode) {
            case '0':
            case '6': {
                if (UNLIKELY(stay.entry.mode == '0' && stay.entry.origin == '6')) {
                    valid &= SetError(out_errors, 25);
                }
                if (UNLIKELY(stay.entry.mode == '6' && stay.entry.origin == 'R')) {
                    valid &= SetError(out_errors, 25);
                }
            } // fallthrough
            case '7': {
                switch (stay.entry.origin) {
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
                switch (stay.entry.origin) {
                    case 0:
                    case '5':
                    case '7': { /* Valid origin */ } break;

                    default: { valid &= SetError(out_errors, 25); } break;
                }
            } break;

            case 0: { valid &= SetError(out_errors, 24); } break;
            default: { valid &= SetError(out_errors, 25); } break;
        }

        // Exit mode and destination
        switch (stay.exit.mode) {
            case '0':
            case '6':
            case '7': {
                switch (stay.exit.destination) {
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
                switch (stay.exit.destination) {
                    case 0:
                    case '7': { /* Valid destination */ } break;

                    default: { valid &= SetError(out_errors, 34); } break;
                }
            } break;

            case '9': {
                if (UNLIKELY(stay.exit.destination)) {
                    valid &= SetError(out_errors, 34);
                }
            } break;

            case 0: { valid &= SetError(out_errors, 33); } break;
            default: { valid &= SetError(out_errors, 34); } break;
        }

        // Sessions
        if (UNLIKELY(agg.stays.len > 1 && stay.session_count > 0)) {
            valid &= SetError(out_errors, 37);
        }
        if (UNLIKELY(stay.session_count < 0 || stay.session_count >= 32)) {
            SetError(out_errors, 66, -1);
        }

        // Gestational age
        if (stay.gestational_age) {
            if (UNLIKELY((stay.gestational_age > 44 ||
                          (stay.gestational_age < 22 && agg.stay.exit.mode != '9' && !agg.age)))) {
                valid &= SetError(out_errors, 127);
            } else if (UNLIKELY(agg.stay.newborn_weight &&
                                ((stay.gestational_age >= 37 && agg.stay.newborn_weight < 1000 && !stay.main_diagnosis.Matches("P95")) ||
                                 (stay.gestational_age < 33 && agg.stay.newborn_weight > 4000) ||
                                 (stay.gestational_age < 28 && agg.stay.newborn_weight > 2500)))) {
                valid &= SetError(out_errors, 129);
            }
        }

        // Menstrual period
        if (UNLIKELY(stay.last_menstrual_period.value &&
                     stay.last_menstrual_period != agg.stay.last_menstrual_period)) {
            valid &= SetError(out_errors, 163);
        }

        // Stillborn
        if (UNLIKELY(stay.main_diagnosis.Matches("P95"))) {
            if (UNLIKELY(stay.exit.mode != '9')) {
                valid &= SetError(out_errors, 143);
                SetError(out_errors, 147);
            } else if (UNLIKELY(agg.stays.len > 1 || stay.entry.mode != '8' ||
                                stay.birthdate != stay.entry.date || !stay.newborn_weight ||
                                stay.exit.date != stay.entry.date)) {
                valid &= SetError(out_errors, 147);
            }
        }
    }

    // Continuity checks
    for (Size i = 1; i < agg.stays.len; i++) {
        switch (agg.stays[i].entry.mode) {
            case '0': {
                if (UNLIKELY(agg.stays[i - 1].exit.mode != '0')) {
                    valid &= SetError(out_errors, 27);
                    SetError(out_errors, 49);
                } else if (UNLIKELY(agg.stays[i].entry.date != agg.stays[i - 1].exit.date &&
                                    agg.stays[i].entry.date - agg.stays[i - 1].exit.date != 1)) {
                    valid &= SetError(out_errors, 50);
                }
            } break;

            case '6': {
                if (UNLIKELY(agg.stays[i].entry.origin != '1' || agg.stays[i - 1].exit.mode != '6')) {
                    valid &= SetError(out_errors, 27);
                    SetError(out_errors, 49);
                } else if (UNLIKELY(agg.stays[i].entry.date != agg.stays[i - 1].exit.date)) {
                    valid &= SetError(out_errors, 23);
                }
            } break;

            default: { valid &= SetError(out_errors, 27); } break;
        }
    }

    // Sessions
    if (TestDiagnosis(*agg.index, agg.stay.sex, agg.stay.main_diagnosis, 8, 0x2)) {
        if (UNLIKELY(!agg.duration && !agg.stay.session_count)) {
            bool tolerate = std::any_of(agg.procedures.begin(), agg.procedures.end(),
                                        [](const ProcedureInfo *proc_info) {
                return (proc_info->bytes[44] & 0x40);
            });
            if (!tolerate) {
                // According to the manual, this is a blocking error but the
                // official classifier does not actually enforce it.
                SetError(out_errors, 145, 0);
            }
        } else if (UNLIKELY(agg.stay.session_count > agg.duration + 1)) {
            SetError(out_errors, 146, -1);
        }
    }

    // Gestation and newborn
    if (UNLIKELY(!agg.stay.gestational_age &&
                 ((agg.flags & (int)ClassifyAggregate::Flag::Childbirth) ||
                  agg.stay.birthdate == agg.stay.entry.date))) {
        valid &= SetError(out_errors, 126);
    }
    if (UNLIKELY(agg.stay.error_mask & (int)Stay::Error::MalformedNewbornWeight)) {
        valid &= SetError(out_errors, 82);
    } else {
        if (UNLIKELY(agg.age_days < 29 && !agg.stay.newborn_weight)) {
            valid &= SetError(out_errors, 168);
        } else if (UNLIKELY(agg.stay.newborn_weight > 0 && agg.stay.newborn_weight < 100)) {
            valid &= SetError(out_errors, 128);
        }
    }
    if (UNLIKELY(agg.stay.exit.date >= Date(2013, 3, 1) &&
                 (agg.flags & (int)ClassifyAggregate::Flag::ChildbirthProcedure) &&
                 agg.stay.gestational_age < 22)) {
        valid &= SetError(out_errors, 174);
    }

    // Menstruation
    if (UNLIKELY((agg.flags & (int)ClassifyAggregate::Flag::Childbirth) &&
                 !agg.stay.last_menstrual_period.value)) {
        valid &= SetError(out_errors, 162);
    }
    if (UNLIKELY(agg.stay.sex == 1 && agg.stay.last_menstrual_period.value)) {
        SetError(out_errors, 164, -1);
    }
    if (agg.stay.last_menstrual_period.value) {
        if (UNLIKELY(agg.stay.last_menstrual_period > agg.stay.entry.date)) {
            SetError(out_errors, 165, -1);
        } else if (UNLIKELY(agg.stay.entry.date - agg.stay.last_menstrual_period > 305)) {
            SetError(out_errors, 166, -1);
        }
    }

    return valid;
}

GhmCode Aggregate(const TableSet &table_set, Span<const Stay> stays,
                  ClassifyAggregate *out_agg,
                  HeapArray<const DiagnosisInfo *> *out_diagnoses,
                  HeapArray<const ProcedureInfo *> *out_procedures,
                  ClassifyErrorSet *out_errors)
{
    DebugAssert(stays.len > 0);

    // These errors are too serious to continue (broken data, etc.)
    if (UNLIKELY(stays[0].error_mask & (int)Stay::Error::UnknownRumVersion)) {
        DebugAssert(stays.len == 1);
        SetError(out_errors, 59);
        return GhmCode::FromString("90Z00Z");
    }
    if (UNLIKELY(!CheckDataErrors(stays, out_errors)))
        return GhmCode::FromString("90Z00Z");

    out_agg->index = table_set.FindIndex(stays[stays.len - 1].exit.date);
    if (!out_agg->index) {
        SetError(out_errors, 502, 2);
        return GhmCode::FromString("90Z03Z");
    }

    // Aggregate basic information
    out_agg->stays = stays;
    out_agg->stay = stays[0];
    out_agg->age = ComputeAge(out_agg->stay.entry.date, out_agg->stay.birthdate);
    out_agg->age_days = out_agg->stay.entry.date - out_agg->stay.birthdate;
    out_agg->duration = 0;
    out_agg->flags = 0;
    out_agg->stay.last_menstrual_period = {};
    for (const Stay &stay: stays) {
        if (stay.gestational_age > 0) {
            out_agg->stay.gestational_age = stay.gestational_age;
        }
        if (stay.last_menstrual_period.value && !out_agg->stay.last_menstrual_period.value) {
            out_agg->stay.last_menstrual_period = stay.last_menstrual_period;
        }
        if (stay.igs2 > out_agg->stay.igs2) {
            out_agg->stay.igs2 = stay.igs2;
        }
        out_agg->duration += stay.exit.date - stay.entry.date;
    }
    out_agg->stay.exit = stays[stays.len - 1].exit;
    out_agg->stay.flags = 0;
    if (out_agg->stays[stays.len - 1].flags & (int)Stay::Flag::Confirmed) {
        out_agg->stay.flags |= (int)Stay::Flag::Confirmed;
    }
    out_agg->stay.diagnoses = {};
    out_agg->stay.procedures = {};

    bool valid = true;

    // Aggregate diagnoses and procedures
    valid &= AppendValidDiagnoses(out_agg, out_diagnoses, out_errors);
    valid &= AppendValidProcedures(out_agg, out_procedures, out_errors);

    // Pick main stay
    if (stays.len > 1) {
        out_agg->main_stay = FindMainStay(*out_agg->index, stays, out_agg->duration);

        out_agg->stay.main_diagnosis = out_agg->main_stay->main_diagnosis;
        out_agg->stay.linked_diagnosis = out_agg->main_stay->linked_diagnosis;
    } else {
        out_agg->main_stay = &stays[0];
    }

    // Check remaining stay errors
    valid &= CheckAggregateErrors(*out_agg, out_errors);
    if (UNLIKELY(!valid))
        return GhmCode::FromString("90Z00Z");

    return {};
}

int GetMinimalDurationForSeverity(int severity)
{
    DebugAssert(severity >= 0 && severity < 4);
    return severity ? (severity + 2) : 0;
}

int LimitSeverityWithDuration(int severity, int duration)
{
    DebugAssert(severity >= 0 && severity < 4);
    return duration >= 3 ? std::min(duration - 2, severity) : 0;
}

static int ExecuteGhmTest(RunGhmTreeContext &ctx, const GhmDecisionNode &ghm_node,
                          ClassifyErrorSet *out_errors)
{
    DebugAssert(ghm_node.type == GhmDecisionNode::Type::Test);

    switch (ghm_node.u.test.function) {
        case 0:
        case 1: {
            return GetDiagnosisByte(ctx.agg->stay.sex, *ctx.main_diagnosis,
                                    ghm_node.u.test.params[0]);
        } break;

        case 2: {
            for (const ProcedureInfo *proc_info: ctx.agg->procedures) {
                if (TestProcedure(*proc_info, ghm_node.u.test.params[0], ghm_node.u.test.params[1]))
                    return 1;
            }
            return 0;
        } break;

        case 3: {
            if (ghm_node.u.test.params[1] == 1) {
                return (ctx.agg->age_days > ghm_node.u.test.params[0]);
            } else {
                return (ctx.agg->age > ghm_node.u.test.params[0]);
            }
        } break;

        case 5: {
            return TestDiagnosis(ctx.agg->stay.sex, *ctx.main_diagnosis,
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
        } break;

        case 6: {
            // NOTE: Incomplete, should behave differently when params[0] >= 128,
            // but it's probably relevant only for FG 9 and 10 (CMAs)
            for (const DiagnosisInfo *diag_info: ctx.agg->diagnoses) {
                if (diag_info == ctx.main_diagnosis || diag_info == ctx.linked_diagnosis)
                    continue;
                if (TestDiagnosis(ctx.agg->stay.sex, *diag_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]))
                    return 1;
            }
            return 0;
        } break;

        case 7: {
            for (const DiagnosisInfo *diag_info: ctx.agg->diagnoses) {
                if (TestDiagnosis(ctx.agg->stay.sex, *diag_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]))
                    return 1;
            }
            return 0;
        } break;

        case 9: {
            int result = 0;
            for (const ProcedureInfo *proc_info: ctx.agg->procedures) {
                if (TestProcedure(*proc_info, 0, 0x80)) {
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
            for (const ProcedureInfo *proc_info: ctx.agg->procedures) {
                if (TestProcedure(*proc_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1])) {
                    matches++;
                    if (matches >= 2)
                        return 1;
                }
            }
            return 0;
        } break;

        case 13: {
            uint8_t diag_byte = GetDiagnosisByte(ctx.agg->stay.sex, *ctx.main_diagnosis,
                                                 ghm_node.u.test.params[0]);
            return (diag_byte == ghm_node.u.test.params[1]);
        } break;

        case 14: {
            return ((int)ctx.agg->stay.sex == ghm_node.u.test.params[0] - 48);
        } break;

        case 18: {
            Size matches = 0, special_matches = 0;
            for (const DiagnosisInfo *diag_info: ctx.agg->diagnoses) {
                if (TestDiagnosis(ctx.agg->stay.sex, *diag_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1])) {
                    matches++;
                    if (diag_info == ctx.main_diagnosis || diag_info == ctx.linked_diagnosis) {
                        special_matches++;
                    }
                    if (matches >= 2 && matches > special_matches)
                        return 1;
                }
            }
            return 0;
        } break;

        case 19: {
            switch (ghm_node.u.test.params[1]) {
                case 0: {
                    return (ctx.agg->stay.exit.mode == '0' + ghm_node.u.test.params[0]);
                } break;
                case 1: {
                    return (ctx.agg->stay.exit.destination == '0' + ghm_node.u.test.params[0]);
                } break;
                case 2: {
                    return (ctx.agg->stay.entry.mode == '0' + ghm_node.u.test.params[0]);
                } break;
                case 3: {
                    return (ctx.agg->stay.entry.origin == '0' + ghm_node.u.test.params[0]);
                } break;
            }
        } break;

        case 20: {
            return 0;
        } break;

        case 22: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.agg->duration < param);
        } break;

        case 26: {
            return TestDiagnosis(*ctx.agg->index, ctx.agg->stay.sex, ctx.agg->stay.linked_diagnosis,
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
        } break;

        case 28: {
            SetError(out_errors, ghm_node.u.test.params[0]);
            return 0;
        } break;

        case 29: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.agg->duration == param);
        } break;

        case 30: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.agg->stay.session_count == param);
        } break;

        case 33: {
            return !!(ctx.agg->proc_activities & (1 << ghm_node.u.test.params[0]));
        } break;

        case 34: {
            if (ctx.linked_diagnosis &&
                    ctx.linked_diagnosis->diag == ctx.agg->stay.linked_diagnosis) {
                uint8_t cmd = ctx.linked_diagnosis->Attributes(ctx.agg->stay.sex).cmd;
                uint8_t jump = ctx.linked_diagnosis->Attributes(ctx.agg->stay.sex).jump;
                if (cmd || jump != 3) {
                    std::swap(ctx.main_diagnosis, ctx.linked_diagnosis);
                }
            }

            return 0;
        } break;

        case 35: {
            return (ctx.main_diagnosis->diag != ctx.agg->stay.main_diagnosis);
        } break;

        case 36: {
            for (const DiagnosisInfo *diag_info: ctx.agg->diagnoses) {
                if (diag_info == ctx.linked_diagnosis)
                    continue;
                if (TestDiagnosis(ctx.agg->stay.sex, *diag_info,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]))
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
                int gestational_age = ctx.agg->stay.gestational_age;
                if (!gestational_age) {
                    gestational_age = 99;
                }

                for (const ValueRangeCell<2> &cell: ctx.agg->index->gnn_cells) {
                    if (cell.Test(0, ctx.agg->stay.newborn_weight) &&
                            cell.Test(1, gestational_age)) {
                        ctx.gnn = cell.value;
                        break;
                    }
                }
            }

            return 0;
        } break;

        case 41: {
            for (const DiagnosisInfo *diag_info: ctx.agg->diagnoses) {
                uint8_t cmd = diag_info->Attributes(ctx.agg->stay.sex).cmd;
                uint8_t jump = diag_info->Attributes(ctx.agg->stay.sex).jump;
                if (cmd == ghm_node.u.test.params[0] && jump == ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;

        case 42: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.agg->stay.newborn_weight && ctx.agg->stay.newborn_weight < param);
        } break;

        case 43: {
            for (const DiagnosisInfo *diag_info: ctx.agg->diagnoses) {
                if (diag_info == ctx.linked_diagnosis)
                    continue;

                uint8_t cmd = diag_info->Attributes(ctx.agg->stay.sex).cmd;
                uint8_t jump = diag_info->Attributes(ctx.agg->stay.sex).jump;
                if (cmd == ghm_node.u.test.params[0] && jump == ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;
    }

    LogError("Unknown test %1 or invalid arguments", ghm_node.u.test.function);
    return -1;
}

static bool CheckConfirmation(const ClassifyAggregate &agg, GhmCode ghm,
                              const GhmRootInfo &ghm_root_info, ClassifyErrorSet *out_errors)
{
    bool valid = true;

    bool confirm = false;
    if (UNLIKELY(agg.duration >= 365)) {
        confirm = true;
    } else if (agg.duration < ghm_root_info.confirm_duration_treshold &&
               agg.stay.exit.mode != '9' && agg.stay.exit.mode != '0' &&
               (agg.stay.exit.mode != '7' || agg.stay.exit.destination != '1')) {
        confirm = true;
    } else if (agg.flags & ((int)ClassifyAggregate::Flag::Childbirth |
                            (int)ClassifyAggregate::Flag::ChildbirthType)) {
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

    if (agg.stay.flags & (int)Stay::Flag::Confirmed) {
        if (confirm) {
            SetError(out_errors, 223, 0);
        } else if (agg.duration >= ghm_root_info.confirm_duration_treshold) {
            valid &= SetError(out_errors, 124);
        }
    } else if (confirm) {
        valid &= SetError(out_errors, 120);
    }

    return valid;
}

static bool CheckGhmErrors(const ClassifyAggregate &agg, GhmCode ghm, ClassifyErrorSet *out_errors)
{
    bool valid = true;

    // Sessions
    if (ghm.parts.cmd == 28) {
        if (UNLIKELY(agg.stays.len > 1)) {
            valid &= SetError(out_errors, 150);
        }
        if (UNLIKELY(agg.stay.exit.date >= Date(2016, 3, 1) &&
                     agg.stay.main_diagnosis.Matches("Z511") &&
                     !agg.stay.linked_diagnosis.IsValid())) {
            valid &= SetError(out_errors, 187);
        }
    }

    // Menstruation
    {
        static GhmRootCode ghm_root_14C04 = GhmRootCode::FromString("14C04");
        static GhmRootCode ghm_root_14M02 = GhmRootCode::FromString("14M02");
        if (UNLIKELY((ghm.parts.cmd == 14 && ghm.Root() != ghm_root_14C04 && ghm.Root() != ghm_root_14M02) &&
                     !agg.stay.last_menstrual_period.value)) {
            valid &= SetError(out_errors, 162);
        }
    }

    // FIXME: Find a way to optimize away calls to FromString() in simple cases
    {
        static GhmRootCode ghm_root_14Z08 = GhmRootCode::FromString("14Z08");
        if (UNLIKELY(agg.stay.exit.date >= Date(2016, 3, 1) && ghm.Root() == ghm_root_14Z08)) {
            bool type_present = std::any_of(agg.procedures.begin(), agg.procedures.end(),
                                            [](const ProcedureInfo *proc_info) {
                static ProcedureCode proc1 = ProcedureCode::FromString("JNJD002");
                static ProcedureCode proc2 = ProcedureCode::FromString("JNJP001");

                return proc_info->proc == proc1 || proc_info->proc == proc2;
            });
            if (!type_present) {
                SetError(out_errors, 179, -1);
            }
        }
    }

    return valid;
}

static GhmCode RunGhmTree(const ClassifyAggregate &agg, ClassifyErrorSet *out_errors)
{
    GhmCode ghm = {};

    RunGhmTreeContext ctx = {};
    ctx.agg = &agg;
    ctx.main_diagnosis = agg.index->FindDiagnosis(agg.stay.main_diagnosis);
    if (agg.stay.linked_diagnosis.IsValid()) {
        ctx.linked_diagnosis = agg.index->FindDiagnosis(agg.stay.linked_diagnosis);
    }

    Size ghm_node_idx = 0;
    for (Size i = 0; !ghm.IsValid(); i++) {
        if (UNLIKELY(i >= agg.index->ghm_nodes.len)) {
            LogError("Empty GHM tree or infinite loop (%2)", agg.index->ghm_nodes.len);
            SetError(out_errors, 4, 2);
            return GhmCode::FromString("90Z03Z");
        }

        Assert(ghm_node_idx < agg.index->ghm_nodes.len);
        const GhmDecisionNode &ghm_node = agg.index->ghm_nodes[ghm_node_idx];

        switch (ghm_node.type) {
            case GhmDecisionNode::Type::Test: {
                int function_ret = ExecuteGhmTest(ctx, ghm_node, out_errors);
                if (UNLIKELY(function_ret < 0 || function_ret >= ghm_node.u.test.children_count)) {
                    LogError("Result for GHM tree test %1 out of range (%2 - %3)",
                             ghm_node.u.test.function, 0, ghm_node.u.test.children_count);
                    SetError(out_errors, 4, 2);
                    return GhmCode::FromString("90Z03Z");
                }

                ghm_node_idx = ghm_node.u.test.children_idx + function_ret;
            } break;

            case GhmDecisionNode::Type::Ghm: {
                ghm = ghm_node.u.ghm.ghm;
                if (ghm_node.u.ghm.error && out_errors) {
                    SetError(out_errors, ghm_node.u.ghm.error);
                }
            } break;
        }
    }

    return ghm;
}

static inline bool TestDiagnosisExclusion(const TableIndex &index,
                                          const DiagnosisInfo &cma_diag_info,
                                          const DiagnosisInfo &main_diag_info)
{
    Assert(cma_diag_info.exclusion_set_idx < index.exclusions.len);
    const ExclusionInfo *excl = &index.exclusions[cma_diag_info.exclusion_set_idx];
    if (UNLIKELY(!excl))
        return false;

    Assert(main_diag_info.cma_exclusion_mask.offset < SIZE(excl->raw));
    return (excl->raw[main_diag_info.cma_exclusion_mask.offset] &
            main_diag_info.cma_exclusion_mask.value);
}

static bool TestExclusion(const ClassifyAggregate &agg,
                          const GhmRootInfo &ghm_root_info,
                          const DiagnosisInfo &diag_info,
                          const DiagnosisInfo &main_diag_info,
                          const DiagnosisInfo *linked_diag_info)
{
    if (agg.age < 14 && (diag_info.Attributes(agg.stay.sex).raw[19] & 0x10))
        return true;
    if (agg.age >= 2 &&
            ((diag_info.Attributes(agg.stay.sex).raw[19] & 0x8) || diag_info.diag.str[0] == 'P'))
        return true;

    Assert(ghm_root_info.cma_exclusion_mask.offset < SIZE(DiagnosisInfo::attributes[0].raw));
    if (diag_info.Attributes(agg.stay.sex).raw[ghm_root_info.cma_exclusion_mask.offset] &
            ghm_root_info.cma_exclusion_mask.value)
        return true;

    if (TestDiagnosisExclusion(*agg.index, diag_info, main_diag_info))
        return true;
    if (linked_diag_info && TestDiagnosisExclusion(*agg.index, diag_info, *linked_diag_info))
        return true;

    return false;
}

static GhmCode RunGhmSeverity(const ClassifyAggregate &agg, GhmCode ghm,
                              const GhmRootInfo &ghm_root_info)
{
    // Ambulatory and / or short duration GHM
    if (ghm_root_info.allow_ambulatory && agg.duration == 0) {
        ghm.parts.mode = 'J';
    } else if (ghm_root_info.short_duration_treshold &&
               agg.duration < ghm_root_info.short_duration_treshold) {
        ghm.parts.mode = 'T';
    } else if (ghm.parts.mode >= 'A' && ghm.parts.mode < 'E') {
        int severity = ghm.parts.mode - 'A';

        if (ghm_root_info.childbirth_severity_list) {
            Assert(ghm_root_info.childbirth_severity_list > 0 &&
                   ghm_root_info.childbirth_severity_list <= SIZE(agg.index->cma_cells));
            for (const ValueRangeCell<2> &cell: agg.index->cma_cells[ghm_root_info.childbirth_severity_list - 1]) {
                if (cell.Test(0, agg.stay.gestational_age) && cell.Test(1, severity)) {
                    severity = cell.value;
                    break;
                }
            }
        }

        ghm.parts.mode = (char)('A' + LimitSeverityWithDuration(severity, agg.duration));
    } else if (!ghm.parts.mode) {
        int severity = 0;

        // We wouldn't have gotten here if main_diagnosis was missing from the index
        const DiagnosisInfo *main_diag_info = agg.index->FindDiagnosis(agg.stay.main_diagnosis);
        const DiagnosisInfo *linked_diag_info = agg.index->FindDiagnosis(agg.stay.linked_diagnosis);

        for (const DiagnosisInfo *diag_info: agg.diagnoses) {
            if (diag_info->diag == agg.stay.main_diagnosis ||
                    diag_info->diag == agg.stay.linked_diagnosis)
                continue;

            int new_severity = diag_info->Attributes(agg.stay.sex).severity;
            if (new_severity > severity && !TestExclusion(agg, ghm_root_info, *diag_info,
                                                          *main_diag_info, linked_diag_info)) {
                severity = new_severity;
            }
        }

        if (agg.age >= ghm_root_info.old_age_treshold &&
                severity < ghm_root_info.old_severity_limit) {
            severity++;
        } else if (agg.age < ghm_root_info.young_age_treshold &&
                   severity < ghm_root_info.young_severity_limit) {
            severity++;
        } else if (agg.stay.exit.mode == '9' && !severity) {
            severity = 1;
        }

        ghm.parts.mode = (char)('1' + LimitSeverityWithDuration(severity, agg.duration));
    }

    return ghm;
}

GhmCode ClassifyGhm(const ClassifyAggregate &agg, ClassifyErrorSet *out_errors)
{
    GhmCode ghm;

    ghm = RunGhmTree(agg, out_errors);

    const GhmRootInfo *ghm_root_info = agg.index->FindGhmRoot(ghm.Root());
    if (UNLIKELY(!ghm_root_info)) {
        LogError("Unknown GHM root '%1'", ghm.Root());
        SetError(out_errors, 4, 2);
        return GhmCode::FromString("90Z03Z");
    }

    if (UNLIKELY(!CheckGhmErrors(agg, ghm, out_errors)))
        return GhmCode::FromString("90Z00Z");
    if (UNLIKELY(!CheckConfirmation(agg, ghm, *ghm_root_info, out_errors)))
        return GhmCode::FromString("90Z00Z");

    ghm = RunGhmSeverity(agg, ghm, *ghm_root_info);

    return ghm;
}

static int8_t GetAuthorizationType(const AuthorizationSet &authorization_set,
                                   UnitCode unit, Date date)
{
    if (unit.number >= 10000) {
        return (int8_t)(unit.number % 100);
    } else if (unit.number) {
        const Authorization *auth = authorization_set.FindUnit(unit, date);
        if (UNLIKELY(!auth)) {
            LogDebug("Unit %1 is missing from authorization set", unit);
            return 0;
        }
        return auth->type;
    } else {
        return 0;
    }
}

static bool TestAuthorization(const AuthorizationSet &authorization_set,
                              UnitCode unit, Date date, int8_t authorization)
{
    if (GetAuthorizationType(authorization_set, unit, date) == authorization)
        return true;

    Span<const Authorization> facility_auths = authorization_set.FindUnit(UnitCode(INT16_MAX));
    for (const Authorization &auth: facility_auths) {
        if (auth.type == authorization && date >= auth.dates[0] && date < auth.dates[1])
            return true;
    }

    return false;
}

static bool TestGhs(const ClassifyAggregate &agg, const AuthorizationSet &authorization_set,
                    const GhsAccessInfo &ghs_access_info)
{
    if (ghs_access_info.minimal_age && agg.age < ghs_access_info.minimal_age)
        return false;

    int duration;
    if (ghs_access_info.unit_authorization) {
        duration = 0;
        bool authorized = false;
        for (const Stay &stay: agg.stays) {
            if (TestAuthorization(authorization_set, stay.unit, stay.exit.date,
                                  ghs_access_info.unit_authorization)) {
                if (stay.exit.date != stay.entry.date) {
                    duration += stay.exit.date - stay.entry.date;
                } else {
                    duration++;
                }
                authorized = true;
            }
        }
        if (!authorized)
            return false;
    } else {
        duration = agg.duration;
    }
    if (ghs_access_info.bed_authorization) {
        bool test = std::any_of(agg.stays.begin(), agg.stays.end(),
                                [&](const Stay &stay) {
            return stay.bed_authorization == ghs_access_info.bed_authorization;
        });
        if (!test)
            return false;
    }
    if (ghs_access_info.minimal_duration && duration < ghs_access_info.minimal_duration)
        return false;

    if (ghs_access_info.main_diagnosis_mask.value) {
        if (!TestDiagnosis(*agg.index, agg.stay.sex, agg.stay.main_diagnosis,
                           ghs_access_info.main_diagnosis_mask))
            return false;
    }
    if (ghs_access_info.diagnosis_mask.value) {
        bool test = std::any_of(agg.diagnoses.begin(), agg.diagnoses.end(),
                                [&](const DiagnosisInfo *diag_info) {
            return TestDiagnosis(agg.stay.sex, *diag_info, ghs_access_info.diagnosis_mask);
        });
        if (!test)
            return false;
    }
    for (const ListMask &mask: ghs_access_info.procedure_masks) {
        bool test = std::any_of(agg.procedures.begin(), agg.procedures.end(),
                                [&](const ProcedureInfo *proc_info) {
            return TestProcedure(*proc_info, mask);
        });
        if (!test)
            return false;
    }

    return true;
}

GhsCode ClassifyGhs(const ClassifyAggregate &agg, const AuthorizationSet &authorization_set,
                    GhmCode ghm)
{
    if (UNLIKELY(!ghm.IsValid() || ghm.IsError()))
        return GhsCode(9999);

    // Deal with UHCD-only stays
    if (agg.duration > 0 && agg.stays[0].entry.mode == '8' &&
            agg.stays[agg.stays.len - 1].exit.mode == '8') {
        bool uhcd = std::all_of(agg.stays.begin(), agg.stays.end(),
                                [&](const Stay &stay) {
            int8_t auth_type = GetAuthorizationType(authorization_set, stay.unit, stay.exit.date);
            return (auth_type == 7);
        });
        if (uhcd) {
            ClassifyAggregate agg0 = agg;
            agg0.duration = 0;

            // Don't run ClassifyGhm() because that would test the confirmation flag,
            // which makes no sense when duration is forced to 0.
            ghm = RunGhmTree(agg0, nullptr);
            const GhmRootInfo *ghm_root_info = agg.index->FindGhmRoot(ghm.Root());
            if (LIKELY(ghm_root_info)) {
                ghm = RunGhmSeverity(agg0, ghm, *ghm_root_info);
            }
        }
    }

    Span<const GhsAccessInfo> compatible_ghs = agg.index->FindCompatibleGhs(ghm);

    for (const GhsAccessInfo &ghs_access_info: compatible_ghs) {
        if (TestGhs(agg, authorization_set, ghs_access_info))
            return ghs_access_info.Ghs(Sector::Public);
    }
    return GhsCode(9999);
}

static bool TestSupplementRea(const ClassifyAggregate &agg, const Stay &stay,
                              Size list2_treshold)
{
    if (stay.igs2 >= 15 || agg.age < 18) {
        Size list2_matches = 0;
        for (const ProcedureRealisation &proc: stay.procedures) {
            if (TestProcedure(*agg.index, stay.exit.date, proc, 27, 0x10))
                return true;
            if (TestProcedure(*agg.index, stay.exit.date, proc, 27, 0x8)) {
                list2_matches++;
                if (list2_matches >= list2_treshold)
                    return true;
            }
        }
    }

    return false;
}

static bool TestSupplementSrc(const ClassifyAggregate &agg, const Stay &stay,
                              int16_t igs2_src_adjust, bool prev_reanimation)
{
    if (prev_reanimation)
        return true;
    if (agg.age >= 18 && stay.igs2 - igs2_src_adjust >= 15)
        return true;

    HeapArray<ProcedureCode> src_procedures;

    if (stay.igs2 - igs2_src_adjust >= 7 || agg.age < 18) {
        for (DiagnosisCode diag: stay.diagnoses) {
            if (TestDiagnosis(*agg.index, agg.stay.sex, diag, 21, 0x10))
                return true;
            if (TestDiagnosis(*agg.index, agg.stay.sex, diag, 21, 0x8)) {
                // TODO: HashSet for SrcPair on diagnoses to accelerate this
                for (const SrcPair &pair: agg.index->src_pairs[0]) {
                    if (pair.diag == diag) {
                        src_procedures.Append(pair.proc);
                    }
                }
            }
        }
    }
    if (agg.age < 18) {
        for (DiagnosisCode diag: stay.diagnoses) {
            if (TestDiagnosis(*agg.index, agg.stay.sex, diag, 22, 0x80))
                return true;
            if (TestDiagnosis(*agg.index, agg.stay.sex, diag, 22, 0x40)) {
                for (const SrcPair &pair: agg.index->src_pairs[1]) {
                    if (pair.diag == diag) {
                        src_procedures.Append(pair.proc);
                    }
                }
            }
        }
    }
    for (const ProcedureRealisation &proc: stay.procedures) {
        for (ProcedureCode diag_proc: src_procedures) {
            if (diag_proc == proc.proc)
                return true;
        }
    }

    for (const ProcedureRealisation &proc: stay.procedures) {
        if (TestProcedure(*agg.index, stay.exit.date, proc, 38, 0x1))
            return true;
    }
    if (&stay > &agg.stays[0]) {
        for (const ProcedureRealisation &proc: (&stay - 1)->procedures) {
            if (TestProcedure(*agg.index, stay.exit.date, proc, 38, 0x1))
                return true;
        }
    }

    return false;
}

// TODO: Count correctly when authorization date is too early (REA)
void CountSupplements(const ClassifyAggregate &agg, const AuthorizationSet &authorization_set,
                      GhsCode ghs, SupplementCounters<int16_t> *out_counters)
{
    DebugAssert(ghs != GhsCode(9999));

    int16_t igs2_src_adjust;
    if (agg.age >= 80) {
        igs2_src_adjust = 18;
    } else if (agg.age >= 75) {
        igs2_src_adjust = 16;
    } else if (agg.age >= 70) {
        igs2_src_adjust = 15;
    } else if (agg.age >= 60) {
        igs2_src_adjust = 12;
    } else if (agg.age >= 40) {
        igs2_src_adjust = 7;
    } else {
        igs2_src_adjust = 0;
    }
    bool prev_reanimation = (agg.stays[0].entry.mode == '7' && agg.stays[0].entry.origin == 'R');

    const Stay *ambu_stay = nullptr;
    int ambu_priority = 0;
    int16_t *ambu_counter = nullptr;

    for (const Stay &stay: agg.stays) {
        int8_t auth_type = GetAuthorizationType(authorization_set, stay.unit, stay.exit.date);
        const AuthorizationInfo *auth_info = agg.index->FindAuthorization(AuthorizationScope::Unit, auth_type);
        if (!auth_info)
            continue;

        int16_t *counter = nullptr;
        int priority = 0;
        bool reanimation = false;

        switch (auth_info->function) {
            case 1: {
                if (LIKELY(agg.age < 2) && ghs != GhsCode(5903)) {
                    counter = &out_counters->st.nn1;
                    priority = 1;
                }
            } break;

            case 2: {
                if (LIKELY(agg.age < 2) && ghs != GhsCode(5903)) {
                    counter = &out_counters->st.nn2;
                    priority = 3;
                }
            } break;

            case 3: {
                if (LIKELY(agg.age < 2) && ghs != GhsCode(5903)) {
                    if (TestSupplementRea(agg, stay, 1)) {
                        counter = &out_counters->st.nn3;
                        priority = 6;
                        reanimation = true;
                    } else {
                        counter = &out_counters->st.nn2;
                        priority = 3;
                    }
                }
            } break;

            case 4: {
                if (TestSupplementRea(agg, stay, 3)) {
                    counter = &out_counters->st.rea;
                    priority = 7;
                    reanimation = true;
                } else {
                    counter = &out_counters->st.reasi;
                    priority = 5;
                }
            } break;

            case 6: {
                if (TestSupplementSrc(agg, stay, igs2_src_adjust, prev_reanimation)) {
                    counter = &out_counters->st.src;
                    priority = 2;
                }
            } break;

            case 8: {
                counter = &out_counters->st.si;
                priority = 4;
            } break;

            case 9: {
                if (ghs != GhsCode(5903)) {
                    if (agg.age < 18) {
                        if (TestSupplementRea(agg, stay, 1)) {
                            counter = &out_counters->st.rep;
                            priority = 8;
                            reanimation = true;
                        } else {
                            counter = &out_counters->st.reasi;
                            priority = 5;
                        }
                    } else {
                        if (TestSupplementRea(agg, stay, 3)) {
                            counter = &out_counters->st.rea;
                            priority = 7;
                            reanimation = true;
                        } else {
                            counter = &out_counters->st.reasi;
                            priority = 5;
                        }
                    }
                }
            } break;
        }

        prev_reanimation = reanimation;

        if (stay.exit.date != stay.entry.date) {
            if (ambu_stay && ambu_priority >= priority) {
                if (counter) {
                    *counter = (int16_t)(*counter + (stay.exit.date - stay.entry.date) +
                                         (stay.exit.mode == '9') - 1);
                }
                (*ambu_counter)++;
            } else if (counter) {
                *counter = (int16_t)(*counter + (stay.exit.date - stay.entry.date) +
                                     (stay.exit.mode == '9'));
            }
            ambu_stay = nullptr;
            ambu_priority = 0;
        } else if (priority > ambu_priority) {
            ambu_stay = &stay;
            ambu_priority = priority;
            ambu_counter = counter;
        }
    }
    if (ambu_stay) {
        (*ambu_counter)++;
    }
}

int PriceGhs(const GhsPriceInfo &price_info, int duration, bool death)
{
    int price_cents = price_info.price_cents;

    if (duration < price_info.exb_treshold && !death) {
        if (price_info.flags & (int)GhsPriceInfo::Flag::ExbOnce) {
            price_cents -= price_info.exb_cents;
        } else {
            price_cents -= price_info.exb_cents * (price_info.exb_treshold - duration);
        }
    } else if (duration + death > price_info.exh_treshold) {
        price_cents += price_info.exh_cents * (duration + death - price_info.exh_treshold);
    }

    return price_cents;
}

int PriceGhs(const ClassifyAggregate &agg, GhsCode ghs)
{
    DebugAssert(ghs != GhsCode(9999));

    // FIXME: Add some kind of error flag when this happens?
    const GhsPriceInfo *price_info = agg.index->FindGhsPrice(ghs, Sector::Public);
    if (!price_info) {
        LogDebug("Cannot find price for GHS %1 (%2 -- %3)", ghs,
                 agg.index->limit_dates[0], agg.index->limit_dates[1]);
        return 0;
    }

    return PriceGhs(*price_info, agg.duration, agg.stay.exit.mode == '9');
}

int PriceSupplements(const TableIndex &index, const SupplementCounters<int16_t> &days,
                     SupplementCounters<int32_t> *out_prices)
{
    const SupplementCounters<int32_t> &prices = index.SupplementPrices(Sector::Public);

    int total_cents = 0;
    for (Size i = 0; i < ARRAY_SIZE(SupplementTypeNames); i++) {
        out_prices->values[i] += days.values[i] * prices.values[i];
        total_cents += days.values[i] * prices.values[i];
    }

    return total_cents;
}

Size ClassifyRaw(const TableSet &table_set, const AuthorizationSet &authorization_set,
                 Span<const Stay> stays, ClassifyResult out_results[])
{
    // Reuse data structures to reduce heap allocations
    // (around 5% faster on typical sets on my old MacBook)
    ClassifyErrorSet errors;
    HeapArray<const DiagnosisInfo *> diagnoses;
    HeapArray<const ProcedureInfo *> procedures;

    Size i;
    for (i = 0; stays.len; i++) {
        ClassifyResult result = {};
        ClassifyAggregate agg;

        errors.main_error = 0;
        diagnoses.Clear(256);
        procedures.Clear(512);

        do {
            result.stays = Cluster(stays, &stays);

            result.ghm = Aggregate(table_set, result.stays,
                                   &agg, &diagnoses, &procedures, &errors);
            if (UNLIKELY(result.ghm.IsError()))
                break;
            result.main_stay_idx = agg.main_stay - agg.stays.ptr;
            result.ghm = ClassifyGhm(agg, &errors);
            if (UNLIKELY(result.ghm.IsError()))
                break;
        } while (false);
        result.main_error = errors.main_error;

        result.ghs = ClassifyGhs(agg, authorization_set, result.ghm);
        if (result.ghs != GhsCode(9999)) {
            CountSupplements(agg, authorization_set, result.ghs, &result.supplement_days);

            result.ghs_price_cents = PriceGhs(agg, result.ghs);
            int supplement_cents = PriceSupplements(*agg.index, result.supplement_days,
                                                     &result.supplement_cents);
            result.price_cents = result.ghs_price_cents + supplement_cents;
        }

        out_results[i] = result;
    }

    return i;
}

void Classify(const TableSet &table_set, const AuthorizationSet &authorization_set,
              Span<const Stay> stays, HeapArray<ClassifyResult> *out_results)
{
    // Pessimistic assumption (no multi-stay)
    out_results->Grow(stays.len);
    out_results->len += ClassifyRaw(table_set, authorization_set, stays,
                                    out_results->end());
}

void ClassifyParallel(const TableSet &table_set, const AuthorizationSet &authorization_set,
                      Span<const Stay> stays, HeapArray<ClassifyResult> *out_results)
{
    if (!stays.len)
        return;

    static const int task_size = 2048;

    // Pessimistic assumption (no multi-stay), but we cannot resize the
    // buffer as we go because the worker threads will fill it directly.
    out_results->Grow(stays.len);

    Async async;
    Size results_count = 1;
    {
        Size results_offset = out_results->len;
        Span<const Stay> task_stays = stays[0];
        for (Size i = 1; i < stays.len; i++) {
            if (stays[i].bill_id != stays[i - 1 ].bill_id) {
                if (results_count % task_size == 0) {
                    async.AddTask([&, task_stays, results_offset]() mutable {
                        ClassifyRaw(table_set, authorization_set, task_stays,
                                    out_results->ptr + results_offset);
                        return true;
                    });
                    results_offset += task_size;
                    task_stays = MakeSpan(&stays[i], 0);
                }
                results_count++;
            }
            task_stays.len++;
        }
        async.AddTask([&, task_stays, results_offset]() mutable {
            ClassifyRaw(table_set, authorization_set, task_stays,
                        out_results->ptr + results_offset);
            return true;
        });
    }
    async.Sync();

    out_results->len += results_count;
}

void Summarize(Span<const ClassifyResult> results, ClassifySummary *out_summary)
{
    out_summary->results_count += results.len;
    for (const ClassifyResult &result: results) {
        out_summary->stays_count += result.stays.len;
        out_summary->failures_count += result.ghm.IsError();
        out_summary->ghs_total_cents += result.ghs_price_cents;
        out_summary->supplement_days += result.supplement_days;
        out_summary->supplement_cents += result.supplement_cents;
        out_summary->total_cents += result.price_cents;
    }
}
