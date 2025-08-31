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

#include "src/core/base/base.hh"
#include "thop.hh"
#include "config.hh"
#include "mco_info.hh"
#include "mco.hh"

namespace K {

static const mco_TableIndex *GetIndexFromRequest(http_IO *io, drd_Sector *out_sector = nullptr)
{
    const http_RequestInfo &request = io->Request();

    LocalDate date = {};
    {
        const char *date_str = request.GetQueryValue("date");

        if (!date_str) {
            LogError("Missing 'date' parameter");
            io->SendError(422);
            return nullptr;
        }
        if (!ParseDate(date_str, &date)) {
            io->SendError(422);
            return nullptr;
        }
    }

    drd_Sector sector = drd_Sector::Public;
    if (out_sector) {
        const char *sector_str = request.GetQueryValue("sector");
        if (!sector_str) {
            LogError("Missing 'sector' parameter");
            io->SendError(422);
            return nullptr;
        } else if (TestStr(sector_str, "public")) {
            sector = drd_Sector::Public;
        } else if (TestStr(sector_str, "private")) {
            sector = drd_Sector::Private;
        } else {
            LogError("Invalid 'sector' parameter");
            io->SendError(422);
            return nullptr;
        }
    }

    const mco_TableIndex *index = mco_table_set.FindIndex(date);
    if (!index || index->limit_dates[0] != date) {
        LogError("No table index for date '%1'", date);
        io->SendError(404);
        return nullptr;
    }

    if (out_sector) {
        *out_sector = sector;
    }
    return index;
}

void ProduceMcoDiagnoses(http_IO *io, const User *)
{
    const http_RequestInfo &request = io->Request();

    const mco_TableIndex *index = GetIndexFromRequest(io);
    if (!index)
        return;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Diagnoses);
    {
        const char *spec_str = request.GetQueryValue("spec");

        if (spec_str && spec_str[0]) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Diagnoses) {
                LogError("Invalid diagnosis list specifier '%1'", spec_str);
                io->SendError(422);
                return;
            }
        }
    }

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;
    char buf[512];

    json.StartArray();
    for (const mco_DiagnosisInfo &diag_info: index->diagnoses) {
        if (spec.Match(diag_info.raw)) {
            json.StartObject();
            json.Key("diag"); json.String(diag_info.diag.str);
            switch (diag_info.sexes) {
                case 0x1: { json.Key("sex"); json.String("Homme"); } break;
                case 0x2: { json.Key("sex"); json.String("Femme"); } break;
                case 0x3: {} break;
            }
            if (diag_info.cmd) {
                json.Key("cmd");
                json.String(Fmt(buf, "D-%1", FmtArg(diag_info.cmd).Pad0(-2)).ptr);
            }
            if (diag_info.cmd && diag_info.jump) {
                json.Key("main_list");
                json.String(Fmt(buf, "D-%1%2", FmtArg(diag_info.cmd).Pad0(-2),
                                               FmtArg(diag_info.jump).Pad0(-2)).ptr);
            }
            if (diag_info.severity) {
                json.Key("severity"); json.Int(diag_info.severity);
            }
            json.EndObject();
        }
    }
    json.EndArray();

    io->AddCachingHeaders(thop_config.max_age, thop_etag);
    return json.Finish();
}

void ProduceMcoProcedures(http_IO *io, const User *)
{
    const http_RequestInfo &request = io->Request();

    const mco_TableIndex *index = GetIndexFromRequest(io);
    if (!index)
        return;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Procedures);
    {
        const char *spec_str = request.GetQueryValue("spec");
        if (spec_str && spec_str[0]) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Procedures) {
                LogError("Invalid procedure list specifier '%1'", spec_str);
                io->SendError(422);
                return;
            }
        }
    }

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;
    char buf[512];

    json.StartArray();
    for (const mco_ProcedureInfo &proc_info: index->procedures) {
        if (spec.Match(proc_info.bytes)) {
            json.StartObject();
            json.Key("proc"); json.String(proc_info.proc.str);
            json.Key("begin_date"); json.String(Fmt(buf, "%1", proc_info.limit_dates[0]).ptr);
            if (proc_info.limit_dates[1] < mco_MaxDate1980) {
                json.Key("end_date"); json.String(Fmt(buf, "%1", proc_info.limit_dates[1]).ptr);
            }
            json.Key("phase"); json.Int(proc_info.phase);
            json.Key("activities"); json.String(proc_info.ActivitiesToStr(buf).ptr);
            if (proc_info.extensions > 1) {
                json.Key("extensions"); json.String(proc_info.ExtensionsToStr(buf).ptr);
            }
            if (proc_info.Test(0, 0x80) || proc_info.Test(23, 0x80)) {
                json.Key("classifying"); json.Bool(true);
            }
            json.EndObject();
        }
    }
    json.EndArray();

    io->AddCachingHeaders(thop_config.max_age, thop_etag);
    return json.Finish();
}

