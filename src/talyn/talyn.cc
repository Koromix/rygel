/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../moya/libmoya.hh"
#ifndef _WIN32
    #include <signal.h>
#endif
#include "resources.hh"
#include "../../lib/libmicrohttpd/src/include/microhttpd.h"
#include "../../lib/rapidjson/memorybuffer.h"
#include "../../lib/rapidjson/prettywriter.h"

struct Page {
    const char *const url;
    const char *const name;
};

static const Page pages[] = {
    {"/tables", "Tables"},
};

static const TableSet *table_set;
static const PricingSet *pricing_set;
static const AuthorizationSet *authorization_set;

static HashMap<const char *, ArrayRef<const uint8_t>> routes;

// FIXME: Switch to stream / callback-based API
static bool BuildYaaJson(Date date, rapidjson::MemoryBuffer *out_buffer)
{
    const TableIndex *index = table_set->FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        return false;
    }

    Allocator temp_alloc;
    rapidjson::PrettyWriter<rapidjson::MemoryBuffer> writer(*out_buffer);

    writer.StartArray();
    for (const GhmRootInfo &ghm_root_info: index->ghm_roots) {
        writer.StartObject();
        // TODO: Use buffer-based Fmt API
        writer.Key("ghm_root"); writer.String(Fmt(&temp_alloc, "%1", ghm_root_info.ghm_root).ptr);
        writer.Key("info"); writer.StartArray();

        ArrayRef<const GhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
        for (const GhsInfo &ghs_info: compatible_ghs) {
            const GhsPricing *ghs_pricing = pricing_set->FindGhsPricing(ghs_info.ghs[0], date);
            if (!ghs_pricing)
                continue;

            writer.StartObject();
            writer.Key("ghm"); writer.String(Fmt(&temp_alloc, "%1", ghs_info.ghm).ptr);
            writer.Key("ghm_mode"); writer.String(&ghs_info.ghm.parts.mode, 1);
            if (ghs_info.ghm.parts.mode == 'J') {
                writer.Key("high_duration_limit"); writer.Int(1);
            } else if (ghs_info.ghm.parts.mode == 'T') {
                if (ghm_root_info.allow_ambulatory) {
                    writer.Key("low_duration_limit"); writer.Int(1);
                }
                writer.Key("high_duration_limit"); writer.Int(ghm_root_info.short_duration_treshold);
            } else {
                int treshold = 0;
                if (ghs_info.ghm.parts.mode >= '1' && ghs_info.ghm.parts.mode < '5') {
                    treshold = GetMinimalDurationForSeverity(ghs_info.ghm.Severity());
                } else if (ghs_info.ghm.parts.mode >= 'B' && ghs_info.ghm.parts.mode < 'E') {
                    treshold = GetMinimalDurationForSeverity(ghs_info.ghm.parts.mode - 'A');
                }
                if (treshold < ghm_root_info.short_duration_treshold) {
                    treshold = ghm_root_info.short_duration_treshold;
                } else if (!treshold && ghm_root_info.allow_ambulatory) {
                    treshold = 1;
                }
                if (treshold) {
                    writer.Key("low_duration_limit"); writer.Int(treshold);
                }
            }
            if (ghm_root_info.young_severity_limit) {
                writer.Key("young_age_treshold"); writer.Int(ghm_root_info.young_age_treshold);
                writer.Key("young_severity_limit"); writer.Int(ghm_root_info.young_severity_limit);
            }
            if (ghm_root_info.old_severity_limit) {
                writer.Key("old_age_treshold"); writer.Int(ghm_root_info.old_age_treshold);
                writer.Key("old_severity_limit"); writer.Int(ghm_root_info.old_severity_limit);
            }
            writer.Key("ghs"); writer.Int(ghs_pricing->ghs.number);

            writer.Key("conditions"); writer.StartArray();
            if (ghs_info.bed_authorization) {
                writer.String(Fmt(&temp_alloc, "Autorisation Lit %1", ghs_info.bed_authorization).ptr);
            }
            if (ghs_info.unit_authorization) {
                writer.String(Fmt(&temp_alloc, "Autorisation Unité %1", ghs_info.unit_authorization).ptr);
                if (ghs_info.minimal_duration) {
                    writer.String(Fmt(&temp_alloc, "Durée Unitée Autorisée ≥ %1", ghs_info.minimal_duration).ptr);
                }
            } else if (ghs_info.minimal_duration) {
                // TODO: Take into account in addition to constraints (when we plug them in)
                writer.String(Fmt(&temp_alloc, "Durée ≥ %1", ghs_info.minimal_duration).ptr);
            }
            if (ghs_info.minimal_age) {
                writer.String(Fmt(&temp_alloc, "Age ≥ %1", ghs_info.minimal_age).ptr);
            }
            if (ghs_info.main_diagnosis_mask) {
                writer.String(Fmt(&temp_alloc, "DP de la liste D$%1.%2",
                                  ghs_info.main_diagnosis_offset, ghs_info.main_diagnosis_mask).ptr);
            }
            if (ghs_info.diagnosis_mask) {
                writer.String(Fmt(&temp_alloc, "Diagnostic de la liste D$%1.%2",
                                  ghs_info.diagnosis_offset, ghs_info.diagnosis_mask).ptr);
            }
            if (ghs_info.proc_mask) {
                writer.String(Fmt(&temp_alloc, "Acte de la liste A$%1.%2",
                                  ghs_info.proc_offset, ghs_info.proc_mask).ptr);
            }
            writer.EndArray();

            writer.Key("price_cents"); writer.Int(ghs_pricing->sectors[0].price_cents);
            if (ghs_pricing->sectors[0].exh_treshold) {
                writer.Key("exh_treshold"); writer.Int(ghs_pricing->sectors[0].exh_treshold);
                writer.Key("exh_cents"); writer.Int(ghs_pricing->sectors[0].exh_cents);
            }
            if (ghs_pricing->sectors[0].exb_treshold) {
                writer.Key("exb_treshold"); writer.Int(ghs_pricing->sectors[0].exb_treshold);
                writer.Key("exb_cents"); writer.Int(ghs_pricing->sectors[0].exb_cents);
                if (ghs_pricing->sectors[0].flags & (int)GhsPricing::Flag::ExbOnce) {
                    writer.Key("exb_once"); writer.Bool(true);
                }
            }

            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
    }
    writer.EndArray();

    return true;
}

