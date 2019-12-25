// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
#include "../../libcc/libcc.hh"
#include "config.hh"
#include "data.hh"
#include "events.hh"
#include "files.hh"
#include "goupile.hh"
#include "schedule.hh"
#include "../../web/libserver/libserver.hh"

namespace RG {

Config goupile_config;
SQLiteDatabase goupile_db;

char goupile_etag[33];

#ifndef NDEBUG
static const char *assets_filename;
static AssetSet asset_set;
#else
extern "C" const Span<const AssetInfo> pack_assets;
#endif
static HashTable<const char *, AssetInfo> assets_map;
static BlockAllocator assets_alloc;

static void HandleManifest(const http_RequestInfo &request, http_IO *io)
{
    http_JsonPageBuilder json(request.compression_type);

    json.StartObject();
    json.Key("short_name"); json.String(goupile_config.app_name);
    json.Key("name"); json.String(goupile_config.app_name);
    json.Key("icons"); json.StartArray();
    json.StartObject();
        json.Key("src"); json.String("favicon.png");
        json.Key("type"); json.String("image/png");
        json.Key("sizes"); json.String("192x192 512x512");
    json.EndObject();
    json.EndArray();
    json.Key("start_url"); json.String(goupile_config.http.base_url);
    json.Key("display"); json.String("standalone");
    json.Key("scope"); json.String(goupile_config.http.base_url);
    json.Key("background_color"); json.String("#f8f8f8");
    json.Key("theme_color"); json.String("#dee1e6");
    json.EndObject();

    io->AddCachingHeaders(goupile_config.max_age, nullptr);
    return json.Finish(io);
}

static AssetInfo PatchGoupileVariables(const AssetInfo &asset, Allocator *alloc)
{
    AssetInfo asset2 = asset;
    asset2.data = PatchAssetVariables(asset, alloc,
                                      [](const char *key, StreamWriter *writer) {
        if (TestStr(key, "VERSION")) {
            writer->Write(BuildVersion);
            return true;
        } else if (TestStr(key, "APP_KEY")) {
            writer->Write(goupile_config.app_key);
            return true;
        } else if (TestStr(key, "APP_NAME")) {
            writer->Write(goupile_config.app_name);
            return true;
        } else if (TestStr(key, "BASE_URL")) {
            writer->Write(goupile_config.http.base_url);
            return true;
        } else if (TestStr(key, "USE_OFFLINE")) {
            writer->Write(goupile_config.use_offline ? "true" : "false");
            return true;
        } else if (TestStr(key, "CACHE_KEY")) {
#ifdef NDEBUG
            writer->Write(BuildVersion);
#else
            writer->Write(goupile_etag);
#endif
            return true;
        } else if (TestStr(key, "LINK_MANIFEST")) {
            if (goupile_config.use_offline) {
                Print(writer, "<link rel=\"manifest\" href=\"%1manifest.json\"/>", goupile_config.http.base_url);
            }
            return true;
        } else if (TestStr(key, "SSE_KEEP_ALIVE")) {
            Print(writer, "%1", goupile_config.sse_keep_alive);
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
        Fmt(goupile_etag, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }

    // Packed static assets
    for (const AssetInfo &asset: assets) {
        if (TestStr(asset.name, "goupile.html") || TestStr(asset.name, "sw.pk.js")) {
            AssetInfo asset2 = PatchGoupileVariables(asset, &assets_alloc);
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
        // Try application files first
        if (HandleFileGet(request, io))
            return;

        // Now try static assets
        {
            const AssetInfo *asset = nullptr;

            if (TestStr(request.url, "/") ||
                    !strncmp(request.url, "/app/", 5) || !strncmp(request.url, "/main/", 6)) {
                asset = assets_map.Find("goupile.html");
            } else if (TestStr(request.url, "/favicon.png")) {
                asset = assets_map.Find("favicon.png");
            } else if (TestStr(request.url, "/sw.pk.js")) {
                asset = assets_map.Find("sw.pk.js");
            } else if (!strncmp(request.url, "/static/", 8)) {
                const char *asset_name = request.url + 8;
                asset = assets_map.Find(asset_name);
            }

            if (asset) {
                const char *etag = request.GetHeaderValue("If-None-Match");

                if (etag && TestStr(etag, goupile_etag)) {
                    MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
                    io->AttachResponse(304, response);
                } else {
                    const char *mimetype = http_GetMimeType(GetPathExtension(asset->name));
                    io->AttachBinary(200, asset->data, mimetype, asset->compression_type);

                    io->AddCachingHeaders(goupile_config.max_age, goupile_etag);
                    if (asset->source_map) {
                        io->AddHeader("SourceMap", asset->source_map);
                    }
                }

                return;
            }
        }

        // And last (but not least), API endpoints
        {
            void (*func)(const http_RequestInfo &request, http_IO *io) = nullptr;

            if (TestStr(request.url, "/manifest.json") && goupile_config.use_offline) {
                func = HandleManifest;
            } else if (TestStr(request.url, "/api/events.json")) {
                func = HandleEvents;
            } else if (TestStr(request.url, "/api/files.json")) {
                func = HandleFileList;
            } else if (TestStr(request.url, "/api/schedule/resources.json")) {
                func = HandleScheduleResources;
            } else if (TestStr(request.url, "/api/schedule/meetings.json")) {
                func = HandleScheduleMeetings;
            }

            if (func) {
                (*func)(request, io);
                return;
            }
        }

        // Found nothing
        io->AttachError(404);
    } else if (TestStr(request.method, "PUT")) {
        HandleFilePut(request, io);
    } else if (TestStr(request.method, "DELETE")) {
        HandleFileDelete(request, io);
    } else {
        io->AttachError(405);
    }
}

int RunGoupil(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupile [options]

Options:
    -C, --config_file <file>     Set configuration file

        --port <port>            Change web server port
                                 (default: %1)
        --base_url <url>         Change base URL
                                 (default: %2)

        --dev [<key>]            Run with fake profile and data)",
                goupile_config.http.port, goupile_config.http.base_url);
    };

    // Find config filename
    const char *config_filename = nullptr;
    const char *dev_key = nullptr;
    {
        OptionParser opt(argc, argv, (int)OptionParser::Flag::SkipNonOptions);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
                config_filename = opt.current_value;
            } else if (opt.Test("--dev", OptionType::OptionalValue)) {
                dev_key = opt.current_value ? opt.current_value : "DEV";
            }
        }
    }

