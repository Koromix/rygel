// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "qtrace.hh"
#include "../../web/libhttp/libhttp.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

Config qtrace_config;
sq_Database qtrace_db;

char qtrace_etag[33];

#ifndef NDEBUG
static const char *assets_filename;
static AssetSet asset_set;
#else
extern "C" const Span<const AssetInfo> pack_assets;
#endif
static HashTable<const char *, AssetInfo> assets_map;
static BlockAllocator assets_alloc;

static AssetInfo PatchQTraceVariables(const AssetInfo &asset, Allocator *alloc)
{
    AssetInfo asset2 = asset;
    asset2.data = PatchAssetVariables(asset, alloc,
                                      [](const char *key, StreamWriter *writer) {
        if (TestStr(key, "VERSION")) {
            writer->Write(FelixVersion);
            return true;
        } else if (TestStr(key, "BASE_URL")) {
            writer->Write(qtrace_config.http.base_url);
            return true;
        } else {
            return false;
        }
    });

    return asset2;
}

static void InitAssets()
{
#ifdef NDEBUG
    Span<const AssetInfo> assets = pack_assets;
#else
    Span<const AssetInfo> assets = asset_set.assets;
#endif

    LogInfo(assets_map.count ? "Reload assets" : "Init assets");

    assets_map.Clear();
    assets_alloc.ReleaseAll();

    // We can use a global ETag because everything is in the binary
    {
        uint64_t buf[2];
        randombytes_buf(&buf, RG_SIZE(buf));
        Fmt(qtrace_etag, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }

    // Packed static assets
    for (const AssetInfo &asset: assets) {
        if (TestStr(asset.name, "qtrace.html")) {
            AssetInfo asset2 = PatchQTraceVariables(asset, &assets_alloc);
            assets_map.Append(asset2);
        } else {
            assets_map.Append(asset);
        }
    }
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
#ifndef NDEBUG
    if (asset_set.LoadFromLibrary(assets_filename) == AssetLoadStatus::Loaded) {
        InitAssets();
    }
#endif

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");

    if (TestStr(request.method, "GET")) {
        const AssetInfo *asset = nullptr;

        if (TestStr(request.url, "/")) {
            asset = assets_map.Find("qtrace.html");
        } else if (TestStr(request.url, "/favicon.png")) {
            asset = assets_map.Find("favicon.png");
        } else if (!strncmp(request.url, "/static/", 8)) {
            const char *asset_name = request.url + 8;
            asset = assets_map.Find(asset_name);
        }

        if (asset) {
            const char *etag = request.GetHeaderValue("If-None-Match");

            if (etag && TestStr(etag, qtrace_etag)) {
                MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
                io->AttachResponse(304, response);
            } else {
                const char *mimetype = http_GetMimeType(GetPathExtension(asset->name));
                io->AttachBinary(200, asset->data, mimetype, asset->compression_type);

                io->AddCachingHeaders(qtrace_config.max_age, qtrace_etag);
                if (asset->source_map) {
                    io->AddHeader("SourceMap", asset->source_map);
                }
            }

            return;
        } else {
            // Found nothing
            io->AttachError(404);
        }
    } else {
        io->AttachError(405);
    }
}

int RunQTrace(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: qtrace [options]

Options:
    -C, --config_file <file>     Set configuration file

        --port <port>            Change web server port
                                 (default: %1)
        --base_url <url>         Change base URL
                                 (default: %2))",
                qtrace_config.http.port, qtrace_config.http.base_url);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("qtrace %1", FelixVersion);
        return 0;
    }

    // Find config filename
    const char *config_filename = nullptr;
    {
        OptionParser opt(argc, argv, (int)OptionParser::Flag::SkipNonOptions);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
                config_filename = opt.current_value;
            }
        }
    }

    // Load config file
    if (config_filename && !LoadConfig(config_filename, &qtrace_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseDec(opt.current_value, &qtrace_config.http.port))
                    return 1;
            } else if (opt.Test("--base_url", OptionType::Value)) {
                qtrace_config.http.base_url = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    // Check project configuration
    {
        if (!config_filename) {
            LogError("Configuration file must be specified");
            return 1;
        }

        bool valid = true;

        if (!qtrace_config.database_filename) {
            LogError("Database file not specified");
            valid = false;
        }

        if (!valid)
            return 1;
    }

    // Init database
    if (!qtrace_db.Open(qtrace_config.database_filename, SQLITE_OPEN_READWRITE))
        return 1;

    // Init assets and files
#ifndef NDEBUG
    assets_filename = Fmt(&temp_alloc, "%1%/qtrace_assets%2",
                          GetApplicationDirectory(), RG_SHARED_LIBRARY_EXTENSION).ptr;
    if (asset_set.LoadFromLibrary(assets_filename) == AssetLoadStatus::Error)
        return 1;
#endif
    InitAssets();

    // Run!
    http_Daemon daemon;
    if (!daemon.Start(qtrace_config.http, HandleRequest))
        return 1;
    LogInfo("Listening on port %1 (%2 stack)",
            qtrace_config.http.port, IPStackNames[(int)qtrace_config.http.ip_stack]);

    WaitForInterruption();

    LogInfo("Exit");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunQTrace(argc, argv); }