// TODO: Deny if URL too long (MHD option?)
static int HandleHttpConnection(void *, struct MHD_Connection *conn,
                                const char *url, const char *,
                                const char *, const char *,
                                size_t *, void **)
{
    const char *const error_page = "<html><body>Error</body></html>";

    MHD_Response *response = nullptr;
    unsigned int code = MHD_HTTP_INTERNAL_SERVER_ERROR;

    if (StrTest(url, "/api/catalog.json")) {
        Date date = {};
        {
            const char *date_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND,
                                                               "date");
            date = Date::FromString(date_str);
        }
        if (date.value) {
            rapidjson::MemoryBuffer buffer;
            if (BuildYaaJson(date, &buffer)) {
                response = MHD_create_response_from_buffer(buffer.GetSize(),
                                                           (void *)buffer.GetBuffer(),
                                                           MHD_RESPMEM_MUST_COPY);
                MHD_add_response_header(response, "Content-Type", "application/json");
                code = MHD_HTTP_OK;
            }
        }
    } else if (StrTest(url, "/api/pages.json")) {
        rapidjson::MemoryBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::MemoryBuffer> writer(buffer);
        writer.StartArray();
        for (const Page &page: pages) {
            writer.StartObject();
            writer.Key("url"); writer.Key(page.url);
            writer.Key("name"); writer.Key(page.name);
            writer.EndObject();
        }
        writer.EndArray();
        response = MHD_create_response_from_buffer(buffer.GetSize(),
                                                   (void *)buffer.GetBuffer(),
                                                   MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Content-Type", "application/json");
        code = MHD_HTTP_OK;
    } else {
        ArrayRef<const uint8_t> resource_data = routes.FindValue(url, {});
        if (resource_data.IsValid()) {
            response = MHD_create_response_from_buffer(resource_data.len,
                                                       (void *)resource_data.ptr,
                                                       MHD_RESPMEM_PERSISTENT);
            code = MHD_HTTP_OK;
        } else {
            code = MHD_HTTP_NOT_FOUND;
        }
    }
    if (!response) {
        response = MHD_create_response_from_buffer(strlen(error_page), (void *)error_page,
                                                   MHD_RESPMEM_PERSISTENT);
    }
    DEFER { MHD_destroy_response(response); };

    return MHD_queue_response(conn, code, response);
}

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, "%1",
R"(Usage: talyn [options]

Talyn options:
    -p, --port <port>            Web server port
                                 (default: 8888)

)");
        PrintLn(fp, "%1", main_options_usage);
    };

    Allocator temp_alloc;

    // Add default data directory
    {
        const char *default_data_dir = Fmt(&temp_alloc, "%1%/data", GetExecutableDirectory()).ptr;
        main_data_directories.Append(default_data_dir);
    }

    uint16_t port = 8888;
    {
        OptionParser opt_parser(argc, argv);

        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (TestOption(opt, "-p", "--port")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return 1;

                char *end_ptr;
                long new_port = strtol(opt_parser.current_value, &end_ptr, 10);
                if (end_ptr == opt_parser.current_value || end_ptr[0] ||
                        new_port < 0 || new_port >= 65536) {
                    LogError("Option '--port' requires a value between 0 and 65535");
                    return 1;
                }
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return 1;
            }
        }
    }

    table_set = GetMainTableSet();
    if (!table_set)
        return 1;
    pricing_set = GetMainPricingSet();
    if (!pricing_set)
        return 1;
    authorization_set = GetMainAuthorizationSet();
    if (!authorization_set)
        return 1;

    routes.Set("/", resources::talyn_html);
    for (const Page &page: pages) {
        routes.Set(page.url, resources::talyn_html);
    }
    routes.Set("/resources/talyn.css", resources::talyn_css);
    routes.Set("/resources/talyn.js", resources::talyn_js);
    routes.Set("/resources/logo.png", resources::logo_png);

    MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_AUTO_INTERNAL_THREAD | MHD_USE_ERROR_LOG, port,
        nullptr, nullptr, HandleHttpConnection, nullptr, MHD_OPTION_END);
    if (!daemon)
        return 1;
    DEFER { MHD_stop_daemon(daemon); };

#ifdef _WIN32
    (void)getchar();
#else
    {
        static volatile bool run = true;
        const auto do_exit = [](int) {
            run = false;
        };
        signal(SIGINT, do_exit);
        signal(SIGTERM, do_exit);

        while (run) {
            pause();
        }
    }
#endif

    return 0;
}
