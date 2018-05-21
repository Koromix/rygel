// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "drdw.hh"

Response ProduceIndexes(MHD_Connection *, const char *, CompressionType compression_type)
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

Response ProducePriceMap(MHD_Connection *conn, const char *, CompressionType compression_type)
{
    Date date = {};
    {
        const char *date_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "date");
        if (date_str) {
            date = Date::FromString(date_str);
        }
        if (!date.value)
            return CreateErrorPage(422);
    }

    const mco_TableIndex *index = drdw_table_set->FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        return CreateErrorPage(404);
    }

    // Redirect to the canonical URL for this version, to improve client-side caching
    if (date != index->limit_dates[0]) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);

        {
            char url_buf[64];
            Fmt(url_buf, "price_map.json?date=%1", index->limit_dates[0]);
            MHD_add_response_header(response, "Location", url_buf);
        }

        return {303, response};
    }
    const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
        *drdw_index_to_constraints[index - drdw_table_set->indexes.ptr];

    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[512];

        writer.StartArray();
        for (const mco_GhmRootInfo &ghm_root_info: index->ghm_roots) {
            writer.StartObject();
            writer.Key("ghm_root"); writer.String(Fmt(buf, "%1", ghm_root_info.ghm_root).ptr);
            writer.Key("ghs"); writer.StartArray();

            Span<const mco_GhmToGhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
            for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
                mco_GhsCode ghs = ghm_to_ghs_info.Ghs(Sector::Public);

                const mco_GhsPriceInfo *ghs_price_info = index->FindGhsPrice(ghs, Sector::Public);
                const mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);
                if (!constraint)
                    continue;

                uint32_t combined_durations = constraint->durations;
                combined_durations &= ~((1u << ghm_to_ghs_info.minimal_duration) - 1);

                writer.StartObject();
                writer.Key("ghm"); writer.String(Fmt(buf, "%1", ghm_to_ghs_info.ghm).ptr);
                writer.Key("ghm_mode"); writer.String(&ghm_to_ghs_info.ghm.parts.mode, 1);
                writer.Key("durations"); writer.Uint(combined_durations);
                if ((combined_durations & 1) &&
                        (constraint->warnings & (int)mco_GhmConstraint::Warning::PreferCmd28)) {
                    writer.Key("warn_cmd28"); writer.Bool(true);
                }
                if (ghm_root_info.young_severity_limit) {
                    writer.Key("young_age_treshold"); writer.Int(ghm_root_info.young_age_treshold);
                    writer.Key("young_severity_limit"); writer.Int(ghm_root_info.young_severity_limit);
                }
                if (ghm_root_info.old_severity_limit) {
                    writer.Key("old_age_treshold"); writer.Int(ghm_root_info.old_age_treshold);
                    writer.Key("old_severity_limit"); writer.Int(ghm_root_info.old_severity_limit);
                }
                writer.Key("ghs"); writer.Int(ghs.number);

                writer.Key("conditions"); writer.StartArray();
                if (ghm_to_ghs_info.bed_authorization) {
                    writer.String(Fmt(buf, "Autorisation Lit %1", ghm_to_ghs_info.bed_authorization).ptr);
                }
                if (ghm_to_ghs_info.unit_authorization) {
                    writer.String(Fmt(buf, "Autorisation Unité %1", ghm_to_ghs_info.unit_authorization).ptr);
                    if (ghm_to_ghs_info.minimal_duration) {
                        writer.String(Fmt(buf, "Durée Unitée Autorisée ≥ %1", ghm_to_ghs_info.minimal_duration).ptr);
                    }
                } else if (ghm_to_ghs_info.minimal_duration) {
                    writer.String(Fmt(buf, "Durée ≥ %1", ghm_to_ghs_info.minimal_duration).ptr);
                }
                if (ghm_to_ghs_info.minimal_age) {
                    writer.String(Fmt(buf, "Age ≥ %1", ghm_to_ghs_info.minimal_age).ptr);
                }
                if (ghm_to_ghs_info.main_diagnosis_mask.value) {
                    writer.String(Fmt(buf, "DP de la liste D$%1.%2",
                                      ghm_to_ghs_info.main_diagnosis_mask.offset,
                                      ghm_to_ghs_info.main_diagnosis_mask.value).ptr);
                }
                if (ghm_to_ghs_info.diagnosis_mask.value) {
                    writer.String(Fmt(buf, "Diagnostic de la liste D$%1.%2",
                                      ghm_to_ghs_info.diagnosis_mask.offset,
                                      ghm_to_ghs_info.diagnosis_mask.value).ptr);
                }
                for (const ListMask &mask: ghm_to_ghs_info.procedure_masks) {
                    writer.String(Fmt(buf, "Acte de la liste A$%1.%2",
                                      mask.offset, mask.value).ptr);
                }
                writer.EndArray();

                if (ghs_price_info) {
                    writer.Key("ghs_cents"); writer.Int(ghs_price_info->ghs_cents);
                    writer.Key("ghs_coefficient"); writer.Double(ghs_price_info->ghs_coefficient);
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
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    });

    return {200, response};
}
