/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../core/libmoya.hh"
#include "pages.hh"
#include "../../lib/libmicrohttpd/src/include/microhttpd.h"
#include "../../lib/rapidjson/memorybuffer.h"
#include "../../lib/rapidjson/prettywriter.h"

static const char *const UsageText =
R"(Usage: talyn [options]

Options:
    -T, --table-dir <path>       Load table directory
        --table-file <path>      Load table file
    -P, --pricing <path>         Load pricing file

    -A, --authorization <path>   Load authorization file)";

static TableSet main_table_set = {};
static PricingSet main_pricing_set = {};
static AuthorizationSet main_authorization_set = {};

static HashMap<const char *, ArrayRef<const char>> resources;

// FIXME: Switch to stream / callback-based API
static bool BuildYaaJson(Date date, rapidjson::MemoryBuffer *out_buffer)
{
    const TableIndex *index = main_table_set.FindIndex(date);
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
        writer.Key("ghm_root"); writer.String(Fmt(&temp_alloc, "%1", ghm_root_info.code).ptr);
        writer.Key("info"); writer.StartArray();

        ArrayRef<const GhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.code);
        for (const GhsInfo &ghs_info: compatible_ghs) {
            const GhsPricing *ghs_pricing = main_pricing_set.FindGhsPricing(ghs_info.ghs[0], date);
            if (!ghs_pricing)
                continue;

            writer.StartObject();
            writer.Key("ghm"); writer.String(Fmt(&temp_alloc, "%1", ghs_info.ghm).ptr);
            writer.Key("ghm_mode"); writer.String(&ghs_info.ghm.parts.mode, 1);
            if (ghs_info.ghm.parts.mode >= '1' && ghs_info.ghm.parts.mode < '5') {
                int treshold;
                if (ghs_info.ghm.parts.mode >= '2') {
                    treshold = GetMinimalDurationForSeverity(ghs_info.ghm.parts.mode - '1');
                } else {
                    treshold = 0;
                }
                if (treshold < ghm_root_info.short_duration_treshold) {
                    treshold = ghm_root_info.short_duration_treshold;
                } else if (!treshold && ghm_root_info.allow_ambulatory) {
                    treshold = 1;
                }
                if (treshold) {
                    writer.Key("low_duration_limit"); writer.Int(treshold);
                }
            } else if (ghs_info.ghm.parts.mode >= 'B' && ghs_info.ghm.parts.mode < 'E') {
                int treshold = GetMinimalDurationForSeverity(ghs_info.ghm.parts.mode - 'A');
                writer.Key("low_duration_limit"); writer.Int(treshold);
            } else if (ghs_info.ghm.parts.mode == 'J') {
                writer.Key("high_duration_limit"); writer.Int(1);
            } else if (ghs_info.ghm.parts.mode == 'T') {
                if (ghm_root_info.allow_ambulatory) {
                    writer.Key("low_duration_limit"); writer.Int(1);
                }
                writer.Key("high_duration_limit"); writer.Int(ghm_root_info.short_duration_treshold);
            }
            writer.Key("ghs"); writer.Int(ghs_pricing->code.number);
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

    if (StrTest(url, "/catalog.json")) {
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
    } else {
        ArrayRef<const char> resource_data = resources.FindValue(url, {});
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
    Allocator temp_alloc;
    OptionParser opt_parser(argc, argv);

    HeapArray<const char *> table_filenames;
    const char *pricing_filename = nullptr;
    const char *authorization_filename = nullptr;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintLn("%1", UsageText);
                return 0;
            } else if (opt_parser.TestOption("-T", "--table-dir")) {
                if (!opt_parser.RequireOptionValue(UsageText))
                    return 1;

                if (!EnumerateDirectoryFiles(opt_parser.current_value, "*.tab", temp_alloc,
                                             &table_filenames, 1024))
                    return 1;
            } else if (opt_parser.TestOption("--table-file")) {
                if (!opt_parser.RequireOptionValue(UsageText))
                    return 1;

                table_filenames.Append(opt_parser.current_value);
            } else if (opt_parser.TestOption("-P", "--pricing")) {
                if (!opt_parser.RequireOptionValue(UsageText))
                    return 1;

                pricing_filename = opt_parser.current_value;
            } else if (opt_parser.TestOption("-A", "--authorization")) {
                if (!opt_parser.RequireOptionValue(UsageText))
                    return 1;

                authorization_filename = opt_parser.current_value;
            } else {
                PrintLn(stderr, "Unknown option '%1'", opt);
                PrintLn(stderr, "%1", UsageText);
                return 1;
            }
        }
    }
    if (!table_filenames.len) {
        LogError("No table provided");
        return 1;
    }
    if (!pricing_filename) {
        LogError("No pricing file specified");
        return 1;
    }
    if (!authorization_filename || !authorization_filename[0]) {
        LogError("No authorization file specified, ignoring");
        authorization_filename = nullptr;
    }

    LoadTableFiles(table_filenames, &main_table_set);
    if (!main_table_set.indexes.len)
        return 1;
    if (!LoadPricingFile(pricing_filename, &main_pricing_set))
        return 1;
    if (authorization_filename && !LoadAuthorizationFile(authorization_filename,
                                                         &main_authorization_set))
        return 1;

    resources.Set("/", page_index);
    for (const Page &page: pages) {
        resources.Set(page.url, page_index);
    }

    MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_AUTO_INTERNAL_THREAD | MHD_USE_ERROR_LOG, 8888,
        nullptr, nullptr, HandleHttpConnection, nullptr, MHD_OPTION_END);
    if (!daemon)
        return 1;
    DEFER { MHD_stop_daemon(daemon); };

    (void)getchar();

    return 0;
}