    // Load config file
    if (config_filename) {
        if (dev_key) {
            LogError("Option '--dev' cannot be used with '--config_file'");
            return 1;
        }

        if (!LoadConfig(config_filename, &goupile_config))
            return 1;

        if (!goupile_config.app_name) {
            goupile_config.app_name = goupile_config.app_key;
        }
    } else if (dev_key) {
        goupile_config.app_key = dev_key;
        goupile_config.app_name = Fmt(&temp_alloc, "goupile (%1)", dev_key).ptr;
    }

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--dev", OptionType::OptionalValue)) {
                // Already handled
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseDec(opt.current_value, &goupile_config.http.port))
                    return 1;
            } else if (opt.Test("--base_url", OptionType::Value)) {
                goupile_config.http.base_url = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    // Check project configuration
    if (!goupile_config.app_key || !goupile_config.app_key[0]) {
        LogError("Project key must not be empty");
        return 1;
    }
    if (goupile_config.files_directory &&
            !TestFile(goupile_config.files_directory, FileType::Directory)) {
        LogError("Application directory '%1' does not exist", goupile_config.files_directory);
        return 1;
    }

    // Init database
    if (goupile_config.database_filename) {
        if (!goupile_db.Open(goupile_config.database_filename, SQLITE_OPEN_READWRITE))
            return 1;
    } else if (dev_key) {
        if (!goupile_db.Open(":memory:", SQLITE_OPEN_READWRITE))
            return 1;
        if (!goupile_db.CreateSchema())
            return 1;
        if (!goupile_db.InsertDemo())
            return 1;
    } else {
        LogError("Database file not specified");
        return 1;
    }

    // Init assets and files
#ifndef NDEBUG
    assets_filename = Fmt(&temp_alloc, "%1%/goupile_assets%2",
                          GetApplicationDirectory(), RG_SHARED_LIBRARY_EXTENSION).ptr;
    if (asset_set.LoadFromLibrary(assets_filename) == AssetLoadStatus::Error)
        return 1;
#endif
    InitAssets();
    if (goupile_config.files_directory && !InitFiles())
        return 1;

    // Run!
    http_Daemon daemon;
    if (!daemon.Start(goupile_config.http, HandleRequest))
        return 1;
    LogInfo("Listening on port %1 (%2 stack)",
            goupile_config.http.port, IPStackNames[(int)goupile_config.http.ip_stack]);

    // We need to send keep-alive notices to SSE clients
    while (!WaitForInterruption(goupile_config.sse_keep_alive)) {
        PushEvent(EventType::KeepAlive);
    }
    CloseAllEventConnections();

    LogInfo("Exit");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunGoupil(argc, argv); }
