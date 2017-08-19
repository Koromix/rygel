#include "kutil.hh"
#include "classifier.hh"
#include "stays.hh"
#include "tables.hh"

static int ComputeAge(Date date, Date birthdate)
{
    int age = date.st.year - birthdate.st.year;
    age -= (date.st.month < birthdate.st.month ||
            (date.st.month == birthdate.st.month && date.st.day < birthdate.st.day));
    return age;
}

uint8_t Classifier::GetDiagnosisByte(DiagnosisCode diag_code, uint8_t byte_idx)
{
    // FIXME: Warning / classifier errors
    if (UNLIKELY(byte_idx >= sizeof(DiagnosisInfo::attributes[0].raw)))
        return 0;

    const DiagnosisInfo *diag_info = index->FindDiagnosis(diag_code);
    if (!diag_info)
        return 0;

    return diag_info->Attributes(agg.stay.sex).raw[byte_idx];
}

uint8_t Classifier::GetProcedureByte(const Procedure &proc, uint8_t byte_idx)
{
    if (UNLIKELY(byte_idx >= sizeof(ProcedureInfo::bytes)))
        return 0;

    const ProcedureInfo *proc_info = index->FindProcedure(proc.code, proc.phase, proc.date);
    if (!proc_info)
        return 0;

    return proc_info->bytes[byte_idx];
}

