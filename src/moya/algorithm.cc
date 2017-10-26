/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "kutil.hh"
#include "algorithm.hh"
#include "data.hh"
#include "tables.hh"

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

static uint8_t GetDiagnosisByte(const TableIndex &index, Sex sex,
                                DiagnosisCode diag, uint8_t byte_idx)
{
    const DiagnosisInfo *diag_info = index.FindDiagnosis(diag);

    // FIXME: Warning / classifier errors
    if (UNLIKELY(!diag_info))
        return 0;
    if (UNLIKELY(byte_idx >= sizeof(DiagnosisInfo::attributes[0].raw)))
        return 0;

    return diag_info->Attributes(sex).raw[byte_idx];
}

static uint8_t GetProcedureByte(const TableIndex &index,
                                const ProcedureRealisation &proc, uint8_t byte_idx)
{
    const ProcedureInfo *proc_info = index.FindProcedure(proc.proc, proc.phase, proc.date);

    if (UNLIKELY(!proc_info))
        return 0;
    if (UNLIKELY(byte_idx >= sizeof(ProcedureInfo::bytes)))
        return 0;

    return proc_info->bytes[byte_idx];
}

static bool AreStaysCompatible(const Stay stay1, const Stay &stay2)
{
    return stay2.stay_id == stay1.stay_id &&
           !stay2.session_count &&
           (stay2.entry.mode == 6 || stay2.entry.mode == 0);
}

ArrayRef<const Stay> Cluster(ArrayRef<const Stay> stays, ClusterMode mode,
                             ArrayRef<const Stay> *out_remainder)
{
    if (!stays.len)
        return {};

    size_t agg_len = 0;
    switch (mode) {
        case ClusterMode::StayModes: {
            agg_len = 1;
            if (!stays[0].session_count) {
                while (agg_len < stays.len &&
                       AreStaysCompatible(stays[agg_len - 1], stays[agg_len])) {
                    agg_len++;
                }
            }
        } break;

        case ClusterMode::BillId: {
            agg_len = 1;
            while (agg_len < stays.len &&
                   stays[agg_len - 1].bill_id == stays[agg_len].bill_id) {
                agg_len++;
            }
        } break;

        case ClusterMode::Disable: {
            agg_len = 1;
        } break;
    }
    DebugAssert(agg_len);

    if (out_remainder) {
        *out_remainder = stays.Take(agg_len, stays.len - agg_len);
    }
    return stays.Take(0, agg_len);
}