void ProduceMcoGhmGhs(http_IO *io, const User *)
{
    drd_Sector sector;
    const mco_TableIndex *index = GetIndexFromRequest(io, &sector);
    if (!index)
        return;

    const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
        *mco_cache_set.index_to_constraints.FindValue(index, nullptr);

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;
    char buf[512];

    json.StartArray();
    for (const mco_GhmRootInfo &ghm_root_info: index->ghm_roots) {
        Span<const mco_GhmToGhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
        for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
            mco_GhsCode ghs = ghm_to_ghs_info.Ghs(sector);

            const mco_GhsPriceInfo *ghs_price_info = index->FindGhsPrice(ghs, sector);
            const mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);

            uint32_t combined_durations = 0;
            if (constraint) {
                combined_durations = constraint->durations &
                                     ~((1u << ghm_to_ghs_info.minimum_duration) - 1);
            }

            json.StartObject();

            json.Key("ghm"); json.String(ghm_to_ghs_info.ghm.ToString(buf).ptr);
            json.Key("ghm_root"); json.String(ghm_to_ghs_info.ghm.Root().ToString(buf).ptr);
            if (ghm_root_info.young_severity_limit) {
                json.Key("young_age_threshold"); json.Int(ghm_root_info.young_age_threshold);
                json.Key("young_severity_limit"); json.Int(ghm_root_info.young_severity_limit);
            }
            if (ghm_root_info.old_severity_limit) {
                json.Key("old_age_threshold"); json.Int(ghm_root_info.old_age_threshold);
                json.Key("old_severity_limit"); json.Int(ghm_root_info.old_severity_limit);
            }
            json.Key("durations"); json.Uint(combined_durations);
            if (constraint) {
                if (constraint->raac_durations) {
                    json.Key("raac_durations"); json.Uint(constraint->raac_durations);
                }
                if ((combined_durations & 0x1) &&
                        (constraint->warnings & (int)mco_GhmConstraint::Warning::PreferCmd28)) {
                    json.Key("warn_cmd28"); json.Bool(true);
                }
            }
            if (ghm_root_info.confirm_duration_threshold) {
                json.Key("confirm_threshold"); json.Int(ghm_root_info.confirm_duration_threshold);
            }

            json.Key("ghs"); json.Int(ghs.number);
            if (ghm_to_ghs_info.unit_authorization) {
                json.Key("unit_authorization"); json.Int(ghm_to_ghs_info.unit_authorization);
            }
            if (ghm_to_ghs_info.bed_authorization) {
                json.Key("bed_authorization"); json.Int(ghm_to_ghs_info.bed_authorization);
            }
            if (ghm_to_ghs_info.minimum_duration) {
                json.Key("minimum_duration"); json.Int(ghm_to_ghs_info.minimum_duration);
            }
            if (ghm_to_ghs_info.minimum_age) {
                json.Key("minimum_age"); json.Int(ghm_to_ghs_info.minimum_age);
            }
            json.Key("modes"); json.StartArray();
            switch (ghm_to_ghs_info.special_mode) {
                case mco_GhmToGhsInfo::SpecialMode::None: {} break;
                case mco_GhmToGhsInfo::SpecialMode::Diabetes2: { json.String("diabetes2"); } break;
                case mco_GhmToGhsInfo::SpecialMode::Diabetes3: { json.String("diabetes3"); } break;
                case mco_GhmToGhsInfo::SpecialMode::Outpatient: { json.String("outpatient"); } break;
                case mco_GhmToGhsInfo::SpecialMode::Intermediary: { json.String("intermediary"); } break;
            }
            json.EndArray();
            if (ghm_to_ghs_info.main_diagnosis_mask.value) {
                json.Key("main_diagnosis");
                json.String(Fmt(buf, "D$%1.%2",
                                  ghm_to_ghs_info.main_diagnosis_mask.offset,
                                  ghm_to_ghs_info.main_diagnosis_mask.value).ptr);
            }
            if (ghm_to_ghs_info.diagnosis_mask.value) {
                json.Key("diagnoses");
                json.String(Fmt(buf, "D$%1.%2",
                                  ghm_to_ghs_info.diagnosis_mask.offset,
                                  ghm_to_ghs_info.diagnosis_mask.value).ptr);
            }
            if (ghm_to_ghs_info.procedure_masks.len) {
                json.Key("procedures"); json.StartArray();
                for (const drd_ListMask &mask: ghm_to_ghs_info.procedure_masks) {
                    json.String(Fmt(buf, "A$%1.%2", mask.offset, mask.value).ptr);
                }
                json.EndArray();
            }

            if (ghs_price_info) {
                if (ghs_price_info->flags & (int)mco_GhsPriceInfo::Flag::Minoration) {
                    json.Key("warn_ucd"); json.Bool(true);
                }
                json.Key("ghs_cents"); json.Int(ghs_price_info->ghs_cents);
                json.Key("ghs_coefficient"); json.Double(index->GhsCoefficient(sector));
                if (ghs_price_info->exh_threshold) {
                    json.Key("exh_threshold"); json.Int(ghs_price_info->exh_threshold);
                    json.Key("exh_cents"); json.Int(ghs_price_info->exh_cents);
                }
                if (ghs_price_info->exb_threshold) {
                    json.Key("exb_threshold"); json.Int(ghs_price_info->exb_threshold);
                    json.Key("exb_cents"); json.Int(ghs_price_info->exb_cents);
                    if (ghs_price_info->flags & (int)mco_GhsPriceInfo::Flag::ExbOnce) {
                        json.Key("exb_once"); json.Bool(true);
                    }
                }
            }

            json.EndObject();
        }
    }
    json.EndArray();

    io->AddCachingHeaders(thop_config.max_age, thop_etag);
    return json.Finish();
}

