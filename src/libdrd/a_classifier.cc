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
    DiagnosisCode main_diagnosis;
    DiagnosisCode linked_diagnosis;
    int gnn;
};

static int ComputeAge(Date date, Date birthdate)
{
    int age = date.st.year - birthdate.st.year;
    age -= (date.st.month < birthdate.st.month ||
            (date.st.month == birthdate.st.month && date.st.day < birthdate.st.day));
    return age;
}

static inline uint8_t GetDiagnosisByte(const TableIndex &index, Sex sex,
                                       DiagnosisCode diag, uint8_t byte_idx)
{
    const DiagnosisInfo *diag_info = index.FindDiagnosis(diag);

    if (UNLIKELY(!diag_info)) {
        LogDebug("Ignoring unknown diagnosis '%1'", diag);
        return 0;
    }

    Assert(byte_idx < SIZE(DiagnosisInfo::attributes[0].raw));
    return diag_info->Attributes(sex).raw[byte_idx];
}

static inline bool TestDiagnosis(const TableIndex &index, Sex sex,
                                 DiagnosisCode diag, ListMask mask)
{
    DebugAssert(mask.offset >= 0 && mask.offset <= UINT8_MAX);
    return GetDiagnosisByte(index, sex, diag, (uint8_t)mask.offset) & mask.value;
}
static inline bool TestDiagnosis(const TableIndex &index, Sex sex,
                                 DiagnosisCode diag, uint8_t offset, uint8_t value)
{
    return GetDiagnosisByte(index, sex, diag, offset) & value;
}

static inline uint8_t GetProcedureByte(const TableIndex &index,
                                       const ProcedureRealisation &proc, int16_t byte_idx)
{
    const ProcedureInfo *proc_info = index.FindProcedure(proc.proc, proc.phase, proc.date);

    if (UNLIKELY(!proc_info)) {
        LogDebug("Ignoring unknown procedure '%1' (%2)", proc.proc, proc.date);
        return 0;
    }

    Assert(byte_idx >= 0 && byte_idx < SIZE(ProcedureInfo::bytes));
    return proc_info->bytes[byte_idx];
}

static inline bool TestProcedure(const TableIndex &index,
                                 const ProcedureRealisation &proc, ListMask mask)
{
    return GetProcedureByte(index, proc, mask.offset) & mask.value;
}
static inline bool TestProcedure(const TableIndex &index,
                                 const ProcedureRealisation &proc, int16_t offset, uint8_t value)
{
    return GetProcedureByte(index, proc, offset) & value;
}

static inline bool AreStaysCompatible(const Stay &stay1, const Stay &stay2,
                                      ClusterMode cluster_mode)
{
    switch (cluster_mode) {
        case ClusterMode::StayModes: {
            return !stay1.session_count &&
                   stay2.stay_id == stay1.stay_id &&
                   !stay2.session_count &&
                   (stay2.entry.mode == '6' || stay2.entry.mode == '0');
        } break;
        case ClusterMode::BillId: { return stay2.bill_id == stay1.bill_id; } break;
        case ClusterMode::Disable: { return false; } break;
    }
    DebugAssert(false);
}