static const Stay *FindMainStay(const TableIndex &index, ArrayRef<const Stay> stays,
                                int duration)
{
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
        int stay_duration = stay.dates[1] - stay.dates[0];
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
            if (GetDiagnosisByte(index, stay.sex, stay.main_diagnosis, 21) & 0x4) {
                last_trauma_stay = &stay;
                if (stay_duration > max_duration) {
                    trauma_stay = &stay;
                }
            } else {
                ignore_trauma = true;
            }
        }

        if (GetDiagnosisByte(index, stay.sex, stay.main_diagnosis, 21) & 0x20) {
            stay_score += 150;
        } else if (stay_duration >= 2) {
            base_score += 100;
        }
        if (stay_duration == 0) {
            stay_score += 2;
        } else if (stay_duration == 1) {
            stay_score++;
        }
        if (GetDiagnosisByte(index, stay.sex, stay.main_diagnosis, 21) & 0x2) {
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

// FIXME: Check Stay invariants before classification (all diag and proc exist, etc.)
GhmCode Aggregate(const TableSet &table_set, ArrayRef<const Stay> stays,
                  ClassifyAggregate *out_agg, HeapArray<int16_t> *out_errors)
{
    Assert(stays.len > 0);

    out_agg->stays = stays;

    {
        Date date = stays[stays.len - 1].dates[1];
        out_agg->index = table_set.FindIndex(date);
        if (!out_agg->index) {
            LogError("No table available on '%1'", stays[stays.len - 1].dates[1]);
            out_errors->Append(502);
            return GhmCode::FromString("90Z03Z");
        }
    }

    bool valid = true;

    out_agg->stay = stays[0];
    out_agg->age = ComputeAge(out_agg->stay.dates[0], out_agg->stay.birthdate);
    out_agg->duration = 0;
    for (const Stay &stay: stays) {
        if (!stay.main_diagnosis.IsValid()) {
            out_errors->Append(40);
            valid = false;
        }

        if (stay.gestational_age > 0) {
            // TODO: Must be first (newborn) or on RUM with a$41.2 only
            out_agg->stay.gestational_age = stay.gestational_age;
        }
        if (stay.igs2 > out_agg->stay.igs2) {
            out_agg->stay.igs2 = stay.igs2;
        }
        out_agg->duration += stay.dates[1] - stay.dates[0];
    }
    out_agg->stay.dates[1] = stays[stays.len - 1].dates[1];
    out_agg->stay.exit = stays[stays.len - 1].exit;
    out_agg->stay.diagnoses = {};
    out_agg->stay.procedures = {};

    // Consistency checks
    if (!stays[0].birthdate.value) {
        if (stays[0].error_mask & (uint32_t)Stay::Error::MalformedBirthdate) {
            out_errors->Append(14);
        } else {
            out_errors->Append(13);
        }
        valid = false;
    } else if (!stays[0].birthdate.IsValid()) {
        out_errors->Append(39);
        valid = false;
    }
    for (size_t i = 1; i < stays.len; i++) {
        if (stays[i].birthdate != stays[0].birthdate) {
            out_errors->Append(45);
            valid = false;
        }
        if (stays[i].sex != stays[0].sex) {
            out_errors->Append(46);
            valid = false;
        }
    }

    // Deduplicate diagnoses
    {
        out_agg->diagnoses.Clear(128);
        for (const Stay &stay: stays) {
            out_agg->diagnoses.Append(stay.diagnoses);
        }

        std::sort(out_agg->diagnoses.begin(), out_agg->diagnoses.end(),
                  [](DiagnosisCode code1, DiagnosisCode code2) {
            return code1.value < code2.value;
        });

        size_t j = 0;
        for (size_t i = 1; i < out_agg->diagnoses.len; i++) {
            if (out_agg->diagnoses[i] != out_agg->diagnoses[j]) {
                out_agg->diagnoses[++j] = out_agg->diagnoses[i];
            }
        }
        out_agg->diagnoses.RemoveFrom(j + 1);
    }

    // Deduplicate procedures
    {
        out_agg->procedures.Clear(512);
        for (const Stay &stay: stays) {
            out_agg->procedures.Append(stay.procedures);
        }

        std::sort(out_agg->procedures.begin(),out_agg->procedures.end(),
                  [](const ProcedureRealisation &proc1, const ProcedureRealisation &proc2) {
            return MultiCmp(proc1.proc.value - proc2.proc.value,
                            proc1.phase - proc2.phase) < 0;
        });

        // TODO: Warn when we deduplicate procedures with different attributes,
        // such as when the two procedures fall into different date ranges / limits.
        size_t j = 0;
        for (size_t i = 1; i < out_agg->procedures.len; i++) {
            if (out_agg->procedures[i].proc == out_agg->procedures[j].proc &&
                    out_agg->procedures[i].phase == out_agg->procedures[j].phase) {
                out_agg->procedures[j].activities |= out_agg->procedures[i].activities;
                out_agg->procedures[j].count += out_agg->procedures[i].count;
                if (UNLIKELY(out_agg->procedures[j].count > 9999)) {
                    out_agg->procedures[j].count = 9999;
                }
            } else {
                out_agg->procedures[++j] = out_agg->procedures[i];
            }
        }
        out_agg->procedures.RemoveFrom(j + 1);
    }

    if (stays.len > 1) {
        const Stay *main_stay = FindMainStay(*out_agg->index, stays, out_agg->duration);

        out_agg->stay.main_diagnosis = main_stay->main_diagnosis;
        out_agg->stay.linked_diagnosis = main_stay->linked_diagnosis;
    }

    if (valid) {
        return {};
    } else {
        return GhmCode::FromString("90Z00Z");
    }
}

static bool TestExclusion(const TableIndex &index,
                          const DiagnosisInfo &cma_diag_info,
                          const DiagnosisInfo &main_diag_info)
{
    // TODO: Check boundaries, and take care of DumpDiagnosis too
    const ExclusionInfo *excl = &index.exclusions[cma_diag_info.exclusion_set_idx];
    if (UNLIKELY(!excl))
        return false;
    return (excl->raw[main_diag_info.cma_exclusion_offset] & main_diag_info.cma_exclusion_mask);
}

int GetMinimalDurationForSeverity(int severity)
{
    DebugAssert(severity && severity < 4);
    return severity ? (severity + 2) : 0;
}

int LimitSeverityWithDuration(int severity, int duration)
{
    DebugAssert(severity && severity < 4);
    return duration >= 3 ? std::min(duration - 2, severity) : 0;
}

static int ExecuteGhmTest(RunGhmTreeContext &ctx, const GhmDecisionNode &ghm_node,
                          HeapArray<int16_t> *out_errors)
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
                uint8_t proc_byte = GetProcedureByte(*ctx.agg->index, proc, ghm_node.u.test.params[0]);
                if (proc_byte & ghm_node.u.test.params[1])
                    return 1;
            }
            return 0;
        } break;

        case 3: {
            if (ghm_node.u.test.params[1] == 1) {
                int age_days = ctx.agg->stay.dates[0] - ctx.agg->stay.birthdate;
                return (age_days > ghm_node.u.test.params[0]);
            } else {
                return (ctx.agg->age > ghm_node.u.test.params[0]);
            }
        } break;

        case 5: {
            uint8_t diag_byte = GetDiagnosisByte(*ctx.agg->index, ctx.agg->stay.sex,
                                                 ctx.main_diagnosis, ghm_node.u.test.params[0]);
            return ((diag_byte & ghm_node.u.test.params[1]) != 0);
        } break;

        case 6: {
            // NOTE: Incomplete, should behave differently when params[0] >= 128,
            // but it's probably relevant only for FG 9 and 10 (CMAs)
            for (DiagnosisCode diag: ctx.agg->diagnoses) {
                if (diag == ctx.main_diagnosis || diag == ctx.linked_diagnosis)
                    continue;

                uint8_t diag_byte = GetDiagnosisByte(*ctx.agg->index, ctx.agg->stay.sex,
                                                     diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;

        case 7: {
            for (const DiagnosisCode &diag: ctx.agg->diagnoses) {
                uint8_t diag_byte = GetDiagnosisByte(*ctx.agg->index, ctx.agg->stay.sex,
                                                     diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1])
                    return 1;
            }
            return 0;
        } break;

        case 9: {
            int result = 0;
            for (const ProcedureRealisation &proc: ctx.agg->procedures) {
                if (GetProcedureByte(*ctx.agg->index, proc, 0) & 0x80) {
                    uint8_t proc_byte = GetProcedureByte(*ctx.agg->index, proc,
                                                         ghm_node.u.test.params[0]);
                    if (proc_byte & ghm_node.u.test.params[1]) {
                        result = 1;
                    } else {
                        return 0;
                    }
                }
            }
            return result;
        } break;

        case 10: {
            size_t matches = 0;
            for (const ProcedureRealisation &proc: ctx.agg->procedures) {
                uint8_t proc_byte = GetProcedureByte(*ctx.agg->index, proc,
                                                     ghm_node.u.test.params[0]);
                if (proc_byte & ghm_node.u.test.params[1]) {
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
            size_t matches = 0, special_matches = 0;
            for (DiagnosisCode diag: ctx.agg->diagnoses) {
                uint8_t diag_byte = GetDiagnosisByte(*ctx.agg->index, ctx.agg->stay.sex,
                                                     diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1]) {
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
            uint8_t diag_byte = GetDiagnosisByte(*ctx.agg->index, ctx.agg->stay.sex,
                                                 ctx.agg->stay.linked_diagnosis,
                                                 ghm_node.u.test.params[0]);
            return ((diag_byte & ghm_node.u.test.params[1]) != 0);
        } break;

        case 28: {
            out_errors->Append(ghm_node.u.test.params[0]);
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

                uint8_t diag_byte = GetDiagnosisByte(*ctx.agg->index, ctx.agg->stay.sex,
                                                     diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1])
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

GhmCode RunGhmTree(const ClassifyAggregate &agg, HeapArray<int16_t> *out_errors)
{
    GhmCode ghm = {};

    RunGhmTreeContext ctx = {};
    ctx.agg = &agg;
    ctx.main_diagnosis = agg.stay.main_diagnosis;
    ctx.linked_diagnosis = agg.stay.linked_diagnosis;

    size_t ghm_node_idx = 0;
    for (size_t i = 0; !ghm.IsValid(); i++) {
        if (UNLIKELY(i >= agg.index->ghm_nodes.len)) {
            LogError("Empty GHM tree or infinite loop (%2)", agg.index->ghm_nodes.len);
            out_errors->Append(4);
            return GhmCode::FromString("90Z03Z");
        }

        // FIXME: Check ghm_node_idx against CountOf()
        const GhmDecisionNode &ghm_node = agg.index->ghm_nodes[ghm_node_idx];
        switch (ghm_node.type) {
            case GhmDecisionNode::Type::Test: {
                int function_ret = ExecuteGhmTest(ctx, ghm_node, out_errors);
                if (UNLIKELY(function_ret < 0 ||
                             (size_t)function_ret >= ghm_node.u.test.children_count)) {
                    LogError("Result for GHM tree test %1 out of range (%2 - %3)",
                             ghm_node.u.test.function, 0, ghm_node.u.test.children_count);
                    out_errors->Append(4);
                    return GhmCode::FromString("90Z03Z");
                }

                ghm_node_idx = ghm_node.u.test.children_idx + (size_t)function_ret;
            } break;

            case GhmDecisionNode::Type::Ghm: {
                ghm = ghm_node.u.ghm.ghm;
                if (ghm_node.u.ghm.error) {
                    out_errors->Append(ghm_node.u.ghm.error);
                }
            } break;
        }
    }

    return ghm;
}

GhmCode RunGhmSeverity(const ClassifyAggregate &agg, GhmCode ghm,
                       HeapArray<int16_t> *out_errors)
{
    const GhmRootInfo *ghm_root_info = agg.index->FindGhmRoot(ghm.Root());
    if (UNLIKELY(!ghm_root_info)) {
        LogError("Unknown GHM root '%1'", ghm.Root());
        out_errors->Append(4);
        return GhmCode::FromString("90Z03Z");
    }

    // Ambulatory and / or short duration GHM
    if (ghm_root_info->allow_ambulatory && agg.duration == 0) {
        ghm.parts.mode = 'J';
    } else if (ghm_root_info->short_duration_treshold &&
               agg.duration < ghm_root_info->short_duration_treshold) {
        ghm.parts.mode = 'T';
    } else if (ghm.parts.mode >= 'A' && ghm.parts.mode <= 'D') {
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

        ghm.parts.mode = 'A' + (char)LimitSeverityWithDuration(severity, agg.duration);
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
                    !(diag_info->Attributes(agg.stay.sex).raw[ghm_root_info->cma_exclusion_offset] &
                      ghm_root_info->cma_exclusion_mask) &&
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

        ghm.parts.mode = '1' + (char)LimitSeverityWithDuration(severity, agg.duration);
    }

    return ghm;
}

GhmCode ClassifyGhm(const ClassifyAggregate &agg, HeapArray<int16_t> *out_errors)
{
    GhmCode ghm;

    ghm = RunGhmTree(agg, out_errors);
    ghm = RunGhmSeverity(agg, ghm, out_errors);

    return ghm;
}

GhsCode ClassifyGhs(const ClassifyAggregate &agg, const AuthorizationSet &authorization_set,
                    GhmCode ghm)
{
    ArrayRef<const GhsInfo> compatible_ghs = agg.index->FindCompatibleGhs(ghm);

    for (const GhsInfo &ghs_info: compatible_ghs) {
        if (ghs_info.minimal_age && agg.age < ghs_info.minimal_age)
            continue;

        int duration;
        if (ghs_info.unit_authorization) {
            duration = 0;
            bool authorized = false;
            for (const Stay &stay: agg.stays) {
                const Authorization *auth = authorization_set.FindUnit(stay.unit, stay.dates[1]);
                if (auth && auth->type == ghs_info.unit_authorization) {
                    duration += stay.dates[1] - stay.dates[0];
                    authorized = true;
                }
            }
            if (!authorized)
                continue;
        } else {
            duration = agg.duration;
        }
        if (ghs_info.bed_authorization &&
                std::none_of(agg.stays.begin(), agg.stays.end(),
                             [&](const Stay &stay) { return stay.bed_authorization == ghs_info.bed_authorization; }))
            continue;
        if (ghs_info.minimal_duration && duration < ghs_info.minimal_duration)
            continue;

        // TODO: Make sure we don't need DP - DR reversal here
        if (ghs_info.main_diagnosis_mask &&
                !(GetDiagnosisByte(*agg.index, agg.stay.sex, agg.stay.main_diagnosis, ghs_info.main_diagnosis_offset) &
                  ghs_info.main_diagnosis_mask))
            continue;
        if (ghs_info.diagnosis_mask &&
                std::none_of(agg.diagnoses.begin(), agg.diagnoses.end(),
                             [&](DiagnosisCode diag) { return GetDiagnosisByte(*agg.index, agg.stay.sex, diag, ghs_info.diagnosis_offset) &
                                                              ghs_info.diagnosis_mask; }))
            continue;
        if (ghs_info.proc_mask &&
                std::none_of(agg.procedures.begin(), agg.procedures.end(),
                             [&](const ProcedureRealisation &proc) { return GetProcedureByte(*agg.index, proc, ghs_info.proc_offset) &
                                                                            ghs_info.proc_mask; }))
            continue;

        return ghs_info.ghs[0];
    }

    return GhsCode(9999);
}

void Summarize(const TableSet &table_set, const AuthorizationSet &authorization_set,
               ArrayRef<const Stay> stays, ClusterMode cluster_mode,
               SummarizeResultSet *out_result_set)
{
    // Reuse data structures to reduce heap allocations
    // (around 5% faster on typical sets on my old MacBook)
    ClassifyAggregate agg;

    while (stays.len) {
        SummarizeResult result = {};

        result.errors.ptr = (int16_t *)out_result_set->store.errors.len;
        do {
            result.stays = Cluster(stays, cluster_mode, &stays);

            result.ghm = Aggregate(table_set, result.stays, &agg,
                                   &out_result_set->store.errors);
            if (UNLIKELY(result.ghm.IsError()))
                break;
            result.ghm = ClassifyGhm(agg, &out_result_set->store.errors);
            if (UNLIKELY(result.ghm.IsError()))
                break;
        } while (false);
        result.errors.len = out_result_set->store.errors.len - (size_t)result.errors.ptr;

        result.ghs = ClassifyGhs(agg, authorization_set, result.ghm);

        out_result_set->results.Append(result);
    }

    for (SummarizeResult &result: out_result_set->results) {
        result.errors.ptr = out_result_set->store.errors.ptr + (size_t)result.errors.ptr;
    }
}
