// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "drdw.hh"

Response ProduceIndexes(const ConnectionInfo *, const char *, CompressionType compression_type)
{
    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        writer.StartArray();
        for (const mco_TableIndex &index: drdw_table_set->indexes) {
            if (!index.valid)
                continue;

            char buf[32];

            writer.StartObject();
            writer.Key("begin_date"); writer.String(Fmt(buf, "%1", index.limit_dates[0]).ptr);
            writer.Key("end_date"); writer.String(Fmt(buf, "%1", index.limit_dates[1]).ptr);
            if (index.changed_tables & ~MaskEnum(mco_TableType::PriceTablePublic)) {
                writer.Key("changed_tables"); writer.Bool(true);
            }
            if (index.changed_tables & MaskEnum(mco_TableType::PriceTablePublic)) {
                writer.Key("changed_prices"); writer.Bool(true);
            }
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    });

    return {200, response};
}

Response ProduceDiagnoses(const ConnectionInfo *conn, const char *, CompressionType compression_type)
{
    Response response = {};
    const mco_TableIndex *index = GetIndexFromQueryString(conn, "diagnoses.json", &response);
    if (!index)
        return response;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Diagnoses);
    {
        const char *spec_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "spec");
        if (spec_str) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Diagnoses) {
                LogError("Invalid diagnosis list specifier '%1'", spec_str);
                return CreateErrorPage(422);
            }
        }
    }

    response.code = 200;
    response.response = BuildJson(compression_type,
                                  [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[512];

        const auto WriteSexSpecificInfo = [&](const mco_DiagnosisInfo &diag_info,
                                              int sex) {
            if (diag_info.Attributes(sex).cmd) {
                writer.Key("cmd");
                writer.String(Fmt(buf, "D-%1",
                                  FmtArg(diag_info.Attributes(sex).cmd).Pad0(-2)).ptr);
            }
            if (diag_info.Attributes(sex).cmd && diag_info.Attributes(sex).jump) {
                writer.Key("main_list");
                writer.String(Fmt(buf, "D-%1%2",
                                  FmtArg(diag_info.Attributes(sex).cmd).Pad0(-2),
                                  FmtArg(diag_info.Attributes(sex).jump).Pad0(-2)).ptr);
            }
            if (diag_info.Attributes(sex).severity) {
                writer.Key("severity"); writer.Int(diag_info.Attributes(sex).severity);
            }
        };

        writer.StartArray();
        for (const mco_DiagnosisInfo &diag_info: index->diagnoses) {
            if (diag_info.flags & (int)mco_DiagnosisInfo::Flag::SexDifference) {
                if (spec.Match(diag_info.Attributes(1).raw)) {
                    writer.StartObject();
                    writer.Key("diag"); writer.String(diag_info.diag.str);
                    writer.Key("sex"); writer.String("Homme");
                    WriteSexSpecificInfo(diag_info, 1);
                    writer.EndObject();
                }

                if (spec.Match(diag_info.Attributes(2).raw)) {
                    writer.StartObject();
                    writer.Key("diag"); writer.String(diag_info.diag.str);
                    writer.Key("sex"); writer.String("Femme");
                    WriteSexSpecificInfo(diag_info, 2);
                    writer.EndObject();
                }
            } else if (spec.Match(diag_info.Attributes(1).raw)) {
                writer.StartObject();
                writer.Key("diag"); writer.String(diag_info.diag.str);
                WriteSexSpecificInfo(diag_info, 1);
                writer.EndObject();
            }
        }
        writer.EndArray();
        return true;
    });

    return response;
}

Response ProduceProcedures(const ConnectionInfo *conn, const char *, CompressionType compression_type)
{
    Response response = {};
    const mco_TableIndex *index = GetIndexFromQueryString(conn, "procedures.json", &response);
    if (!index)
        return response;

    mco_ListSpecifier spec(mco_ListSpecifier::Table::Procedures);
    {
        const char *spec_str = MHD_lookup_connection_value(conn->conn, MHD_GET_ARGUMENT_KIND, "spec");
        if (spec_str) {
            spec = mco_ListSpecifier::FromString(spec_str);
            if (!spec.IsValid() || spec.table != mco_ListSpecifier::Table::Procedures) {
                LogError("Invalid procedure list specifier '%1'", spec_str);
                return CreateErrorPage(422);
            }
        }
    }

    response.code = 200;
    response.response = BuildJson(compression_type,
                                  [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[512];

        writer.StartArray();
        for (const mco_ProcedureInfo &proc_info: index->procedures) {
            if (spec.Match(proc_info.bytes)) {
                writer.StartObject();
                writer.Key("proc"); writer.String(proc_info.proc.str);
                writer.Key("begin_date"); writer.String(Fmt(buf, "%1", proc_info.limit_dates[0]).ptr);
                writer.Key("end_date"); writer.String(Fmt(buf, "%1", proc_info.limit_dates[1]).ptr);
                writer.Key("phase"); writer.Int(proc_info.phase);
                writer.Key("activities"); writer.Int(proc_info.ActivitiesToDec());
                if (proc_info.extensions > 1) {
                    writer.Key("extensions"); writer.Int(proc_info.ExtensionsToDec());
                }
                writer.EndObject();
            }
        }
        writer.EndArray();
        return true;
    });

    return response;
}

