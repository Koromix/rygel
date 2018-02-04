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
                                       DiagnosisCode diag, int16_t byte_idx)
{
    const DiagnosisInfo *diag_info = index.FindDiagnosis(diag);

    // FIXME: Warning / classifier errors
    if (UNLIKELY(!diag_info))
        return 0;
    if (UNLIKELY(byte_idx >= SIZE(DiagnosisInfo::attributes[0].raw)))
        return 0;

    return diag_info->Attributes(sex).raw[byte_idx];
}

static inline bool TestDiagnosis(const TableIndex &index, Sex sex,
                                 DiagnosisCode diag, ListMask mask)
{
    return GetDiagnosisByte(index, sex, diag, mask.offset) & mask.value;
}
static inline bool TestDiagnosis(const TableIndex &index, Sex sex,
                                 DiagnosisCode diag, int16_t offset, uint8_t value)
{
    return GetDiagnosisByte(index, sex, diag, offset) & value;
}

static inline uint8_t GetProcedureByte(const TableIndex &index,
                                       const ProcedureRealisation &proc, int16_t byte_idx)
{
    const ProcedureInfo *proc_info = index.FindProcedure(proc.proc, proc.phase, proc.date);

    if (UNLIKELY(!proc_info))
        return 0;
    if (UNLIKELY(byte_idx >= SIZE(ProcedureInfo::bytes)))
        return 0;

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
                   (stay2.entry.mode == 6 || stay2.entry.mode == 0);
        } break;
        case ClusterMode::BillId: { return stay2.bill_id == stay1.bill_id; } break;
        case ClusterMode::Disable: { return false; } break;
    }
    Assert(false);
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

static const Stay *FindMainStay(const TableIndex &index, Span<const Stay> stays,
                                int duration)
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
            if (!proc_info)
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