void ProduceMcoTree(http_IO *io, const User *)
{
    const mco_TableIndex *index = GetIndexFromRequest(io);
    if (!index)
        return;

    const HeapArray<mco_ReadableGhmNode> *readable_nodes;
    readable_nodes = mco_cache_set.readable_nodes.Find(index);

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    for (const mco_ReadableGhmNode &readable_node: *readable_nodes) {
        json.StartObject();
        if (readable_node.header) {
            json.Key("header"); json.String(readable_node.header);
        }
        json.Key("key"); json.String(readable_node.key);
        json.Key("type"); json.String(readable_node.type);
        json.Key("text"); json.String(readable_node.text);
        if (readable_node.reverse) {
            json.Key("reverse"); json.String(readable_node.reverse);
        }
        if (readable_node.children_idx) {
            json.Key("test"); json.Int(readable_node.function);
            json.Key("children_idx"); json.Int64(readable_node.children_idx);
            json.Key("children_count"); json.Int64(readable_node.children_count);
        }
        json.EndObject();
    }
    json.EndArray();

    io->AddCachingHeaders(thop_config.max_age, thop_etag);
    json.Finish();
}

struct HighlightContext {
    Span<const mco_GhmDecisionNode> ghm_nodes;

    bool ignore_diagnoses;
    bool ignore_procedures;
    HeapArray<const mco_DiagnosisInfo *> diagnoses;
    HeapArray<const mco_ProcedureInfo *> procedures;
    uint8_t proc_activities;

    bool ignore_medical;
};

// Keep in sync with code in mco_info.js (renderTree function)
enum class HighlightFlag {
    Session = 1 << 0,
    NoSession = 1 << 1,
    A7D = 1 << 2,
    NoA7D = 1 << 3
};

static bool HighlightNodes(const HighlightContext &ctx, Size node_idx, uint16_t flags,
                           HashMap<int16_t, uint16_t> *out_nodes);
static void HighlightChildren(const HighlightContext &ctx, const mco_GhmDecisionNode &ghm_node,
                              uint16_t flags, HashMap<int16_t, uint16_t> *out_nodes)
{
    for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
        HighlightNodes(ctx, ghm_node.u.test.children_idx + i, flags, out_nodes);
    }
}

