// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "events.hh"
#include "files.hh"
#include "goupile.hh"
#include "schedule.hh"
#include "user.hh"
#include "../../web/libserver/libserver.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

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

static Size ConvertToJsName(const char *name, Span<char> out_buf)
{
    // This is used for static strings (e.g. permission names), and the Span<char>
    // output buffer will abort debug builds on out-of-bounds access.

    if (name[0]) {
        out_buf[0] = LowerAscii(name[0]);

        Size j = 1;
        for (Size i = 1; name[i]; i++) {
            if (name[i] >= 'A' && name[i] <= 'Z') {
                out_buf[j++] = '_';
                out_buf[j++] = LowerAscii(name[i]);
            } else {
                out_buf[j++] = name[i];
            }
        }
        out_buf[j] = 0;

        return j;
    } else {
        out_buf[0] = 0;
        return 0;
    }
}

static void HandleSettings(const http_RequestInfo &request, http_IO *io)
{
    std::shared_ptr<const Session> session = GetCheckedSession(request, io);
    if (!session)
        return;

    http_JsonPageBuilder json(request.compression_type);

    json.StartObject();

    json.Key("username"); json.String(session->username);

    json.Key("permissions"); json.StartObject();
    for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
        char js_name[64];
        ConvertToJsName(UserPermissionNames[i], js_name);

        json.Key(js_name); json.Bool(session->permissions & (1 << i));
    }
    json.EndObject();

    json.EndObject();

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
        if (TestStr(asset.name, "goupile.html") || TestStr(asset.name, "sw.pk.js") ||
                TestStr(asset.name, "manifest.json")) {
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
            } else if (TestStr(request.url, "/manifest.json") && goupile_config.use_offline) {
                asset = assets_map.Find("manifest.json");
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

            if (TestStr(request.url, "/api/settings.json")) {
                func = HandleSettings;
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
    } else if (TestStr(request.method, "POST")) {
        if (TestStr(request.url, "/api/login.json")) {
            HandleLogin(request, io);
        } else if (TestStr(request.url, "/api/logout.json")) {
            HandleLogout(request, io);
        } else {
            io->AttachError(404);
        }
    } else if (TestStr(request.method, "PUT")) {
        HandleFilePut(request, io);
    } else if (TestStr(request.method, "DELETE")) {
        HandleFileDelete(request, io);
    } else {
        io->AttachError(405);
    }
}

int RunGoupile(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupile [options]

Options:
    -C, --config_file <file>     Set configuration file

        --port <port>            Change web server port
                                 (default: %1)
        --base_url <url>         Change base URL
                                 (default: %2))",
                goupile_config.http.port, goupile_config.http.base_url);
    };

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
    if (config_filename && !LoadConfig(config_filename, &goupile_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
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
    {
        if (!config_filename) {
            LogError("Configuration file must be specified");
            return 1;
        }

        bool valid = true;

        if (!goupile_config.app_key || !goupile_config.app_key[0]) {
            LogError("Project key must not be empty");
            valid = false;
        }
        if (!goupile_config.files_directory) {
            LogError("Application directory not specified");
            valid = false;
        } else if (!TestFile(goupile_config.files_directory, FileType::Directory)) {
            LogError("Application directory '%1' does not exist", goupile_config.files_directory);
            valid = false;
        }
        if (!goupile_config.database_filename) {
            LogError("Database file not specified");
            valid = false;
        }

        if (!valid)
            return 1;
    }

    // Init database
    if (!goupile_db.Open(goupile_config.database_filename, SQLITE_OPEN_READWRITE))
        return 1;

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

    // We need to send keep-alive notices to SSE clients quite often for two reasons:
    // first, disconnects are detected faster by the client but most importantly, Firefox
    // does not close a connection (for example after tab close) until it receives an event.
    // Because Firefox also prevents more than 6 HTTP connections to the same server,
    // this means that a long keep-alive time can block goupile for a long time in some
    // situations, such as a few page refreshes.
    while (!WaitForInterruption(15000)) {
        PushEvent(EventType::KeepAlive);
    }
    CloseAllEventConnections();

    LogInfo("Exit");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunGoupile(argc, argv); }
