#include "kutil.hh"
#include "classifier.hh"
#include "stays.hh"
#include "tables.hh"

struct ClassifierContext {
    const Stay *stay;
    const ClassifierIndex *index;

    // Keep a copy for DP - DR reversal during GHM tree traversal (function 34)
    DiagnosisCode main_diagnosis;
    DiagnosisCode linked_diagnosis;

    // Computed values
    int duration;
    int age;
    int gnn;
};

static int ComputeAge(Date date, Date birthdate)
{
    int age = date.st.year - birthdate.st.year;
    age -= (date.st.month < birthdate.st.month ||
            (date.st.month == birthdate.st.month && date.st.day < birthdate.st.day));
    return age;
}

static uint16_t GetDiagnosisByte(ClassifierContext &ctx, DiagnosisCode diag_code,
                                 uint8_t byte_idx)
{
    // FIXME: Warning / classifier errors
    if (byte_idx >= sizeof(DiagnosisInfo::attributes[0].raw))
        return 0;

    const DiagnosisInfo *diag_info = ctx.index->FindDiagnosis(diag_code);
    if (!diag_info)
        return 0;

    return diag_info->Attributes(ctx.stay->sex).raw[byte_idx];
}

static uint8_t GetProcedureByte(ClassifierContext &ctx, const Procedure &proc,
                                uint8_t byte_idx)
{
    if (byte_idx >= sizeof(ProcedureInfo::bytes))
        return 0;

    const ProcedureInfo *proc_info = ctx.index->FindProcedure(proc.code, proc.phase, proc.date);
    if (!proc_info)
        return 0;

    return proc_info->bytes[byte_idx];
}

static const Stay *FindMainStay(ClassifierContext &ctx, ArrayRef<const Stay> stays)
{
    int max_duration = -1;
    const Stay *zx_stay = nullptr;
    const Stay *proc_stay = nullptr;
    int proc_priority = 0;
    const Stay *trauma_stay = nullptr;
    bool ignore_trauma = false;
    const Stay *score_stay = nullptr;
    int base_score = 0;
    int min_score = INT_MAX;

    int full_duration = stays[stays.len - 1].dates[1] - stays[0].dates[0];
    for (const Stay &stay: stays) {
        int part_duration = stay.dates[1] - stay.dates[0];

        for (const Procedure &proc: stay.procedures) {
            const ProcedureInfo *proc_info = ctx.index->FindProcedure(proc.code, proc.phase,
                                                                      proc.date);
            if (!proc_info)
                continue;

            if (proc_info->bytes[0] & 0x80 && !(proc_info->bytes[23] & 0x80))
                return &stay;

            if (proc_priority < 3 && proc_info->bytes[38] & 0x2) {
                proc_stay = &stay;
                proc_priority = 3;
            } else if (proc_priority < 2 && full_duration <= 1 && proc_info->bytes[39] & 0x80) {
                proc_stay = &stay;
                proc_priority = 2;
            } else if (proc_priority < 1 && full_duration == 0 && proc_info->bytes[39] & 0x40) {
                proc_stay = &stay;
                proc_priority = 1;
            }
        }

        if (part_duration > max_duration) {
            if (stay.main_diagnosis.Matches("Z515") ||
                    stay.main_diagnosis.Matches("Z502") ||
                    stay.main_diagnosis.Matches("Z503")) {
                zx_stay = &stay;
            } else {
                zx_stay = nullptr;
            }
        }

        if (!ignore_trauma) {
            if (GetDiagnosisByte(ctx, stay.main_diagnosis, 21) & 0x4) {
                if (part_duration > max_duration) {
                    trauma_stay = &stay;
                }
            } else {
                ignore_trauma = true;
            }
        }

        {
            int stay_score = base_score;

            if (GetDiagnosisByte(ctx, stay.main_diagnosis, 21) & 0x20) {
                stay_score += 150;
            } else if (part_duration >= 2) {
                base_score += 100;
            }
            if (part_duration == 0) {
                stay_score += 2;
            } else if (part_duration == 1) {
                stay_score++;
            }
            if (GetDiagnosisByte(ctx, stay.main_diagnosis, 21) & 0x2) {
                stay_score += 201;
            }

            if (stay_score < min_score) {
                score_stay = &stay;
                min_score = stay_score;
            }
        }

        if (part_duration > max_duration) {
            max_duration = part_duration;
        }
    }

    if (zx_stay)
        return zx_stay;
    if (proc_stay)
        return proc_stay;
    if (trauma_stay > score_stay)
        return trauma_stay;
    return score_stay;
}

