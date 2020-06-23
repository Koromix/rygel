// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "thop.hh"
#include "config.hh"
#include "mco_info.hh"
#include "mco.hh"

namespace RG {

static const mco_TableIndex *GetIndexFromRequest(const http_RequestInfo &request, http_IO *io,
                                                 drd_Sector *out_sector = nullptr)
{
    Date date = {};
    {
        const char *date_str = request.GetQueryValue("date");
        if (date_str) {
            date = Date::FromString(date_str);
        } else {
            LogError("Missing 'date' parameter");
        }
        if (!date.value) {
            io->AttachError(422);
            return nullptr;
        }
    }

    drd_Sector sector = drd_Sector::Public;
    if (out_sector) {
        const char *sector_str = request.GetQueryValue("sector");
        if (!sector_str) {
            LogError("Missing 'sector' parameter");
            io->AttachError(422);
            return nullptr;
        } else if (TestStr(sector_str, "public")) {
            sector = drd_Sector::Public;
        } else if (TestStr(sector_str, "private")) {
            sector = drd_Sector::Private;
        } else {
            LogError("Invalid 'sector' parameter");
            io->AttachError(422);
            return nullptr;
        }
    }

    const mco_TableIndex *index = mco_table_set.FindIndex(date);
    if (!index || index->limit_dates[0] != date) {
        LogError("No table index for date '%1'", date);
        io->AttachError(404);
        return nullptr;
    }

    if (out_sector) {
        *out_sector = sector;
    }
    return index;
}

void ProduceMcoDiagnoses(const http_RequestInfo &request, const User *, http_IO *io)
{
    const mco_TableIndex *index = GetIndexFromRequest(request, io);
    if (!index)
        return;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Diagnoses);
    {
        const char *spec_str = request.GetQueryValue("spec");
        if (spec_str && spec_str[0]) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Diagnoses) {
                LogError("Invalid diagnosis list specifier '%1'", spec_str);
                io->AttachError(422);
                return;
            }
        }
    }

    http_JsonPageBuilder json(request.compression_type);
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
    return json.Finish(io);
}

void ProduceMcoProcedures(const http_RequestInfo &request, const User *, http_IO *io)
{
    const mco_TableIndex *index = GetIndexFromRequest(request, io);
    if (!index)
        return;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Procedures);
    {
        const char *spec_str = request.GetQueryValue("spec");
        if (spec_str && spec_str[0]) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Procedures) {
                LogError("Invalid procedure list specifier '%1'", spec_str);
                io->AttachError(422);
                return;
            }
        }
    }

    http_JsonPageBuilder json(request.compression_type);
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
            json.EndObject();
        }
    }
    json.EndArray();

    io->AddCachingHeaders(thop_config.max_age, thop_etag);
    return json.Finish(io);
}

void ProduceMcoGhmGhs(const http_RequestInfo &request, const User *, http_IO *io)
{
    drd_Sector sector;
    const mco_TableIndex *index = GetIndexFromRequest(request, io, &sector);
    if (!index)
        return;

    const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
        *mco_cache_set.index_to_constraints.FindValue(index, nullptr);

    http_JsonPageBuilder json(request.compression_type);
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
                json.Key("young_age_treshold"); json.Int(ghm_root_info.young_age_treshold);
                json.Key("young_severity_limit"); json.Int(ghm_root_info.young_severity_limit);
            }
            if (ghm_root_info.old_severity_limit) {
                json.Key("old_age_treshold"); json.Int(ghm_root_info.old_age_treshold);
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
            if (ghm_root_info.confirm_duration_treshold) {
                json.Key("confirm_treshold"); json.Int(ghm_root_info.confirm_duration_treshold);
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
            switch (ghm_to_ghs_info.special_mode) {
                case mco_GhmToGhsInfo::SpecialMode::None: {} break;
                case mco_GhmToGhsInfo::SpecialMode::Diabetes2: { json.Key("special_mode"); json.String("diabetes2"); } break;
                case mco_GhmToGhsInfo::SpecialMode::Diabetes3: { json.Key("special_mode"); json.String("diabetes3"); } break;
                case mco_GhmToGhsInfo::SpecialMode::Outpatient: { json.Key("special_mode"); json.String("outpatient"); } break;
                case mco_GhmToGhsInfo::SpecialMode::Intermediary: { json.Key("special_mode"); json.String("intermediary"); } break;
            }
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
                if (ghs_price_info->exh_treshold) {
                    json.Key("exh_treshold"); json.Int(ghs_price_info->exh_treshold);
                    json.Key("exh_cents"); json.Int(ghs_price_info->exh_cents);
                }
                if (ghs_price_info->exb_treshold) {
                    json.Key("exb_treshold"); json.Int(ghs_price_info->exb_treshold);
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
    return json.Finish(io);
}

void ProduceMcoTree(const http_RequestInfo &request, const User *, http_IO *io)
{
    const mco_TableIndex *index = GetIndexFromRequest(request, io);
    if (!index)
        return;

    const HeapArray<mco_ReadableGhmNode> *readable_nodes;
    readable_nodes = mco_cache_set.readable_nodes.Find(index);

    http_JsonPageBuilder json(request.compression_type);

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
    json.Finish(io);
}

}