const Stay *Classifier::FindMainStay()
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
        for (const Procedure &proc: stay.procedures) {
            const ProcedureInfo *proc_info = index->FindProcedure(proc.code, proc.phase,
                                                                  proc.date);
            if (!proc_info)
                continue;

            if (proc_info->bytes[0] & 0x80 && !(proc_info->bytes[23] & 0x80))
                return &stay;

            if (proc_priority < 3 && proc_info->bytes[38] & 0x2) {
                proc_priority = 3;
            } else if (proc_priority < 2 && agg.duration <= 1 && proc_info->bytes[39] & 0x80) {
                proc_priority = 2;
            } else if (proc_priority < 1 && agg.duration == 0 && proc_info->bytes[39] & 0x40) {
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
            if (GetDiagnosisByte(stay.main_diagnosis, 21) & 0x4) {
                last_trauma_stay = &stay;
                if (stay_duration > max_duration) {
                    trauma_stay = &stay;
                }
            } else {
                ignore_trauma = true;
            }
        }

        if (GetDiagnosisByte(stay.main_diagnosis, 21) & 0x20) {
            stay_score += 150;
        } else if (stay_duration >= 2) {
            base_score += 100;
        }
        if (stay_duration == 0) {
            stay_score += 2;
        } else if (stay_duration == 1) {
            stay_score++;
        }
        if (GetDiagnosisByte(stay.main_diagnosis, 21) & 0x2) {
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

int Classifier::ExecuteGhmTest(const GhmDecisionNode &ghm_node,
                               HeapArray<int16_t> *out_errors)
{
    DebugAssert(ghm_node.type == GhmDecisionNode::Type::Test);

    switch (ghm_node.u.test.function) {
        case 0:
        case 1: {
            return GetDiagnosisByte(main_diagnosis, ghm_node.u.test.params[0]);
        } break;

        case 2: {
            for (const Procedure &proc: procedures) {
                uint8_t proc_byte = GetProcedureByte(proc, ghm_node.u.test.params[0]);
                if (proc_byte & ghm_node.u.test.params[1])
                    return 1;
            }
            return 0;
        } break;

        case 3: {
            if (ghm_node.u.test.params[1] == 1) {
                int age_days = agg.stay.dates[0] - agg.stay.birthdate;
                return (age_days > ghm_node.u.test.params[0]);
            } else {
                return (agg.age > ghm_node.u.test.params[0]);
            }
        } break;

        case 5: {
            uint8_t diag_byte = GetDiagnosisByte(main_diagnosis, ghm_node.u.test.params[0]);
            return ((diag_byte & ghm_node.u.test.params[1]) != 0);
        } break;

        case 6: {
            // NOTE: Incomplete, should behave differently when params[0] >= 128,
            // but it's probably relevant only for FG 9 and 10 (CMAs)
            for (DiagnosisCode diag: diagnoses) {
                if (diag == main_diagnosis || diag == linked_diagnosis)
                    continue;

                uint8_t diag_byte = GetDiagnosisByte(diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;

        case 7: {
            for (const DiagnosisCode &diag: diagnoses) {
                uint8_t diag_byte = GetDiagnosisByte(diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1])
                    return 1;
            }
            return 0;
        } break;

        case 9: {
            int result = 0;
            for (const Procedure &proc: procedures) {
                if (GetProcedureByte(proc, 0) & 0x80) {
                    uint8_t proc_byte = GetProcedureByte(proc, ghm_node.u.test.params[0]);
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
            for (const Procedure &proc: procedures) {
                uint8_t proc_byte = GetProcedureByte(proc, ghm_node.u.test.params[0]);
                if (proc_byte & ghm_node.u.test.params[1]) {
                    matches++;
                    if (matches >= 2)
                        return 1;
                }
            }
            return 0;
        } break;

        case 13: {
            uint8_t diag_byte = GetDiagnosisByte(main_diagnosis, ghm_node.u.test.params[0]);
            return (diag_byte == ghm_node.u.test.params[1]);
        } break;

        case 14: {
            return ((int)agg.stay.sex - 1 == ghm_node.u.test.params[0] - 49);
        } break;

        case 18: {
            size_t matches = 0, special_matches = 0;
            for (DiagnosisCode diag: diagnoses) {
                uint8_t diag_byte = GetDiagnosisByte(diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1]) {
                    matches++;
                    if (diag == main_diagnosis || diag == linked_diagnosis) {
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
                    return (agg.stay.exit.mode == ghm_node.u.test.params[0]);
                } break;
                case 1: {
                    return (agg.stay.exit.destination == ghm_node.u.test.params[0]);
                } break;
                case 2: {
                    return (agg.stay.entry.mode == ghm_node.u.test.params[0]);
                } break;
                case 3: {
                    return (agg.stay.entry.origin == ghm_node.u.test.params[0]);
                } break;
            }
        } break;

        case 20: {
            return 0;
        } break;

        case 22: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (agg.duration < param);
        } break;

        case 26: {
            uint8_t diag_byte = GetDiagnosisByte(agg.stay.linked_diagnosis,
                                                 ghm_node.u.test.params[0]);
            return ((diag_byte & ghm_node.u.test.params[1]) != 0);
        } break;

        case 28: {
            out_errors->Append(ghm_node.u.test.params[0]);
            return 0;
        } break;

        case 29: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (agg.duration == param);
        } break;

        case 30: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (agg.stay.session_count == param);
        } break;

        case 33: {
            for (const Procedure &proc: procedures) {
                if (proc.activities & (1 << ghm_node.u.test.params[0]))
                    return 1;
            }

            return 0;
        } break;

        case 34: {
            if (linked_diagnosis.IsValid() && main_diagnosis == agg.stay.main_diagnosis) {
                const DiagnosisInfo *diag_info = index->FindDiagnosis(linked_diagnosis);
                if (diag_info) {
                    uint8_t cmd = diag_info->Attributes(agg.stay.sex).cmd;
                    uint8_t jump = diag_info->Attributes(agg.stay.sex).jump;
                    if (cmd || jump != 3) {
                        std::swap(main_diagnosis, linked_diagnosis);
                    }
                }
            }

            return 0;
        } break;

        case 35: {
            return (main_diagnosis != agg.stay.main_diagnosis);
        } break;

        case 36: {
            for (DiagnosisCode diag: agg.stay.diagnoses) {
                if (diag == linked_diagnosis)
                    continue;

                uint8_t diag_byte = GetDiagnosisByte(diag, ghm_node.u.test.params[0]);
                if (diag_byte & ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;

        case 38: {
            return (lazy.gnn >= ghm_node.u.test.params[0] &&
                    lazy.gnn <= ghm_node.u.test.params[1]);
        } break;

        case 39: {
            if (!lazy.gnn) {
                int gestational_age = agg.stay.gestational_age;
                if (!gestational_age) {
                    gestational_age = 99;
                }

                for (const ValueRangeCell<2> &cell: index->gnn_cells) {
                    if (cell.Test(0, agg.stay.newborn_weight) && cell.Test(1, gestational_age)) {
                        lazy.gnn = cell.value;
                        break;
                    }
                }
            }

            return 0;
        } break;

        case 41: {
            for (DiagnosisCode diag: diagnoses) {
                const DiagnosisInfo *diag_info = index->FindDiagnosis(diag);
                if (!diag_info)
                    continue;

                uint8_t cmd = diag_info->Attributes(agg.stay.sex).cmd;
                uint8_t jump = diag_info->Attributes(agg.stay.sex).jump;
                if (cmd == ghm_node.u.test.params[0] && jump == ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;

        case 42: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            return (agg.stay.newborn_weight && agg.stay.newborn_weight < param);
        } break;

        case 43: {
            for (DiagnosisCode diag: diagnoses) {
                if (diag == linked_diagnosis)
                    continue;

                const DiagnosisInfo *diag_info = index->FindDiagnosis(diag);
                if (!diag_info)
                    continue;

                uint8_t cmd = diag_info->Attributes(agg.stay.sex).cmd;
                uint8_t jump = diag_info->Attributes(agg.stay.sex).jump;
                if (cmd == ghm_node.u.test.params[0] && jump == ghm_node.u.test.params[1])
                    return 1;
            }

            return 0;
        } break;
    }

    LogError("Unknown test %1 or invalid arguments", ghm_node.u.test.function);
    return -1;
}

static int LimitSeverity(int duration, int severity)
{
    if (severity == 3 && duration < 5) {
        severity = 2;
    }
    if (severity == 2 && duration < 4) {
        severity = 1;
    }
    if (severity == 1 && duration < 3) {
        severity = 0;
    }

    return severity;
}

bool Classifier::TestExclusion(const DiagnosisInfo &cma_diag_info,
                               const DiagnosisInfo &main_diag_info)
{
    // TODO: Check boundaries, and take care of DumpDiagnosis too
    const ExclusionInfo *excl = &index->exclusions[cma_diag_info.exclusion_set_idx];
    if (UNLIKELY(!excl))
        return false;
    return (excl->raw[main_diag_info.cma_exclusion_offset] & main_diag_info.cma_exclusion_mask);
}

GhmCode Classifier::Run(HeapArray<int16_t> *out_errors)
{
    GhmCode ghm;

    ghm = RunGhmTree(out_errors);
    ghm = RunGhmSeverity(ghm, out_errors);

    return ghm;
}

GhmCode Classifier::RunGhmTree(HeapArray<int16_t> *out_errors)
{
    GhmCode ghm = {};

    main_diagnosis = agg.stay.main_diagnosis;
    linked_diagnosis = agg.stay.linked_diagnosis;

    size_t ghm_node_idx = 0;
    for (size_t i = 0; !ghm.IsValid(); i++) {
        if (i >= index->ghm_nodes.len) {
            LogError("Empty GHM tree or infinite loop");
            out_errors->Append(4);
            return GhmCode::FromString("90Z03Z");
        }

        const GhmDecisionNode &ghm_node = index->ghm_nodes[ghm_node_idx];
        switch (ghm_node.type) {
            case GhmDecisionNode::Type::Test: {
                int function_ret = ExecuteGhmTest(ghm_node, out_errors);
                if (function_ret < 0 || (size_t)function_ret >= ghm_node.u.test.children_count) {
                    LogError("Result for GHM tree test %1 out of range (%2 - %3)",
                             ghm_node.u.test.function, 0, ghm_node.u.test.children_count);
                    out_errors->Append(4);
                    return GhmCode::FromString("90Z03Z");
                }

                ghm_node_idx = ghm_node.u.test.children_idx + function_ret;
            } break;

            case GhmDecisionNode::Type::Ghm: {
                ghm = ghm_node.u.ghm.code;
                if (ghm_node.u.ghm.error) {
                    out_errors->Append(ghm_node.u.ghm.error);
                }
            } break;
        }
    }

    return ghm;
}

GhmCode Classifier::RunGhmSeverity(GhmCode ghm, HeapArray<int16_t> *out_errors)
{
    const GhmRootInfo *ghm_root_info = index->FindGhmRoot(ghm.Root());
    if (!ghm_root_info) {
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
    }

    if (ghm.parts.mode >= 'A' && ghm.parts.mode <= 'D') {
        int severity = ghm.parts.mode - 'A';

        if (ghm_root_info->childbirth_severity_list) {
            // TODO: Check boundaries
            for (const ValueRangeCell<2> &cell: index->cma_cells[ghm_root_info->childbirth_severity_list - 1]) {
                if (cell.Test(0, agg.stay.gestational_age) && cell.Test(1, severity)) {
                    severity = cell.value;
                    break;
                }
            }
        }

        ghm.parts.mode = 'A' + LimitSeverity(agg.duration, severity);
    } else if (!ghm.parts.mode) {
        int severity = 0;

        const DiagnosisInfo *main_diag_info = index->FindDiagnosis(main_diagnosis);
        const DiagnosisInfo *linked_diag_info = index->FindDiagnosis(linked_diagnosis);
        for (const DiagnosisCode &diag: diagnoses) {
            if (diag == main_diagnosis || diag == linked_diagnosis)
                continue;

            const DiagnosisInfo *diag_info = index->FindDiagnosis(diag);
            if (!diag_info)
                continue;

            // TODO: Check boundaries (ghm_root CMA exclusion offset, etc.)
            int new_severity = diag_info->Attributes(agg.stay.sex).severity;
            if (new_severity > severity &&
                    !(agg.age < 14 && diag_info->Attributes(agg.stay.sex).raw[19] & 0x10) &&
                    !(agg.age >= 2 && diag_info->Attributes(agg.stay.sex).raw[19] & 0x8) &&
                    !(agg.age >= 2 && diag.str[0] == 'P') &&
                    !(diag_info->Attributes(agg.stay.sex).raw[ghm_root_info->cma_exclusion_offset] &
                      ghm_root_info->cma_exclusion_mask) &&
                    !TestExclusion(*diag_info, *main_diag_info) &&
                    (!linked_diag_info || !TestExclusion(*diag_info, *linked_diag_info))) {
                severity = new_severity;
            }
        }

        if (agg.age >= ghm_root_info->old_age_treshold && severity < ghm_root_info->old_severity_limit) {
            severity++;
        } else if (agg.age < ghm_root_info->young_age_treshold && severity < ghm_root_info->young_severity_limit) {
            severity++;
        } else if (agg.stay.exit.mode == 9 && !severity) {
            severity = 1;
        }

        ghm.parts.mode = '1' + LimitSeverity(agg.duration, severity);
    }

    return ghm;
}

GhmCode Classifier::AddError(HeapArray<int16_t> *out_errors, int16_t error)
{
    out_errors->Append(error);
    return GhmCode::FromString("90Z00Z");
}

static bool AreStaysCompatible(const Stay stay1, const Stay &stay2)
{
    return stay2.stay_id == stay1.stay_id &&
           !stay2.session_count &&
           (stay2.entry.mode == 6 || stay2.entry.mode == 0);
}

ArrayRef<const Stay> ClusterStays(ArrayRef<const Stay> stays, ClusterMode mode,
                                  ArrayRef<const Stay> *out_remainder)
{
    if (!stays.len)
        return {};

    size_t agg_len = 1;
    switch (mode) {
        case ClusterMode::StayModes: {
            if (!stays[0].session_count) {
                while (agg_len < stays.len &&
                       AreStaysCompatible(stays[agg_len - 1], stays[agg_len])) {
                    agg_len++;
                }
            }
        } break;

        case ClusterMode::BillId: {
            while (agg_len < stays.len &&
                   stays[agg_len - 1].bill_id == stays[agg_len].bill_id) {
                agg_len++;
            }
        } break;

        case ClusterMode::Disable: {} break;
    }

    if (out_remainder) {
        *out_remainder = stays.Take(agg_len, stays.len - agg_len);
    }
    return stays.Take(0, agg_len);
}

// FIXME: Check Stay invariants before classification (all diag and proc exist, etc.)
GhmCode Classifier::Init(const ClassifierSet &classifier_set, ArrayRef<const Stay> stays,
                         HeapArray<int16_t> *out_errors)
{
    Assert(stays.len > 0);

    this->stays = stays;

    index = classifier_set.FindIndex(stays[stays.len - 1].dates[1]);
    if (!index) {
        LogError("No classifier table available on '%1'", stays[stays.len - 1].dates[1]);
        out_errors->Append(502);
        return GhmCode::FromString("90Z03Z");
    }

    agg.stay = stays[0];
    agg.age = ComputeAge(agg.stay.dates[0], agg.stay.birthdate);
    agg.duration = 0;
    for (const Stay &stay: stays) {
        if (stay.gestational_age > 0) {
            // TODO: Must be first (newborn) or on RUM with a$41.2 only
            agg.stay.gestational_age = stay.gestational_age;
        }
        if (stay.igs2 > agg.stay.igs2) {
            agg.stay.igs2 = stay.igs2;
        }
        agg.duration += stay.dates[1] - stay.dates[0];
    }
    agg.stay.dates[1] = stays[stays.len - 1].dates[1];
    agg.stay.exit = stays[stays.len - 1].exit;

    // Deduplicate diagnoses
    {
        diagnoses.Clear(256);
        for (const Stay &stay: stays) {
            diagnoses.Append(stay.diagnoses);
        }

        std::sort(diagnoses.begin(), diagnoses.end(),
                  [](DiagnosisCode code1, DiagnosisCode code2) {
            return code1.value < code2.value;
        });

        diagnoses.RemoveFrom(std::unique(diagnoses.begin(), diagnoses.end()) - diagnoses.ptr);
    }
    agg.stay.diagnoses = diagnoses;

    // Deduplicate procedures
    {
        procedures.Clear(512);
        for (const Stay &stay: stays) {
            procedures.Append(stay.procedures);
        }

        std::sort(procedures.begin(), procedures.end(),
                  [](const Procedure &proc1, const Procedure &proc2) {
            if (proc1.code.value < proc2.code.value) {
                return true;
            } else {
                return proc1.phase < proc2.phase;
            }
        });

        // TODO: Warn when we deduplicate procedures with different attributes,
        // such as when the two procedures fall into different date ranges / limits.
        size_t k = 0;
        for (size_t j = 1; j < procedures.len; j++) {
            if (procedures[j].code == procedures[k].code &&
                    procedures[j].phase == procedures[k].phase) {
                procedures[k].activities |= procedures[j].activities;
                procedures[k].count += procedures[j].count;
                if (UNLIKELY(procedures[k].count > 9999)) {
                    procedures[k].count = 9999;
                }
            } else {
                procedures[++k] = procedures[j];
            }
        }
        procedures.RemoveFrom(k + 1);
    }
    agg.stay.procedures = procedures;

    if (stays.len > 1) {
        const Stay *main_stay = FindMainStay();

        agg.stay.main_diagnosis = main_stay->main_diagnosis;
        agg.stay.linked_diagnosis = main_stay->linked_diagnosis;
    }

    lazy = {};

    return {};
}

GhmCode ClassifyCluster(const ClassifierSet &classifier_set, ArrayRef<const Stay> stays,
                        HeapArray<int16_t> *out_errors)
{
    GhmCode ghm;
    Classifier classifier;

    ghm = classifier.Init(classifier_set, stays, out_errors);
    if (!ghm.IsValid()) {
        ghm = classifier.Run(out_errors);
    }

    return ghm;
}

void Classify(const ClassifierSet &classifier_set, ArrayRef<const Stay> stays,
              ClusterMode cluster_mode, ClassifyResultSet *out_result_set)
{
    // Reuse Classifier instance to reduce heap allocations (diagnoses and procedures)
    Classifier classifier;

    while (stays.len) {
        ArrayRef<const Stay> cluster_stays = ClusterStays(stays, cluster_mode, &stays);

        ClassifyResult result;
        {
            result.errors.ptr = (int16_t *)out_result_set->store.errors.len;
            result.ghm = classifier.Init(classifier_set, cluster_stays,
                                         &out_result_set->store.errors);
            if (!result.ghm.IsValid()) {
                result.ghm = classifier.Run(&out_result_set->store.errors);
            }
            result.errors.len = out_result_set->store.errors.len - (size_t)result.errors.ptr;
        }

        out_result_set->results.Append(result);
    }

    for (ClassifyResult &result: out_result_set->results) {
        result.errors.ptr = out_result_set->store.errors.ptr + (size_t)result.errors.ptr;
    }
}