static bool TestMatchingDiagnoses(ClassifierContext &ctx,
                                  uint8_t byte_idx, uint8_t byte_mask, size_t count)
{
    for (DiagnosisCode diag: ctx.stay->diagnoses) {
        uint8_t diag_byte = GetDiagnosisByte(ctx, diag, byte_idx);
        if (diag_byte & byte_mask && !--count)
            return true;
    }
    return false;
}

// FIXME: Completely wrong, probably need to test on unique code + phase values
static bool TestMatchingProcedures(ClassifierContext &ctx,
                                   uint8_t byte_idx, uint8_t byte_mask, size_t count)
{
    for (const Procedure &proc: ctx.stay->procedures) {
        uint8_t proc_byte = GetProcedureByte(ctx, proc, byte_idx);
        if (proc_byte & byte_mask && !--count)
            return true;
    }
    return false;
}

static int RunGhmTest(ClassifierContext &ctx, const GhmDecisionNode &ghm_node)
{
    DebugAssert(ghm_node.type == GhmDecisionNode::Type::Test);

    switch (ghm_node.u.test.function) {
        case 0:
        case 1: {
            return GetDiagnosisByte(ctx, ctx.main_diagnosis, ghm_node.u.test.params[0]);
        } break;

        case 2: {
            for (const Procedure &proc: ctx.stay->procedures) {
                uint8_t proc_byte = GetProcedureByte(ctx, proc, ghm_node.u.test.params[0]);
                if (proc_byte & ghm_node.u.test.params[1])
                    return 1;
            }
            return 0;
        } break;

        case 3: {
            if (ghm_node.u.test.params[1] == 1) {
                int age_days = ctx.stay->dates[0] - ctx.stay->birthdate;
                return (age_days > ghm_node.u.test.params[0]);
            } else {
                return (ctx.age > ghm_node.u.test.params[0]);
            }
        } break;

        case 5: {
            uint8_t diag_byte = GetDiagnosisByte(ctx, ctx.main_diagnosis,
                                                 ghm_node.u.test.params[0]);
            return ((diag_byte & ghm_node.u.test.params[1]) != 0);
        } break;

        case 6: {
            // NOTE: Incomplete, should behave differently when params[0] >= 128,
            // but it's probably relevant only for FG 9 and 10 (CMAs)
            for (DiagnosisCode diag: ctx.stay->diagnoses) {
                if (diag == ctx.main_diagnosis || diag == ctx.linked_diagnosis)
                    continue;

                uint8_t diag_byte = GetDiagnosisByte(ctx, diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;

        case 7: {
            for (const DiagnosisCode &diag: ctx.stay->diagnoses) {
                uint8_t diag_byte = GetDiagnosisByte(ctx, diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1])
                    return 1;
            }
            return 0;
        } break;

        case 9: {
            if (!ctx.stay->procedures.len)
                return 0;
            for (const Procedure &proc: ctx.stay->procedures) {
                uint8_t proc_byte = GetProcedureByte(ctx, proc, ghm_node.u.test.params[0]);
                if (!(proc_byte & ghm_node.u.test.params[1]))
                    return 0;
            }
            return 1;
        } break;

        case 10: {
            // FIXME: Wrong, should count unique matching codes
            return TestMatchingProcedures(ctx, ghm_node.u.test.params[0],
                                          ghm_node.u.test.params[1], 2);
        } break;

        case 13: {
            uint8_t diag_byte = GetDiagnosisByte(ctx, ctx.main_diagnosis,
                                                 ghm_node.u.test.params[0]);
            return (diag_byte == ghm_node.u.test.params[1]);
        } break;

        case 14: {
            return ((int)ctx.stay->sex - 1 == ghm_node.u.test.params[0] - 49);
        } break;

        case 18: {
            return TestMatchingDiagnoses(ctx, ghm_node.u.test.params[0],
                                         ghm_node.u.test.params[1], 2);
        } break;

        case 19: {
            switch (ghm_node.u.test.params[1]) {
                case 0: {
                    return (ctx.stay->exit.mode == ghm_node.u.test.params[0]);
                } break;
                case 1: {
                    return (ctx.stay->exit.destination == ghm_node.u.test.params[0]);
                } break;
                case 2: {
                    return (ctx.stay->entry.mode == ghm_node.u.test.params[0]);
                } break;
                case 3: {
                    return (ctx.stay->entry.origin == ghm_node.u.test.params[0]);
                } break;
            }
        } break;

        case 20: {
            return 0;
        } break;

        case 22: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.duration < param);
        } break;

        case 26: {
            uint8_t diag_byte = GetDiagnosisByte(ctx, ctx.stay->linked_diagnosis,
                                                 ghm_node.u.test.params[0]);
            return ((diag_byte & ghm_node.u.test.params[1]) != 0);
        } break;

        case 28: {
            PrintLn("%1 -- Erreur %2", ctx.stay->stay_id, ghm_node.u.test.params[0]);
            // TODO: Add error
            return 0;
        } break;

        case 29: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.duration == param);
        } break;

        case 30: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (ctx.stay->session_count == param);
        } break;

        case 33: {
            for (const Procedure &proc: ctx.stay->procedures) {
                if (proc.activity == ghm_node.u.test.params[0])
                    return 1;
            }

            return 0;
        } break;

        case 34: {
            if (ctx.stay->linked_diagnosis.IsValid() &&
                    ctx.main_diagnosis == ctx.stay->main_diagnosis) {
                const DiagnosisInfo *diag_info = ctx.index->FindDiagnosis(ctx.linked_diagnosis);
                if (diag_info) {
                    uint8_t cmd = diag_info->Attributes(ctx.stay->sex).cmd;
                    uint8_t jump = diag_info->Attributes(ctx.stay->sex).jump;
                    if (cmd || jump != 3) {
                        std::swap(ctx.main_diagnosis, ctx.linked_diagnosis);
                    }
                }
            }

            return 0;
        } break;

        case 35: {
            return (ctx.main_diagnosis != ctx.stay->main_diagnosis);
        } break;

        case 36: {
            for (DiagnosisCode diag: ctx.stay->diagnoses) {
                if (diag == ctx.linked_diagnosis)
                    continue;

                uint8_t diag_byte = GetDiagnosisByte(ctx, diag, ghm_node.u.test.params[0]);
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
                int gestational_age = ctx.stay->gestational_age;
                if (!gestational_age) {
                    gestational_age = 99;
                }

                for (const ValueRangeCell<2> &cell: ctx.index->gnn_cells) {
                    if (cell.Test(0, ctx.stay->newborn_weight) && cell.Test(1, gestational_age)) {
                        ctx.gnn = cell.value;
                        break;
                    }
                }
            }

            return 0;
        } break;

        case 41: {
            for (DiagnosisCode diag: ctx.stay->diagnoses) {
                const DiagnosisInfo *diag_info = ctx.index->FindDiagnosis(diag);
                if (!diag_info)
                    continue;

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
            for (DiagnosisCode diag: ctx.stay->diagnoses) {
                if (diag == ctx.linked_diagnosis)
                    continue;

                const DiagnosisInfo *diag_info = ctx.index->FindDiagnosis(diag);
                if (!diag_info)
                    continue;

                uint8_t cmd = diag_info->Attributes(ctx.stay->sex).cmd;
                uint8_t jump = diag_info->Attributes(ctx.stay->sex).jump;
                if (cmd == ghm_node.u.test.params[0] && jump == ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;
    }

    LogError("Unknown test %1 or invalid arguments", ghm_node.u.test.function);
    return -1;
}

static int LimitSeverity(ClassifierContext &ctx, int severity)
{
    if (severity == 3 && ctx.duration < 5) {
        severity = 2;
    }
    if (severity == 2 && ctx.duration < 4) {
        severity = 1;
    }
    if (severity == 1 && ctx.duration < 3) {
        severity = 0;
    }

    return severity;
}

static bool TestExclusion(ClassifierContext &ctx, const DiagnosisInfo &cma_diag_info,
                          const DiagnosisInfo &main_diag_info)
{
    // TODO: Check boundaries, and take care of DumpDiagnosis too
    const ExclusionInfo *excl = &ctx.index->exclusions[cma_diag_info.exclusion_set_idx];
    return (excl->raw[main_diag_info.cma_exclusion_offset] & main_diag_info.cma_exclusion_mask);
}

void RunGhmTree(ClassifierContext &ctx)
{
    GhmCode ghm_code = {};
    int error = 0;

    // Useful variables we'll probably need in all cases
    ctx.main_diagnosis = ctx.stay->main_diagnosis;
    ctx.linked_diagnosis = ctx.stay->linked_diagnosis;
    ctx.duration = ctx.stay->dates[1] - ctx.stay->dates[0];
    ctx.age = ComputeAge(ctx.stay->dates[0], ctx.stay->birthdate);

    // Run the GHM tree
    size_t ghm_node_idx = 0;
    for (size_t i = 0; !ghm_code.IsValid(); i++) {
        if (i >= ctx.index->ghm_nodes.len) {
            LogError("Infinite GHM tree iteration");
            return;
        }

        const GhmDecisionNode &ghm_node = ctx.index->ghm_nodes[ghm_node_idx];
        switch (ghm_node.type) {
            case GhmDecisionNode::Type::Test: {
                int function_ret = RunGhmTest(ctx, ghm_node);
                if (function_ret < 0 || (size_t)function_ret >= ghm_node.u.test.children_count) {
                    PrintLn("%1 -- Failed (function %2)", ctx.stay->stay_id,
                            ghm_node.u.test.function);
                    return;
                }

                ghm_node_idx = ghm_node.u.test.children_idx + function_ret;
            } break;

            case GhmDecisionNode::Type::Ghm: {
                ghm_code = ghm_node.u.ghm.code;
                error = ghm_node.u.ghm.error;
            } break;
        }
    }

    const GhmRootInfo *ghm_root = ctx.index->FindGhmRoot(ghm_code.Root());
    if (!ghm_root) {
        LogError("Unknown GHM root '%1'", ghm_code.Root());
        return;
    }

    // Ambulatory and / or short duration GHM
    if (ghm_root->allow_ambulatory && ctx.duration == 0) {
        ghm_code.parts.mode = 'J';
    } else if (ghm_root->short_duration_treshold &&
               ctx.duration < ghm_root->short_duration_treshold) {
        ghm_code.parts.mode = 'T';
    }

    // Apply mode / severity algorithm
    if (ghm_code.parts.mode >= 'A' && ghm_code.parts.mode <= 'D') {
        if (ghm_root->childbirth_severity_list) {
            int severity = ghm_code.parts.mode - 'A';

            // TODO: Check boundaries
            for (const ValueRangeCell<2> &cell: ctx.index->cma_cells[ghm_root->childbirth_severity_list - 1]) {
                if (cell.Test(0, ctx.stay->gestational_age) && cell.Test(1, severity)) {
                    severity = cell.value;
                    break;
                }
            }

            ghm_code.parts.mode = 'A' + LimitSeverity(ctx, severity);
        }
    } else if (!ghm_code.parts.mode) {
        int severity = 0;

        const DiagnosisInfo *main_diag_info = ctx.index->FindDiagnosis(ctx.main_diagnosis);
        const DiagnosisInfo *linked_diag_info = ctx.index->FindDiagnosis(ctx.linked_diagnosis);
        for (const DiagnosisCode &diag: ctx.stay->diagnoses) {
            if (diag == ctx.main_diagnosis || diag == ctx.linked_diagnosis)
                continue;

            const DiagnosisInfo *diag_info = ctx.index->FindDiagnosis(diag);
            if (!diag_info)
                continue;

            // TODO: Check boundaries (ghm_root CMA exclusion offset, etc.)
            int new_severity = diag_info->Attributes(ctx.stay->sex).severity;
            if (new_severity > severity &&
                    !(ctx.age < 14 && diag_info->Attributes(ctx.stay->sex).raw[19] & 0x10) &&
                    !(ctx.age >= 2 && diag_info->Attributes(ctx.stay->sex).raw[19] & 0x8) &&
                    !(ctx.age >= 2 && diag.str[0] == 'P') &&
                    !(diag_info->Attributes(ctx.stay->sex).raw[ghm_root->cma_exclusion_offset] &
                      ghm_root->cma_exclusion_mask) &&
                    !TestExclusion(ctx, *diag_info, *main_diag_info) &&
                    (!linked_diag_info || !TestExclusion(ctx, *diag_info, *linked_diag_info))) {
                severity = new_severity;
            }
        }

        if (ctx.age >= ghm_root->old_age_treshold && severity < ghm_root->old_severity_limit) {
            severity++;
        } else if (ctx.age < ghm_root->young_age_treshold && severity < ghm_root->young_severity_limit) {
            severity++;
        } else if (ctx.stay->exit.mode == 9 && !severity) {
            severity = 1;
        }

        ghm_code.parts.mode = '1' + LimitSeverity(ctx, severity);
    }

    Print("%1 -- %2", ctx.stay->stay_id, ghm_code);
    if (error) {
        Print(" (err = %1)", error);
    }
#ifdef TESTING
    if (ctx.stay->test.ghm != ghm_code) {
        Print(" DIFFERENT (%1)", ctx.stay->test.ghm);
    }
#endif
    PrintLn();
}

// FIXME: Check Stay invariants before classification (all diag and proc exist, etc.)
bool Classify(const ClassifierSet &classifier_set, const StaySet &stay_set,
              ResultSet *out_result_set)
{
    for (size_t i = 0; i < stay_set.stays.len;) {
        ClassifierContext ctx = {};

        ArrayRef<const Stay> stays = stay_set.stays.Take(i, 1);
        while (i + stays.len < stay_set.stays.len &&
               stay_set.stays[i + stays.len].stay_id == stays[0].stay_id &&
               (stays[stays.len - 1].exit.mode == 6 || stays[stays.len - 1].exit.mode == 0)) {
            stays.len++;
        }

        ctx.index = classifier_set.FindIndex(stays[stays.len - 1].dates[1]);
        if (!ctx.index) {
            PrintLn("Damn it: %1", stays[stays.len - 1].dates[1]);
            continue;
        }

        ctx.stay = &stays[0];
        if (stays.len > 1) {
            Stay synth_stay = {};
            {
                const Stay *main_stay = FindMainStay(ctx, stays);

                synth_stay.main_diagnosis = main_stay->main_diagnosis;
                synth_stay.linked_diagnosis = main_stay->linked_diagnosis;
                synth_stay.diagnoses.ptr = stays[0].diagnoses.ptr;
                synth_stay.diagnoses.len = stays[stays.len - 1].diagnoses.ptr + stays[stays.len - 1].diagnoses.len - stays[0].diagnoses.ptr;
                synth_stay.procedures.ptr = stays[0].procedures.ptr;
                synth_stay.procedures.len = stays[stays.len - 1].procedures.ptr + stays[stays.len - 1].procedures.len - stays[0].procedures.ptr;
                synth_stay.dates[0] = stays[0].dates[0];
                synth_stay.dates[1] = stays[stays.len - 1].dates[1];
                synth_stay.birthdate = stays[0].birthdate;
                synth_stay.entry = stays[0].entry;
                synth_stay.exit = stays[stays.len - 1].exit;
                synth_stay.gestational_age = stays[0].gestational_age; // FIXME: Not sure where I should take it
                synth_stay.last_menstrual_period = stays[0].last_menstrual_period;
                synth_stay.igs2 = stays[0].igs2;
                synth_stay.newborn_weight = stays[0].newborn_weight;
                synth_stay.session_count = stays[0].session_count;
                synth_stay.sex = stays[0].sex;
                synth_stay.stay_id = stays[0].stay_id;
#ifdef TESTING
                synth_stay.test.ghm = stays[0].test.ghm;
#endif
            }

            ctx.stay = &synth_stay;
            RunGhmTree(ctx);
        } else {
            RunGhmTree(ctx);
        }

        i += stays.len;
    }

    return true;
}