static bool HighlightNodes(const HighlightContext &ctx, Size node_idx, uint16_t flags,
                           HashMap<int16_t, uint16_t> *out_nodes)
{
    for (Size i = 0;; i++) {
        K_ASSERT(i < ctx.ghm_nodes.len); // Infinite loops
        K_ASSERT(node_idx < ctx.ghm_nodes.len);

        const mco_GhmDecisionNode &ghm_node = ctx.ghm_nodes[node_idx];
        bool stop = false;

        switch (ghm_node.function) {
            case 0:
            case 1: {
                if (!ctx.ignore_diagnoses) {
                    for (const mco_DiagnosisInfo *diag_info: ctx.diagnoses) {
                        uint8_t diag_byte = diag_info->GetByte(ghm_node.u.test.params[0]);
                        stop |= diag_byte &&
                                HighlightNodes(ctx, ghm_node.u.test.children_idx + diag_byte, flags, out_nodes);
                    }
                } else {
                    HighlightChildren(ctx, ghm_node, flags, out_nodes);
                }
            } break;

            case 2:
            case 9:
            case 10: {
                if (!ctx.ignore_procedures) {
                    for (const mco_ProcedureInfo *proc_info: ctx.procedures) {
                        stop |= proc_info->Test(ghm_node.u.test.params[0], ghm_node.u.test.params[1]) &&
                                HighlightNodes(ctx, ghm_node.u.test.children_idx + 1, flags, out_nodes);
                    }
                } else {
                    HighlightChildren(ctx, ghm_node, flags, out_nodes);
                }
            } break;

            case 3: {
                if (ghm_node.u.test.params[1] == 1 && ghm_node.u.test.params[0] == 7) {
                    HighlightChildren(ctx, ghm_node, flags & ~(int)HighlightFlag::NoA7D, out_nodes);
                    flags &= ~(int)HighlightFlag::A7D;
                } else {
                    HighlightChildren(ctx, ghm_node, flags, out_nodes);
                }
            } break;
            case 19: {
                // This is ugly, but needed for A7D to work correctly. Otherwise there
                // are entry mode nodes that lead back to A7D nodes but with NoA7D :/
                if (ghm_node.u.test.params[1] != 2) {
                    HighlightChildren(ctx, ghm_node, flags, out_nodes);
                }
            } break;

            case 5:
            case 6:
            case 7:
            case 18:
            case 26:
            case 36: {
                if (!ctx.ignore_diagnoses) {
                    for (const mco_DiagnosisInfo *diag_info: ctx.diagnoses) {
                        stop |= diag_info->Test(ghm_node.u.test.params[0], ghm_node.u.test.params[1]) &&
                                HighlightNodes(ctx, ghm_node.u.test.children_idx + 1, flags, out_nodes);
                    }
                } else {
                    HighlightChildren(ctx, ghm_node, flags, out_nodes);
                }
            } break;

            case 12: { // GHM, at least!
                if (!ctx.ignore_medical || ghm_node.u.ghm.ghm.parts.type == 'C' ||
                                           ghm_node.u.ghm.ghm.parts.type == 'K' ||
                                           (ghm_node.u.ghm.ghm.Root() == mco_GhmRootCode(90, 'Z', 1) &&
                                            ghm_node.u.ghm.error == 6)) {
                    uint16_t *ptr = out_nodes->TrySet((int16_t)node_idx, 0);
                    *ptr |= flags;

                    return true;
                } else {
                    return false;
                }
            } break;

            case 13: {
                if (!ctx.ignore_diagnoses) {
                    for (const mco_DiagnosisInfo *diag_info: ctx.diagnoses) {
                        uint8_t diag_byte = diag_info->GetByte(ghm_node.u.test.params[0]);
                        stop |= (diag_byte == ghm_node.u.test.params[1]) &&
                                HighlightNodes(ctx, ghm_node.u.test.children_idx + 1, flags, out_nodes);
                    }
                } else {
                    HighlightChildren(ctx, ghm_node, flags, out_nodes);
                }
            } break;

            case 20: { // GOTO
                HighlightNodes(ctx, ghm_node.u.test.children_idx, flags, out_nodes);
                return false;
            } break;

            case 28: {
                // The point of this is to highlight non-blocking error nodes,
                // such as errors 80 and 222.
                if (HighlightNodes(ctx, ghm_node.u.test.children_idx, flags, out_nodes)) {
                    uint16_t *ptr = out_nodes->TrySet((int16_t)node_idx, 0);
                    *ptr |= flags;

                    return true;
                } else {
                    return false;
                }
            } break;

            case 30: {
                uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);

                if (param == 0) {
                    HighlightChildren(ctx, ghm_node, flags & ~(int)HighlightFlag::NoSession, out_nodes);
                    flags &= ~(int)HighlightFlag::Session;
                } else {
                    HighlightChildren(ctx, ghm_node, flags, out_nodes);
                }
            } break;

            case 33: {
                if (!ctx.ignore_procedures) {
                    stop |= (ctx.proc_activities & (1 << ghm_node.u.test.params[0])) &&
                            HighlightNodes(ctx, ghm_node.u.test.children_idx + 1, flags, out_nodes);
                } else {
                    HighlightChildren(ctx, ghm_node, flags, out_nodes);
                }
            } break;

            case 41:
            case 43: {
                if (!ctx.ignore_diagnoses) {
                    for (const mco_DiagnosisInfo *diag_info: ctx.diagnoses) {
                        stop |= (diag_info->cmd == ghm_node.u.test.params[0]) &&
                                (diag_info->jump == ghm_node.u.test.params[1]) &&
                                HighlightNodes(ctx, ghm_node.u.test.children_idx + 1, flags, out_nodes);
                    }
                } else {
                    HighlightChildren(ctx, ghm_node, flags, out_nodes);
                }
            } break;

            default: {
                HighlightChildren(ctx, ghm_node, flags, out_nodes);
            } break;
        }

        if (stop)
            return true;
        node_idx = ghm_node.u.test.children_idx;
    }
}