Span<const Stay> Cluster(Span<const Stay> stays, ClusterMode cluster_mode,
                         Span<const Stay> *out_remainder)
{
    DebugAssert(stays.len > 0);

    Size agg_len = 1;
    while (agg_len < stays.len &&
           AreStaysCompatible(stays[agg_len - 1], stays[agg_len], cluster_mode)) {
        agg_len++;
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
                index.FindProcedure(proc.proc, proc.phase, proc.date);
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

static bool SetError(ClassifyErrorSet *error_set, int16_t error, bool force = false)
{
    if (!error)
        return true;

    DebugAssert(error >= 0 && error < error_set->errors.Bits);
    if (error_set) {
        if (!error_set->main_error || error < error_set->main_error || force) {
            error_set->main_error = error;
        }
        error_set->errors.Set(error);
    }

    // For convenience
    return false;
}

static bool CheckDateErrors(Date date, bool malformed_flag,
                            const int16_t error_codes[3], ClassifyErrorSet *out_errors)
{
    if (UNLIKELY(!date.value)) {
        if (!malformed_flag) {
            return SetError(out_errors, error_codes[0]);
        } else {
            return SetError(out_errors, error_codes[1]);
        }
    } else if (UNLIKELY(!date.IsValid())) {
        return SetError(out_errors, error_codes[2]);
    }

    return true;
}

static bool CheckDiagnosisErrors(const ClassifyAggregate &agg, DiagnosisCode diag,
                                 const int16_t error_codes[9], ClassifyErrorSet *out_errors)
{
    const DiagnosisInfo *diag_info = agg.index->FindDiagnosis(diag);
    if (UNLIKELY(!diag_info))
        return SetError(out_errors, error_codes[0]);

    const auto &diag_attr = diag_info->Attributes(agg.stay.sex);
    if (UNLIKELY(!(diag_attr.raw[5] & 1))) {
        return SetError(out_errors, error_codes[0]);
    } else if (UNLIKELY(diag_attr.raw[5] & 2)) {
        return SetError(out_errors, error_codes[1]);
    } else if (UNLIKELY(!diag_attr.raw[0])) {
        switch (diag_attr.raw[1]) {
            case 0: { return SetError(out_errors, error_codes[2]); } break;
            case 1: { return SetError(out_errors, error_codes[3]); } break;
            case 2: { return SetError(out_errors, error_codes[4]); } break;
            case 3: { return SetError(out_errors, error_codes[5]); } break;
        }
    } else if (UNLIKELY(diag_attr.raw[0] == 23 && diag_attr.raw[1] == 14)) {
        return SetError(out_errors, error_codes[6]);
    } else if (UNLIKELY(diag_attr.raw[19] & 0x10 && agg.age < 9)) {
        return SetError(out_errors, error_codes[7]);
    } else if (UNLIKELY(diag_attr.raw[19] & 0x8 && agg.age >= 2)) {
        return SetError(out_errors, error_codes[8]);
    }

    return true;
}

static bool CheckAggregateErrors(const ClassifyAggregate &agg, ClassifyErrorSet *out_errors)
{
    bool valid = true;

    // TODO: Do complete inter-RSS compatibility checks
    if (UNLIKELY(agg.stay.entry.mode == '6' && agg.stay.entry.origin == '1')) {
        valid &= SetError(out_errors, 26);
    }

    if (UNLIKELY(agg.stay.exit.mode == '6' && agg.stay.exit.destination == '1')) {
        valid &= SetError(out_errors, 35);
    }

    return valid;
}

// Continuity checks are not done here, see CheckStayContinuity()
static bool CheckStayErrors(const ClassifyAggregate &agg, const Stay &stay,
                            ClassifyErrorSet *out_errors)
{
    static const int16_t birthdate_error_codes[3] = {13, 14, 39};
    static const int16_t entry_date_error_codes[3] = {19, 20, 21};
    static const int16_t exit_date_error_codes[3] = {28, 29, 30};

    static const int16_t main_diagnosis_error_codes[9] = {67, 68, 113, 114, 115, 113,
                                                          180, 130, 133};
    static const int16_t linked_diagnosis_error_codes[9] = {94, 95, 116, 117, 118, 0,
                                                            181, 131, 134};

    bool valid = true;

    // Main and linked diagnosis
    if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
        valid &= SetError(out_errors, 40);
    } else {
        valid &= CheckDiagnosisErrors(agg, stay.main_diagnosis,
                                      main_diagnosis_error_codes, out_errors);
    }
    if (stay.linked_diagnosis.IsValid()) {
        valid &= CheckDiagnosisErrors(agg, stay.linked_diagnosis,
                                      linked_diagnosis_error_codes, out_errors);
    }

    // Sex
    if (UNLIKELY(stay.sex != Sex::Male && stay.sex != Sex::Female)) {
        if (!(int)stay.sex && !(stay.error_mask & (int)Stay::Error::MalformedSex)) {
            valid &= SetError(out_errors, 16);
        } else {
            valid &= SetError(out_errors, 17);
        }
    }

    // Birthdate
    valid &= CheckDateErrors(stay.birthdate,
                             stay.error_mask & (int)Stay::Error::MalformedBirthdate,
                             birthdate_error_codes, out_errors);
    if (UNLIKELY(stay.birthdate > stay.entry.date &&
                 stay.birthdate.IsValid() && stay.entry.date.IsValid())) {
        valid &= SetError(out_errors, 15);
    }

    // Entry and exit dates
    valid &= CheckDateErrors(stay.entry.date,
                             stay.error_mask & (int)Stay::Error::MalformedEntryDate,
                             entry_date_error_codes, out_errors);
    valid &= CheckDateErrors(stay.exit.date,
                             stay.error_mask & (int)Stay::Error::MalformedExitDate,
                             exit_date_error_codes, out_errors);
    if (UNLIKELY(stay.exit.date < stay.entry.date &&
                 stay.entry.date.IsValid() && stay.exit.date.IsValid())) {
        valid &= SetError(out_errors, 32);
    }

    // Entry mode and origin
    if (UNLIKELY(stay.error_mask & ((int)Stay::Error::MalformedEntryMode |
                                    (int)Stay::Error::MalformedEntryOrigin))) {
        valid &= SetError(out_errors, 25);
    }
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
    if (UNLIKELY(stay.error_mask & ((int)Stay::Error::MalformedExitMode |
                                    (int)Stay::Error::MalformedExitDestination))) {
        valid &= SetError(out_errors, 34);
    }
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

    // Misc checks
    if (UNLIKELY(stay.main_diagnosis.Matches("P95"))) {
        if (UNLIKELY(stay.exit.mode != '9')) {
            valid &= SetError(out_errors, 143);
            SetError(out_errors, 147);
        } else if (UNLIKELY(agg.stays.len > 1 || stay.entry.mode != '8' || agg.age > 0 ||
                            stay.birthdate != stay.entry.date || !stay.newborn_weight ||
                            stay.exit.date != stay.entry.date)) {
            valid &= SetError(out_errors, 147);
        }
    }

    return valid;
}

static bool CheckStayContinuity(const Stay &stay1, const Stay &stay2, ClassifyErrorSet *out_errors)
{
    bool valid = true;

    // Sex
    if (UNLIKELY(stay2.sex != stay1.sex && (stay2.sex == Sex::Male ||
                                            stay2.sex == Sex::Female))) {
        valid &= SetError(out_errors, 46);
    }

    // Birthdate
    if (UNLIKELY(stay2.birthdate != stay1.birthdate && stay2.birthdate.IsValid())) {
        valid &= SetError(out_errors, 45);
    }

    // Entry mode
    switch (stay2.entry.mode) {
        case '0': {
            if (UNLIKELY(stay1.exit.mode != '0')) {
                valid &= SetError(out_errors, 27);
                SetError(out_errors, 49);
            } else if (UNLIKELY(stay2.entry.date - stay1.exit.date > 1)) {
                valid &= SetError(out_errors, 50);
            }
        } break;

        case '6': {
            if (UNLIKELY(stay2.entry.origin != '1' || stay1.exit.mode != '6')) {
                valid &= SetError(out_errors, 27);
                SetError(out_errors, 49);
            } else if (UNLIKELY(stay2.entry.date != stay1.exit.date)) {
                valid &= SetError(out_errors, 23);
            }
        } break;

        default: { valid &= SetError(out_errors, 27); } break;
    }

    return valid;
}

// FIXME: Check Stay invariants before classification (all diag and proc exist, etc.)
GhmCode Aggregate(const TableSet &table_set, Span<const Stay> stays,
                  ClassifyAggregate *out_agg,
                  HeapArray<DiagnosisCode> *out_diagnoses,
                  HeapArray<ProcedureRealisation> *out_procedures,
                  ClassifyErrorSet *out_errors)
{
    DebugAssert(stays.len > 0);

    out_agg->stays = stays;

    out_agg->index = table_set.FindIndex(stays[stays.len - 1].exit.date);
    if (!out_agg->index) {
        LogError("No table available on '%1'", stays[stays.len - 1].exit.date);
        SetError(out_errors, 502, true);
        return GhmCode::FromString("90Z03Z");
    }

    out_agg->stay = stays[0];
    out_agg->age = ComputeAge(out_agg->stay.entry.date, out_agg->stay.birthdate);
    out_agg->duration = 0;
    for (const Stay &stay: stays) {
        if (stay.gestational_age > 0) {
            // TODO: Must be first (newborn) or on RUM with a$41.2 only
            out_agg->stay.gestational_age = stay.gestational_age;
        }
        if (stay.igs2 > out_agg->stay.igs2) {
            out_agg->stay.igs2 = stay.igs2;
        }
        out_agg->duration += stay.exit.date - stay.entry.date;
    }
    out_agg->stay.exit = stays[stays.len - 1].exit;
    out_agg->stay.diagnoses = {};
    out_agg->stay.procedures = {};

    // Individual and coherency checks
    {
        bool valid = true;

        valid &= CheckAggregateErrors(*out_agg, out_errors);
        valid &= CheckStayErrors(*out_agg, stays[0], out_errors);
        for (Size i = 1; i < stays.len; i++) {
            valid &= CheckStayErrors(*out_agg, stays[i], out_errors);
            valid &= CheckStayContinuity(stays[i - 1], stays[i], out_errors);
        }

        if (UNLIKELY(!valid))
            return GhmCode::FromString("90Z00Z");
    }

    // Deduplicate diagnoses
    if (out_diagnoses) {
        for (const Stay &stay: stays) {
            out_diagnoses->Append(stay.diagnoses);
        }

        std::sort(out_diagnoses->begin(), out_diagnoses->end(),
                  [](DiagnosisCode code1, DiagnosisCode code2) {
            return code1.value < code2.value;
        });

        if (out_diagnoses->len) {
            Span<DiagnosisCode> diagnoses = *out_diagnoses;

            Size j = 0;
            for (Size i = 1; i < diagnoses.len; i++) {
                if (diagnoses[i] != diagnoses[j]) {
                    diagnoses[++j] = diagnoses[i];
                }
            }
            out_diagnoses->RemoveFrom(j + 1);
        }

        out_agg->diagnoses = *out_diagnoses;
    }

    // Deduplicate procedures
    if (out_procedures) {
        Size procedures_start = out_procedures->len;
        for (const Stay &stay: stays) {
            out_procedures->Append(stay.procedures);
        }
        out_agg->procedures = out_procedures->Take(procedures_start,
                                                   out_procedures->len - procedures_start);

        std::sort(out_procedures->begin(), out_procedures->end(),
                  [](const ProcedureRealisation &proc1, const ProcedureRealisation &proc2) {
            return MultiCmp(proc1.proc.value - proc2.proc.value,
                            proc1.phase - proc2.phase) < 0;
        });

        // TODO: Warn when we deduplicate procedures with different attributes,
        // such as when the two procedures fall into different date ranges / limits.
        if (out_procedures->len) {
            Span<ProcedureRealisation> procedures = *out_procedures;

            Size j = 0;
            for (Size i = 1; i < out_procedures->len; i++) {
                if (procedures[i].proc == procedures[j].proc &&
                        procedures[i].phase == procedures[j].phase) {
                    procedures[j].activities = (uint8_t)(procedures[j].activities |
                                                         procedures[i].activities);
                    int new_count = procedures[j].count + procedures[i].count;
                    if (LIKELY(new_count < 9999)) {
                        procedures[j].count = (int16_t)new_count;
                    } else {
                        procedures[j].count = 9999;
                    }
                } else {
                    procedures[++j] = procedures[i];
                }
            }
            out_procedures->RemoveFrom(j + 1);
        }

        out_agg->procedures = *out_procedures;
    }

    if (stays.len > 1) {
        const Stay *main_stay = FindMainStay(*out_agg->index, stays, out_agg->duration);

        out_agg->stay.main_diagnosis = main_stay->main_diagnosis;
        out_agg->stay.linked_diagnosis = main_stay->linked_diagnosis;
    }

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
            return GetDiagnosisByte(*ctx.agg->index, ctx.agg->stay.sex,
                                    ctx.main_diagnosis, ghm_node.u.test.params[0]);
        } break;

        case 2: {
            for (const ProcedureRealisation &proc: ctx.agg->procedures) {
                if (TestProcedure(*ctx.agg->index, proc,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]))
                    return 1;
            }
            return 0;
        } break;

        case 3: {
            if (ghm_node.u.test.params[1] == 1) {
                int age_days = ctx.agg->stay.entry.date - ctx.agg->stay.birthdate;
                return (age_days > ghm_node.u.test.params[0]);
            } else {
                return (ctx.agg->age > ghm_node.u.test.params[0]);
            }
        } break;

        case 5: {
            return TestDiagnosis(*ctx.agg->index, ctx.agg->stay.sex, ctx.main_diagnosis,
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
        } break;

        case 6: {
            // NOTE: Incomplete, should behave differently when params[0] >= 128,
            // but it's probably relevant only for FG 9 and 10 (CMAs)
            for (DiagnosisCode diag: ctx.agg->diagnoses) {
                if (diag == ctx.main_diagnosis || diag == ctx.linked_diagnosis)
                    continue;
                if (TestDiagnosis(*ctx.agg->index, ctx.agg->stay.sex, diag,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]))
                    return 1;
            }
            return 0;
        } break;

        case 7: {
            for (const DiagnosisCode &diag: ctx.agg->diagnoses) {
                if (TestDiagnosis(*ctx.agg->index, ctx.agg->stay.sex, diag,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1]))
                    return 1;
            }
            return 0;
        } break;

        case 9: {
            int result = 0;
            for (const ProcedureRealisation &proc: ctx.agg->procedures) {
                if (TestProcedure(*ctx.agg->index, proc, 0, 0x80)) {
                    if (TestProcedure(*ctx.agg->index, proc,
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
            for (const ProcedureRealisation &proc: ctx.agg->procedures) {
                if (TestProcedure(*ctx.agg->index, proc,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1])) {
                    matches++;
                    if (matches >= 2)
                        return 1;
                }
            }
            return 0;
        } break;

        case 13: {
            uint8_t diag_byte = GetDiagnosisByte(*ctx.agg->index, ctx.agg->stay.sex,
                                                 ctx.main_diagnosis, ghm_node.u.test.params[0]);
            return (diag_byte == ghm_node.u.test.params[1]);
        } break;

        case 14: {
            return ((int)ctx.agg->stay.sex - 1 == ghm_node.u.test.params[0] - 49);
        } break;

        case 18: {
            Size matches = 0, special_matches = 0;
            for (DiagnosisCode diag: ctx.agg->diagnoses) {
                if (TestDiagnosis(*ctx.agg->index, ctx.agg->stay.sex, diag,
                                  ghm_node.u.test.params[0], ghm_node.u.test.params[1])) {
                    matches++;
                    if (diag == ctx.main_diagnosis || diag == ctx.linked_diagnosis) {
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
            for (const ProcedureRealisation &proc: ctx.agg->procedures) {
                if (proc.activities & (1 << ghm_node.u.test.params[0]))
                    return 1;
            }

            return 0;
        } break;

        case 34: {
            if (ctx.linked_diagnosis.IsValid() &&
                    ctx.linked_diagnosis == ctx.agg->stay.linked_diagnosis) {
                const DiagnosisInfo *diag_info = ctx.agg->index->FindDiagnosis(ctx.linked_diagnosis);
                if (LIKELY(diag_info)) {
                    uint8_t cmd = diag_info->Attributes(ctx.agg->stay.sex).cmd;
                    uint8_t jump = diag_info->Attributes(ctx.agg->stay.sex).jump;
                    if (cmd || jump != 3) {
                        std::swap(ctx.main_diagnosis, ctx.linked_diagnosis);
                    }
                }
            }

            return 0;
        } break;

        case 35: {
            return (ctx.main_diagnosis != ctx.agg->stay.main_diagnosis);
        } break;

        case 36: {
            for (DiagnosisCode diag: ctx.agg->diagnoses) {
                if (diag == ctx.linked_diagnosis)
                    continue;
                if (TestDiagnosis(*ctx.agg->index, ctx.agg->stay.sex, diag,
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
            for (DiagnosisCode diag: ctx.agg->diagnoses) {
                const DiagnosisInfo *diag_info = ctx.agg->index->FindDiagnosis(diag);
                if (UNLIKELY(!diag_info))
                    continue;

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
            for (DiagnosisCode diag: ctx.agg->diagnoses) {
                if (diag == ctx.linked_diagnosis)
                    continue;

                const DiagnosisInfo *diag_info = ctx.agg->index->FindDiagnosis(diag);
                if (UNLIKELY(!diag_info))
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

GhmCode RunGhmTree(const ClassifyAggregate &agg, ClassifyErrorSet *out_errors)
{
    GhmCode ghm = {};

    RunGhmTreeContext ctx = {};
    ctx.agg = &agg;
    ctx.main_diagnosis = agg.stay.main_diagnosis;
    ctx.linked_diagnosis = agg.stay.linked_diagnosis;

    Size ghm_node_idx = 0;
    for (Size i = 0; !ghm.IsValid(); i++) {
        if (UNLIKELY(i >= agg.index->ghm_nodes.len)) {
            LogError("Empty GHM tree or infinite loop (%2)", agg.index->ghm_nodes.len);
            SetError(out_errors, 4, true);
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
                    SetError(out_errors, 4, true);
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

    if (ghm.parts.cmd == 28) {
        if (UNLIKELY(agg.stays.len > 1)) {
            SetError(out_errors, 150);
            return GhmCode::FromString("90Z00Z");
        }
        if (UNLIKELY(agg.stay.exit.date >= Date(2016, 3, 1) &&
                     agg.stay.main_diagnosis.Matches("Z511") &&
                     !agg.stay.linked_diagnosis.IsValid())) {
            SetError(out_errors, 187);
            return GhmCode::FromString("90Z00Z");
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

GhmCode RunGhmSeverity(const ClassifyAggregate &agg, GhmCode ghm,
                       ClassifyErrorSet *out_errors)
{
    const GhmRootInfo *ghm_root_info = agg.index->FindGhmRoot(ghm.Root());
    if (UNLIKELY(!ghm_root_info)) {
        LogError("Unknown GHM root '%1'", ghm.Root());
        SetError(out_errors, 4, true);
        return GhmCode::FromString("90Z03Z");
    }

    // Ambulatory and / or short duration GHM
    if (ghm_root_info->allow_ambulatory && agg.duration == 0) {
        ghm.parts.mode = 'J';
    } else if (ghm_root_info->short_duration_treshold &&
               agg.duration < ghm_root_info->short_duration_treshold) {
        ghm.parts.mode = 'T';
    } else if (ghm.parts.mode >= 'A' && ghm.parts.mode < 'E') {
        int severity = ghm.parts.mode - 'A';

        if (ghm_root_info->childbirth_severity_list) {
            Assert(ghm_root_info->childbirth_severity_list > 0 &&
                   ghm_root_info->childbirth_severity_list <= SIZE(agg.index->cma_cells));
            for (const ValueRangeCell<2> &cell: agg.index->cma_cells[ghm_root_info->childbirth_severity_list - 1]) {
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

        for (const DiagnosisCode &diag: agg.diagnoses) {
            if (diag == agg.stay.main_diagnosis || diag == agg.stay.linked_diagnosis)
                continue;

            const DiagnosisInfo *diag_info = agg.index->FindDiagnosis(diag);
            if (UNLIKELY(!diag_info))
                continue;

            int new_severity = diag_info->Attributes(agg.stay.sex).severity;
            if (new_severity > severity && !TestExclusion(agg, *ghm_root_info, *diag_info,
                                                          *main_diag_info, linked_diag_info)) {
                severity = new_severity;
            }
        }

        if (agg.age >= ghm_root_info->old_age_treshold &&
                severity < ghm_root_info->old_severity_limit) {
            severity++;
        } else if (agg.age < ghm_root_info->young_age_treshold &&
                   severity < ghm_root_info->young_severity_limit) {
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
    ghm = RunGhmSeverity(agg, ghm, out_errors);

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
        if (auth.type == authorization)
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
                                [&](DiagnosisCode diag) {
            return TestDiagnosis(*agg.index, agg.stay.sex, diag, ghs_access_info.diagnosis_mask);
        });
        if (!test)
            return false;
    }
    for (const ListMask &mask: ghs_access_info.procedure_masks) {
        bool test = std::any_of(agg.procedures.begin(), agg.procedures.end(),
                                [&](const ProcedureRealisation &proc) {
            return TestProcedure(*agg.index, proc, mask);
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
            ghm = ClassifyGhm(agg0, nullptr);
        }
    }

    Span<const GhsAccessInfo> compatible_ghs = agg.index->FindCompatibleGhs(ghm);

    for (const GhsAccessInfo &ghs_access_info: compatible_ghs) {
        if (TestGhs(agg, authorization_set, ghs_access_info))
            return ghs_access_info.ghs[0];
    }
    return GhsCode(9999);
}

static bool TestSupplementRea(const ClassifyAggregate &agg, const Stay &stay,
                              Size list2_treshold)
{
    if (stay.igs2 >= 15 || agg.age < 18) {
        Size list2_matches = 0;
        for (const ProcedureRealisation &proc: stay.procedures) {
            if (TestProcedure(*agg.index, proc, 27, 0x10))
                return true;
            if (TestProcedure(*agg.index, proc, 27, 0x8)) {
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
        if (TestProcedure(*agg.index, proc, 38, 0x1))
            return true;
    }
    if (&stay > &agg.stays[0]) {
        for (const ProcedureRealisation &proc: (&stay - 1)->procedures) {
            if (TestProcedure(*agg.index, proc, 38, 0x1))
                return true;
        }
    }

    return false;
}

// TODO: Count correctly when authorization date is too early (REA)
void CountSupplements(const ClassifyAggregate &agg, const AuthorizationSet &authorization_set,
                      GhsCode ghs, SupplementCounters *out_counters)
{
    if (UNLIKELY(ghs == GhsCode(9999)))
         return;

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
    int32_t *ambu_counter = nullptr;

    for (const Stay &stay: agg.stays) {
        int8_t auth_type = GetAuthorizationType(authorization_set, stay.unit, stay.exit.date);
        const AuthorizationInfo *auth_info = agg.index->FindAuthorization(AuthorizationScope::Unit, auth_type);
        if (!auth_info)
            continue;

        int32_t *counter = nullptr;
        int priority = 0;
        bool reanimation = false;

        switch (auth_info->function) {
            case 1: {
                if (LIKELY(agg.age < 2) && ghs != GhsCode(5903)) {
                    counter = &out_counters->nn1;
                    priority = 1;
                }
            } break;

            case 2: {
                if (LIKELY(agg.age < 2) && ghs != GhsCode(5903)) {
                    counter = &out_counters->nn2;
                    priority = 3;
                }
            } break;

            case 3: {
                if (LIKELY(agg.age < 2) && ghs != GhsCode(5903)) {
                    if (TestSupplementRea(agg, stay, 1)) {
                        counter = &out_counters->nn3;
                        priority = 6;
                        reanimation = true;
                    } else {
                        counter = &out_counters->nn2;
                        priority = 3;
                    }
                }
            } break;

            case 4: {
                if (TestSupplementRea(agg, stay, 3)) {
                    counter = &out_counters->rea;
                    priority = 7;
                    reanimation = true;
                } else {
                    counter = &out_counters->reasi;
                    priority = 5;
                }
            } break;

            case 6: {
                if (TestSupplementSrc(agg, stay, igs2_src_adjust, prev_reanimation)) {
                    counter = &out_counters->src;
                    priority = 2;
                }
            } break;

            case 8: {
                counter = &out_counters->si;
                priority = 4;
            } break;

            case 9: {
                if (ghs != GhsCode(5903)) {
                    if (agg.age < 18) {
                        if (TestSupplementRea(agg, stay, 1)) {
                            counter = &out_counters->rep;
                            priority = 8;
                            reanimation = true;
                        } else {
                            counter = &out_counters->reasi;
                            priority = 5;
                        }
                    } else {
                        if (TestSupplementRea(agg, stay, 3)) {
                            counter = &out_counters->rea;
                            priority = 7;
                            reanimation = true;
                        } else {
                            counter = &out_counters->reasi;
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
                    *counter += stay.exit.date - stay.entry.date + (stay.exit.mode == '9') - 1;
                }
                (*ambu_counter)++;
            } else if (counter) {
                *counter += stay.exit.date - stay.entry.date + (stay.exit.mode == '9');
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
    int price_cents = price_info.sectors[0].price_cents;

    if (duration < price_info.sectors[0].exb_treshold && !death) {
        if (price_info.sectors[0].flags & (int)GhsPriceInfo::Flag::ExbOnce) {
            price_cents -= price_info.sectors[0].exb_cents;
        } else {
            price_cents -= price_info.sectors[0].exb_cents * (price_info.sectors[0].exb_treshold - duration);
        }
    } else if (duration + death > price_info.sectors[0].exh_treshold) {
        price_cents += price_info.sectors[0].exh_cents * (duration + death - price_info.sectors[0].exh_treshold);
    }

    return price_cents;
}

int PriceGhs(const ClassifyAggregate &agg, GhsCode ghs)
{
    if (ghs == GhsCode(9999))
        return 0;

    const GhsPriceInfo *price_info = agg.index->FindGhsPrice(ghs);
    if (!price_info) {
        LogDebug("Cannot find price for GHS %1 (%2 -- %3)", ghs,
                 agg.index->limit_dates[0], agg.index->limit_dates[1]);
        return 0;
    }

    return PriceGhs(*price_info, agg.duration, agg.stay.exit.mode == '9');
}

Size ClassifyRaw(const TableSet &table_set, const AuthorizationSet &authorization_set,
                 Span<const Stay> stays, ClusterMode cluster_mode,
                 ClassifyResult out_results[])
{
    // Reuse data structures to reduce heap allocations
    // (around 5% faster on typical sets on my old MacBook)
    ClassifyErrorSet errors;
    HeapArray<DiagnosisCode> diagnoses;
    HeapArray<ProcedureRealisation> procedures;

    Size i;
    for (i = 0; stays.len; i++) {
        ClassifyResult result = {};
        ClassifyAggregate agg;

        errors.main_error = 0;
        diagnoses.Clear(256);
        procedures.Clear(512);

        do {
            result.stays = Cluster(stays, cluster_mode, &stays);

            result.ghm = Aggregate(table_set, result.stays,
                                   &agg, &diagnoses, &procedures, &errors);
            result.duration = agg.duration;
            if (UNLIKELY(result.ghm.IsError()))
                break;
            result.ghm = ClassifyGhm(agg, &errors);
            if (UNLIKELY(result.ghm.IsError()))
                break;
        } while (false);
        result.main_error = errors.main_error;

        result.ghs = ClassifyGhs(agg, authorization_set, result.ghm);
        result.ghs_price_cents = PriceGhs(agg, result.ghs);
        CountSupplements(agg, authorization_set, result.ghs, &result.supplements);

        out_results[i] = result;
    }

    return i;
}

void Classify(const TableSet &table_set, const AuthorizationSet &authorization_set,
              Span<const Stay> stays, ClusterMode cluster_mode,
              HeapArray<ClassifyResult> *out_results)
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
            if (!AreStaysCompatible(stays[i - 1], stays[i], cluster_mode)) {
                if (results_count % task_size == 0) {
                    async.AddTask([&, task_stays, results_offset]() mutable {
                        ClassifyRaw(table_set, authorization_set, task_stays, cluster_mode,
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
            ClassifyRaw(table_set, authorization_set, task_stays, cluster_mode,
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
        out_summary->ghs_total_cents += result.ghs_price_cents;
        out_summary->supplements.rea += result.supplements.rea;
        out_summary->supplements.reasi += result.supplements.reasi;
        out_summary->supplements.si += result.supplements.si;
        out_summary->supplements.src += result.supplements.src;
        out_summary->supplements.rep += result.supplements.rep;
        out_summary->supplements.nn1 += result.supplements.nn1;
        out_summary->supplements.nn2 += result.supplements.nn2;
        out_summary->supplements.nn3 += result.supplements.nn3;
    }
}
