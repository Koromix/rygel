/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../core/libmoya.hh"
#include "pages.hh"
#include "../../lib/libmicrohttpd/src/include/microhttpd.h"
#include "../../lib/rapidjson/memorybuffer.h"
#include "../../lib/rapidjson/prettywriter.h"

static const char *const UsageText =
R"(Usage: moya_web [options]

Options:
    -t, --table-file <filename>  Load table file
    -T, --table-dir <dir>        Load table directory

    -p, --pricing <filename>     Load pricing file)";

static TableSet main_table_set = {};
static PricingSet main_pricing_set = {};

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
        for (const GhsInfo &ghs_info: index->ghs) {
            // FIXME: Implement and use FindIndex::FindGhs()
            if (ghs_info.ghm.Root() != ghm_root_info.code)
                continue;

            const GhsPricing *ghs_pricing = main_pricing_set.FindGhsPricing(ghs_info.ghs[0], date);
            if (!ghs_pricing)
                continue;

            writer.StartObject();
            writer.Key("ghm"); writer.String(Fmt(&temp_alloc, "%1", ghs_info.ghm).ptr);
            writer.Key("ghm_mode"); writer.String(&ghs_info.ghm.parts.mode, 1);
            if (ghs_info.ghm.parts.mode >= '1' && ghs_info.ghm.parts.mode < '5') {
                int treshold;
                if (ghs_info.ghm.parts.mode >= '2') {
                    // TODO: Make GetMinimumDuration(ghm) function in algorithm.cc
                    treshold = ghs_info.ghm.parts.mode - '0' + 1;
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
                int treshold = ghs_info.ghm.parts.mode - 'A' + 2;
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

static int HandleHttpConnection(void *, struct MHD_Connection *conn,
                                const char *url, const char *,
                                const char *, const char *,
                                size_t *, void **)
{
    const char *const error_page = "<html><body>Error</body></html>";

    MHD_Response *response = nullptr;
    unsigned int code;
    if (StrTest(url, "/")) {
        response = MHD_create_response_from_buffer(page_index.len,
                                                   (void *)page_index.ptr,
                                                   MHD_RESPMEM_PERSISTENT);
        code = MHD_HTTP_OK;
    } else if (StrTest(url, "/catalog.json")) {
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
            } else {
                code = MHD_HTTP_INTERNAL_SERVER_ERROR;
            }
        } else {
            code = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
    } else {
        code = MHD_HTTP_NOT_FOUND;
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
            } else if (opt_parser.TestOption("-t", "--table-file")) {
                if (!opt_parser.RequireOptionValue(UsageText))
                    return 1;

                table_filenames.Append(opt_parser.current_value);
            } else if (opt_parser.TestOption("-p", "--pricing")) {
                if (!opt_parser.RequireOptionValue(UsageText))
                    return 1;

                pricing_filename = opt_parser.current_value;
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

    LoadTableSet(table_filenames, &main_table_set);
    if (!main_table_set.indexes.len)
        return 1;
    if (!LoadPricingSet(pricing_filename, &main_pricing_set))
        return 1;

    MHD_Daemon *daemon = MHD_start_daemon(MHD_USE_AUTO_INTERNAL_THREAD, 8888,
                                          nullptr, nullptr, HandleHttpConnection,
                                          nullptr, MHD_OPTION_END);
    if (!daemon)
        return 1;
    DEFER { MHD_stop_daemon(daemon); };

    (void)getchar();

    return 0;
}