void ProduceMcoHighlight(http_IO *io, const User *)
{
    const http_RequestInfo &request = io->Request();

    const mco_TableIndex *index = GetIndexFromRequest(io);
    if (!index)
        return;

    HighlightContext ctx = {};
    ctx.ghm_nodes = index->ghm_nodes;

    // Diagnosis?
    if (const char *code = request.GetQueryValue("diag"); code && code[0]) {
        if (TestStr(code, "*")) {
            ctx.ignore_diagnoses = true;
        } else {
            drd_DiagnosisCode diag =
                drd_DiagnosisCode::Parse(code, K_DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log);
            if (!diag.IsValid()) {
                LogError("Invalid CIM-10 code '%1'", code);
                io->SendError(422);
                return;
            }

            for (const mco_DiagnosisInfo &diag_info: index->FindDiagnosis(diag)) {
                ctx.diagnoses.Append(&diag_info);
            }
            if (!ctx.diagnoses.len) {
                LogError("Unknown diagnosis '%1'", code);
                io->SendError(404);
                return;
            }
        }
    }

    // Procedure?
    if (const char *code = request.GetQueryValue("proc"); code && code[0]) {
        if (TestStr(code, "*")) {
            ctx.ignore_procedures = true;
        } else {
            drd_ProcedureCode proc =
                drd_ProcedureCode::Parse(code, K_DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log);
            if (!proc.IsValid()) {
                LogError("Invalid CCAM code '%1'", code);
                io->SendError(422);
                return;
            }

            for (const mco_ProcedureInfo &proc_info: index->FindProcedure(proc)) {
                ctx.procedures.Append(&proc_info);
                ctx.proc_activities |= proc_info.activities;
            }
            if (!ctx.procedures.len) {
                LogError("Unknown procedure '%1'", code);
                io->SendError(404);
                return;
            }
        }
    }

    // If the user only specifies a major procedure, but no diagnosis, the algorithm
    // fails. But typically, the user wants to see potential GHMs. We can find them with
    // by using a wildcard for diagnosis and by refusing non-C/non-K GHMs.
    if (ctx.procedures.len && !ctx.diagnoses.len && !ctx.ignore_diagnoses) {
        bool invasive = std::any_of(ctx.procedures.begin(), ctx.procedures.end(),
                                    [](const mco_ProcedureInfo *proc_info) { return proc_info->Test(0, 0x80) ||
                                                                                    proc_info->Test(23, 0x80); });

        if (invasive) {
            ctx.ignore_diagnoses = true;
            ctx.ignore_medical = true;
        }
    }

    // Run highlighter
    HashMap<int16_t, uint16_t> matches;
    HighlightNodes(ctx, 0, 0xF, &matches);

    // Output matches!
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();
    for (const auto &it: matches.table) {
        char buf[16];
        json.Key(Fmt(buf, "%1", it.key).ptr); json.Int64(it.value);
    }
    json.EndObject();

    io->AddCachingHeaders(thop_config.max_age, thop_etag);
    json.Finish();
}

}