static GhmCode SetError(ClassifyErrorSet *error_set, int8_t category, int16_t error)
{
    static GhmCode base_error_ghm = GhmCode::FromString("90Z00Z");

    DebugAssert(error >= 0 && error < error_set->errors.Bits);
    if (error_set) {
        if (!error_set->main_error || error < error_set->main_error) {
            error_set->main_error = error;
        }
        error_set->errors.Set(error);
    }

    GhmCode error_ghm = base_error_ghm;
    error_ghm.parts.seq = category;
    return error_ghm;
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

    {
        Date date = stays[stays.len - 1].exit.date;
        out_agg->index = table_set.FindIndex(date);
        if (!out_agg->index) {
            LogError("No table available on '%1'", stays[stays.len - 1].exit.date);
            return SetError(out_errors, 3, 502);
        }
    }

    bool valid = true;

    out_agg->stay = stays[0];
    out_agg->age = ComputeAge(out_agg->stay.entry.date, out_agg->stay.birthdate);
    out_agg->duration = 0;
    for (const Stay &stay: stays) {
        if (!stay.main_diagnosis.IsValid()) {
            SetError(out_errors, 0, 40);
            valid = false;
        }

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

    // Individual checks
    for (const Stay &stay: stays) {
        if (UNLIKELY(!stay.birthdate.value)) {
            if (stays[0].error_mask & (int)Stay::Error::MalformedBirthdate) {
                SetError(out_errors, 0, 14);
            } else {
                SetError(out_errors, 0, 13);
            }
            valid = false;
        } else if (UNLIKELY(!stay.birthdate.IsValid())) {
            SetError(out_errors, 0, 39);
            valid = false;
        }
        if (UNLIKELY(stay.exit.date < stay.entry.date)) {
            SetError(out_errors, 0, 32);
            valid = false;
        }
    }

    // Coherency checks
    for (Size i = 1; i < stays.len; i++) {
        const Stay &stay = stays[i];

        if (UNLIKELY(stay.entry.date != stays[i - 1].exit.date)) {
            if (stay.entry.mode != 0 || stays[i - 1].exit.mode != 0 ||
                    stays[i - 1].exit.date - stay.entry.date > 1) {
                SetError(out_errors, 0, 23);
                valid = false;
            }
        }
        if (UNLIKELY(stay.birthdate != stays[0].birthdate)) {
            SetError(out_errors, 0, 45);
            valid = false;
        }
        if (UNLIKELY(stay.sex != stays[0].sex)) {
            SetError(out_errors, 0, 46);
            valid = false;
        }
    }

    if (!valid)
        return GhmCode::FromString("90Z00Z");

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
            Size j = 0;
            for (Size i = 1; i < out_diagnoses->len; i++) {
                if ((*out_diagnoses)[i] != (*out_diagnoses)[j]) {
                    (*out_diagnoses)[++j] = (*out_diagnoses)[i];
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
            Size j = 0;
            for (Size i = 1; i < out_procedures->len; i++) {
                if ((*out_procedures)[i].proc == (*out_procedures)[j].proc &&
                        (*out_procedures)[i].phase == (*out_procedures)[j].phase) {
                    (*out_procedures)[j].activities =
                        (uint8_t)((*out_procedures)[j].activities | (*out_procedures)[i].activities);
                    // TODO: Prevent possible overflow
                    (*out_procedures)[j].count =
                        (int16_t)((*out_procedures)[j].count + (*out_procedures)[i].count);
                    if (UNLIKELY((*out_procedures)[j].count > 9999)) {
                        (*out_procedures)[j].count = 9999;
                    }
                } else {
                    (*out_procedures)[++j] = (*out_procedures)[i];
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

static bool TestExclusion(const TableIndex &index,
                          const DiagnosisInfo &cma_diag_info,
                          const DiagnosisInfo &main_diag_info)
{
    // TODO: Check boundaries, and take care of DumpDiagnosis too
    const ExclusionInfo *excl = &index.exclusions[cma_diag_info.exclusion_set_idx];
    if (UNLIKELY(!excl))
        return false;
    return (excl->raw[main_diag_info.cma_exclusion_mask.offset] &
            main_diag_info.cma_exclusion_mask.value);
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
                    return (ctx.agg->stay.exit.mode == ghm_node.u.test.params[0]);
                } break;
                case 1: {
                    return (ctx.agg->stay.exit.destination == ghm_node.u.test.params[0]);
                } break;
                case 2: {
                    return (ctx.agg->stay.entry.mode == ghm_node.u.test.params[0]);
                } break;
                case 3: {
                    return (ctx.agg->stay.entry.origin == ghm_node.u.test.params[0]);
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
            SetError(out_errors, 1, ghm_node.u.test.params[0]);
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
            return SetError(out_errors, 3, 4);
        }

        // FIXME: Check ghm_node_idx against CountOf()
        const GhmDecisionNode &ghm_node = agg.index->ghm_nodes[ghm_node_idx];
        switch (ghm_node.type) {
            case GhmDecisionNode::Type::Test: {
                int function_ret = ExecuteGhmTest(ctx, ghm_node, out_errors);
                if (UNLIKELY(function_ret < 0 || function_ret >= ghm_node.u.test.children_count)) {
                    LogError("Result for GHM tree test %1 out of range (%2 - %3)",
                             ghm_node.u.test.function, 0, ghm_node.u.test.children_count);
                    return SetError(out_errors, 3, 4);
                }

                ghm_node_idx = ghm_node.u.test.children_idx + function_ret;
            } break;

            case GhmDecisionNode::Type::Ghm: {
                ghm = ghm_node.u.ghm.ghm;
                if (ghm_node.u.ghm.error && out_errors) {
                    SetError(out_errors, ghm.parts.seq, ghm_node.u.ghm.error);
                }
            } break;
        }
    }

    return ghm;
}

GhmCode RunGhmSeverity(const ClassifyAggregate &agg, GhmCode ghm,
                       ClassifyErrorSet *out_errors)
{
    const GhmRootInfo *ghm_root_info = agg.index->FindGhmRoot(ghm.Root());
    if (UNLIKELY(!ghm_root_info)) {
        LogError("Unknown GHM root '%1'", ghm.Root());
        return SetError(out_errors, 3, 4);
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
            // TODO: Check boundaries
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

        const DiagnosisInfo *main_diag_info = agg.index->FindDiagnosis(agg.stay.main_diagnosis);
        const DiagnosisInfo *linked_diag_info = agg.index->FindDiagnosis(agg.stay.linked_diagnosis);
        for (const DiagnosisCode &diag: agg.diagnoses) {
            if (diag == agg.stay.main_diagnosis || diag == agg.stay.linked_diagnosis)
                continue;

            const DiagnosisInfo *diag_info = agg.index->FindDiagnosis(diag);
            if (UNLIKELY(!diag_info))
                continue;

            // TODO: Check boundaries (ghm_root CMA exclusion offset, etc.)
            int new_severity = diag_info->Attributes(agg.stay.sex).severity;
            if (new_severity > severity &&
                    !(agg.age < 14 && diag_info->Attributes(agg.stay.sex).raw[19] & 0x10) &&
                    !(agg.age >= 2 && diag_info->Attributes(agg.stay.sex).raw[19] & 0x8) &&
                    !(agg.age >= 2 && diag.str[0] == 'P') &&
                    !(diag_info->Attributes(agg.stay.sex).raw[ghm_root_info->cma_exclusion_mask.offset] &
                      ghm_root_info->cma_exclusion_mask.value) &&
                    !TestExclusion(*agg.index, *diag_info, *main_diag_info) &&
                    (!linked_diag_info || !TestExclusion(*agg.index, *diag_info, *linked_diag_info))) {
                severity = new_severity;
            }
        }

        if (agg.age >= ghm_root_info->old_age_treshold &&
                severity < ghm_root_info->old_severity_limit) {
            severity++;
        } else if (agg.age < ghm_root_info->young_age_treshold &&
                   severity < ghm_root_info->young_severity_limit) {
            severity++;
        } else if (agg.stay.exit.mode == 9 && !severity) {
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
    if (agg.duration > 0 && agg.stays[0].entry.mode == 8 &&
            agg.stays[agg.stays.len - 1].exit.mode == 8) {
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
    bool prev_reanimation = (agg.stays[0].entry.mode == 7 && agg.stays[0].entry.origin == 34);

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
                    *counter += stay.exit.date - stay.entry.date + (stay.exit.mode == 9) - 1;
                }
                (*ambu_counter)++;
            } else if (counter) {
                *counter += stay.exit.date - stay.entry.date + (stay.exit.mode == 9);
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

    return PriceGhs(*price_info, agg.duration, agg.stay.exit.mode == 9);
}

void Classify(const TableSet &table_set, const AuthorizationSet &authorization_set,
              Span<const Stay> stays, ClusterMode cluster_mode,
              ClassifyResultSet *out_result_set)
{
    if (!stays.len)
        return;

    static const int set_size = 2048;

    HeapArray<Span<const Stay>> stays_sets;
    Size results_count = 0;
    {
        Span<const Stay> task_stays = stays[0];
        for (Size i = 1; i < stays.len; i++) {
            if (!AreStaysCompatible(stays[i - 1], stays[i], cluster_mode)) {
                results_count++;
                if (results_count % set_size == 0) {
                    stays_sets.Append(task_stays);
                    task_stays = MakeSpan(&stays[i], 0);
                }
            }
            task_stays.len++;
        }
        results_count++;
        stays_sets.Append(task_stays);
    }

    out_result_set->results.Grow(results_count);

    Async async;
    for (Size i = 0; i < stays_sets.len; i++) {
        async.AddTask([&, i]() {
            Span<const Stay> task_stays = stays_sets[i];

            // Reuse data structures to reduce heap allocations
            // (around 5% faster on typical sets on my old MacBook)
            ClassifyErrorSet errors;
            HeapArray<DiagnosisCode> diagnoses;
            HeapArray<ProcedureRealisation> procedures;

            for (Size j = out_result_set->results.len; task_stays.len; j++) {
                ClassifyResult result = {};
                ClassifyAggregate agg;

                errors.main_error = 0;
                diagnoses.Clear(256);
                procedures.Clear(512);

                do {
                    result.stays = Cluster(task_stays, cluster_mode, &task_stays);

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

                out_result_set->results.ptr[i * set_size + j] = result;
            }
            return true;
        });
    }
    async.Sync();

    out_result_set->results.len += results_count;
    for (const ClassifyResult &result: out_result_set->results) {
        out_result_set->ghs_total_cents += result.ghs_price_cents;
        out_result_set->supplements.rea += result.supplements.rea;
        out_result_set->supplements.reasi += result.supplements.reasi;
        out_result_set->supplements.si += result.supplements.si;
        out_result_set->supplements.src += result.supplements.src;
        out_result_set->supplements.rep += result.supplements.rep;
        out_result_set->supplements.nn1 += result.supplements.nn1;
        out_result_set->supplements.nn2 += result.supplements.nn2;
        out_result_set->supplements.nn3 += result.supplements.nn3;
    }
}