// TODO: Add ghm_ghs export to drdR
Response ProduceGhmGhs(const ConnectionInfo *conn, const char *, CompressionType compression_type)
{
    Response response = {};
    const mco_TableIndex *index = GetIndexFromQueryString(conn, "ghm_ghs.json", &response);
    if (!index)
        return response;

    const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
        *drdw_index_to_constraints[index - drdw_table_set->indexes.ptr];

    response.code = 200;
    response.response = BuildJson(compression_type,
                                  [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[512];

        writer.StartArray();
        for (const mco_GhmRootInfo &ghm_root_info: index->ghm_roots) {
            Span<const mco_GhmToGhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
            for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
                mco_GhsCode ghs = ghm_to_ghs_info.Ghs(Sector::Public);

                const mco_GhsPriceInfo *ghs_price_info = index->FindGhsPrice(ghs, Sector::Public);
                const mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);
                if (!constraint)
                    continue;

                uint32_t combined_durations = constraint->durations &
                                              ~((1u << ghm_to_ghs_info.minimal_duration) - 1);

                writer.StartObject();

                writer.Key("ghm"); writer.String(ghm_to_ghs_info.ghm.ToString(buf).ptr);
                writer.Key("ghm_root"); writer.String(ghm_to_ghs_info.ghm.Root().ToString(buf).ptr);
                if (ghm_root_info.young_severity_limit) {
                    writer.Key("young_age_treshold"); writer.Int(ghm_root_info.young_age_treshold);
                    writer.Key("young_severity_limit"); writer.Int(ghm_root_info.young_severity_limit);
                }
                if (ghm_root_info.old_severity_limit) {
                    writer.Key("old_age_treshold"); writer.Int(ghm_root_info.old_age_treshold);
                    writer.Key("old_severity_limit"); writer.Int(ghm_root_info.old_severity_limit);
                }
                writer.Key("durations"); writer.Uint(combined_durations);

                writer.Key("ghs"); writer.Int(ghm_to_ghs_info.Ghs(Sector::Public).number);
                if ((combined_durations & 1) &&
                        (constraint->warnings & (int)mco_GhmConstraint::Warning::PreferCmd28)) {
                    writer.Key("warn_cmd28"); writer.Bool(true);
                }
                if (ghm_root_info.confirm_duration_treshold) {
                    writer.Key("confirm_treshold"); writer.Int(ghm_root_info.confirm_duration_treshold);
                }
                if (ghm_to_ghs_info.unit_authorization) {
                    writer.Key("unit_authorization"); writer.Int(ghm_to_ghs_info.unit_authorization);
                }
                if (ghm_to_ghs_info.bed_authorization) {
                    writer.Key("bed_authorization"); writer.Int(ghm_to_ghs_info.bed_authorization);
                }
                if (ghm_to_ghs_info.minimal_duration) {
                    writer.Key("minimum_duration"); writer.Int(ghm_to_ghs_info.minimal_duration);
                }
                if (ghm_to_ghs_info.minimal_age) {
                    writer.Key("minimum_age"); writer.Int(ghm_to_ghs_info.minimal_age);
                }
                if (ghm_to_ghs_info.main_diagnosis_mask.value) {
                    writer.Key("main_diagnosis");
                    writer.String(Fmt(buf, "D$%1.%2",
                                      ghm_to_ghs_info.main_diagnosis_mask.offset,
                                      ghm_to_ghs_info.main_diagnosis_mask.value).ptr);
                }
                if (ghm_to_ghs_info.diagnosis_mask.value) {
                    writer.Key("diagnoses");
                    writer.String(Fmt(buf, "D$%1.%2",
                                      ghm_to_ghs_info.diagnosis_mask.offset,
                                      ghm_to_ghs_info.diagnosis_mask.value).ptr);
                }
                if (ghm_to_ghs_info.procedure_masks.len) {
                    writer.Key("procedures"); writer.StartArray();
                    for (const ListMask &mask: ghm_to_ghs_info.procedure_masks) {
                        writer.String(Fmt(buf, "A$%1.%2", mask.offset, mask.value).ptr);
                    }
                    writer.EndArray();
                }

                if (ghs_price_info) {
                    writer.Key("ghs_cents"); writer.Int(ghs_price_info->ghs_cents);
                    writer.Key("ghs_coefficient"); writer.Double(index->GhsCoefficient(Sector::Public));
                    if (ghs_price_info->exh_treshold) {
                        writer.Key("exh_treshold"); writer.Int(ghs_price_info->exh_treshold);
                        writer.Key("exh_cents"); writer.Int(ghs_price_info->exh_cents);
                    }
                    if (ghs_price_info->exb_treshold) {
                        writer.Key("exb_treshold"); writer.Int(ghs_price_info->exb_treshold);
                        writer.Key("exb_cents"); writer.Int(ghs_price_info->exb_cents);
                        if (ghs_price_info->flags & (int)mco_GhsPriceInfo::Flag::ExbOnce) {
                            writer.Key("exb_once"); writer.Bool(true);
                        }
                    }
                }

                writer.EndObject();
            }
        }
        writer.EndArray();
        return true;
    });

    return response;
}
