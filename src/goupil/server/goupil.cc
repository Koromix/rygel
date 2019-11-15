// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
#include "../../libcc/libcc.hh"
#include "config.hh"
#include "data.hh"
#include "files.hh"
#include "goupil.hh"
#include "schedule.hh"
#include "../../wrappers/http.hh"

namespace RG {

struct PushContext {
    PushContext *next;

    std::mutex mutex;
    MHD_Connection *conn;

    unsigned int events;
};

Config goupil_config;
SQLiteDatabase goupil_db;

static char etag[64];

#ifndef NDEBUG
static const char *assets_filename;
static AssetSet asset_set;
#else
extern "C" const Span<const AssetInfo> pack_assets;
#endif
static HashTable<const char *, AssetInfo> assets_map;
static BlockAllocator assets_alloc;

static std::atomic_bool push_run = true;
static std::atomic<PushContext *> push_head;
static std::atomic_int push_count;

static void HandleManifest(const http_RequestInfo &request, http_IO *io)
{
    http_JsonPageBuilder json(request.compression_type);

    json.StartObject();
    json.Key("short_name"); json.String(goupil_config.app_name);
    json.Key("name"); json.String(goupil_config.app_name);
    json.Key("icons"); json.StartArray();
    json.StartObject();
        json.Key("src"); json.String("favicon.png");
        json.Key("type"); json.String("image/png");
        json.Key("sizes"); json.String("192x192 512x512");
    json.EndObject();
    json.EndArray();
    json.Key("start_url"); json.String(goupil_config.http.base_url);
    json.Key("display"); json.String("standalone");
    json.Key("scope"); json.String(goupil_config.http.base_url);
    json.Key("background_color"); json.String("#f8f8f8");
    json.Key("theme_color"); json.String("#24579d");
    json.EndObject();

    io->flags |= (int)http_IO::Flag::EnableCache;
    return json.Finish(io);
}

static void PushEvents(unsigned int events)
{
    static std::mutex push_mutex;
    std::lock_guard<std::mutex> push_lock(push_mutex);

    PushContext *ctx = nullptr;
    while (!push_head.compare_exchange_strong(ctx, nullptr));

    while (ctx) {
        std::lock_guard<std::mutex> ctx_lock(ctx->mutex);

        PushContext *next = ctx->next;
        ctx->events |= events;
        MHD_resume_connection(ctx->conn);

        ctx = next;
    }
}

void PushEvent(EventType type)
{
    PushEvents(1u << (int)type);
}

static ssize_t SendPendingEvents(void *cls, uint64_t, char *buf, size_t max)
{
    PushContext *ctx = (PushContext *)cls;

    std::lock_guard<std::mutex> ctx_lock(ctx->mutex);

    if (push_run) {
        if (ctx->events) {
            int ctz = CountTrailingZeros(ctx->events);
            ctx->events &= ~(1u << ctz);

            // FIXME: This may result in truncation when max is very low
            return Fmt(MakeSpan(buf, max), "event: %1\ndata:\n\n", EventTypeNames[ctz]).len;
        } else {
            ctx->next = nullptr;
            while (!push_head.compare_exchange_strong(ctx->next, ctx));
            MHD_suspend_connection(ctx->conn);

            // libmicrohttpd crashes (assert) if you return 0
            buf[0] = '\n';
            return 1;
        }
    } else {
        return MHD_CONTENT_READER_END_OF_STREAM;
    }
}

static void FreePushContext(void *cls)
{
    PushContext *ctx = (PushContext *)cls;
    delete ctx;

    push_count--;
}

static void HandleEvents(const http_RequestInfo &request, http_IO *io)
{
    // TODO: Use the allocator buried in http_RequestInfo?
    PushContext *ctx = new PushContext();
    ctx->conn = request.conn;

    MHD_Response *response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 1024,
                                                               SendPendingEvents, ctx, FreePushContext);
    io->AttachResponse(200, response);
    io->AddHeader("Content-Type", "text/event-stream");

    push_count++;
}

static AssetInfo PatchGoupilVariables(const AssetInfo &asset, Allocator *alloc)
{
    AssetInfo asset2 = asset;
    asset2.data = PatchAssetVariables(asset, alloc,
                                      [](const char *key, StreamWriter *writer) {
        if (TestStr(key, "VERSION")) {
            writer->Write(BuildVersion);
            return true;
        } else if (TestStr(key, "APP_KEY")) {
            writer->Write(goupil_config.app_key);
            return true;
        } else if (TestStr(key, "APP_NAME")) {
            writer->Write(goupil_config.app_name);
            return true;
        } else if (TestStr(key, "BASE_URL")) {
            writer->Write(goupil_config.http.base_url);
            return true;
        } else if (TestStr(key, "CACHE_KEY")) {
            writer->Write(goupil_config.database_filename ? etag : "");
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
        Fmt(etag, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }

    // Packed static assets
    for (const AssetInfo &asset: assets) {
        if (TestStr(asset.name, "goupil.html") || TestStr(asset.name, "sw.pk.js")) {
            AssetInfo asset2 = PatchGoupilVariables(asset, &assets_alloc);
            assets_map.Append(asset2);
        } else {
            assets_map.Append(asset);
        }
    }
}

static void AddCachingHeaders(http_IO *io)
{
#ifndef NDEBUG
    io->flags &= ~(unsigned int)http_IO::Flag::EnableCache;
#endif
    io->AddCachingHeaders(goupil_config.max_age, etag);
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
        // Handle server-side cache validation (ETag)
        {
            const char *client_etag = request.GetHeaderValue("If-None-Match");
            if (client_etag && TestStr(client_etag, etag)) {
                MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
                io->AttachResponse(304, response);
                return;
            }
        }

        // Try application files first
        {
            const FileEntry *file;
            if (TestStr(request.url, "/favicon.png")) {
                file = LockFile("/app/favicon.png");
            } else if (TestStr(request.url, "/manifest.json")) {
                file = LockFile("/app/manifest.json");
            } else {
                file = LockFile(request.url);
            }

            if (file) {
                io->RunAsync([=](const http_RequestInfo &request, http_IO *io) {
                    RG_DEFER { UnlockFile(file); };

                    HandleFileGet(request, *file, io);
                    io->flags |= (int)http_IO::Flag::EnableCache;

                    AddCachingHeaders(io);
                });
                return;
            }
        }

        // Now try static assets
        {
            const AssetInfo *asset = nullptr;

            if (TestStr(request.url, "/") || !strncmp(request.url, "/dev/", 5)) {
                asset = assets_map.Find("goupil.html");
            } else if (TestStr(request.url, "/favicon.png")) {
                asset = assets_map.Find("favicon.png");
            } else if (TestStr(request.url, "/sw.pk.js")) {
                asset = assets_map.Find("sw.pk.js");
            } else if (!strncmp(request.url, "/static/", 8)) {
                const char *asset_name = request.url + 8;
                asset = assets_map.Find(asset_name);
            }

            if (asset) {
                const char *mimetype = http_GetMimeType(GetPathExtension(asset->name));

                io->AttachBinary(200, asset->data, mimetype, asset->compression_type);
                io->flags |= (int)http_IO::Flag::EnableCache;

                AddCachingHeaders(io);
                if (asset->source_map) {
                    io->AddHeader("SourceMap", asset->source_map);
                }

                return;
            }
        }

        // And last (but not least), API endpoints
        {
            void (*func)(const http_RequestInfo &request, http_IO *io) = nullptr;

            if (TestStr(request.url, "/manifest.json")) {
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
                io->RunAsync([=](const http_RequestInfo &request, http_IO *io) {
                    (*func)(request, io);
                    AddCachingHeaders(io);
                });

                return;
            }
        }

        // Found nothing
        io->AttachError(404);
    } else if (TestStr(request.method, "PUT")) {
        io->RunAsync(HandleFilePut);
    } else if (TestStr(request.method, "DELETE")) {
        io->RunAsync(HandleFileDelete);
    } else {
        io->AttachError(405);
    }
}

int RunGoupil(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil [options]

Options:
    -C, --config_file <file>     Set configuration file

        --port <port>            Change web server port
                                 (default: %1)
        --base_url <url>         Change base URL
                                 (default: %2)

        --dev [<key>]            Run with fake profile and data)",
                goupil_config.http.port, goupil_config.http.base_url);
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

        if (!LoadConfig(config_filename, &goupil_config))
            return 1;

        if (!goupil_config.app_name) {
            goupil_config.app_name = goupil_config.app_key;
        }
    } else if (dev_key) {
        goupil_config.app_key = dev_key;
        goupil_config.app_name = Fmt(&temp_alloc, "goupil (%1)", dev_key).ptr;
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
                if (!ParseDec(opt.current_value, &goupil_config.http.port))
                    return 1;
            } else if (opt.Test("--base_url", OptionType::Value)) {
                goupil_config.http.base_url = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    // Check project configuration
    if (!goupil_config.app_key || !goupil_config.app_key[0]) {
        LogError("Project key must not be empty");
        return 1;
    }
    if (goupil_config.app_directory &&
            !TestFile(goupil_config.app_directory, FileType::Directory)) {
        LogError("Application directory '%1' does not exist", goupil_config.app_directory);
        return 1;
    }

    // Init database
    if (goupil_config.database_filename) {
        if (!goupil_db.Open(goupil_config.database_filename, SQLITE_OPEN_READWRITE))
            return 1;
    } else if (dev_key) {
        if (!goupil_db.Open(":memory:", SQLITE_OPEN_READWRITE))
            return 1;
        if (!goupil_db.CreateSchema())
            return 1;
        if (!goupil_db.InsertDemo())
            return 1;
    } else {
        LogError("Database file not specified");
        return 1;
    }

    // Init assets and files
#ifndef NDEBUG
    assets_filename = Fmt(&temp_alloc, "%1%/goupil_assets%2",
                          GetApplicationDirectory(), RG_SHARED_LIBRARY_EXTENSION).ptr;
    if (asset_set.LoadFromLibrary(assets_filename) == AssetLoadStatus::Error)
        return 1;
#endif
    InitAssets();
    if (goupil_config.app_directory && !InitFiles())
        return 1;

    // Run!
    http_Daemon daemon;
    if (!daemon.Start(goupil_config.http, HandleRequest))
        return 1;
    LogInfo("Listening on port %1 (%2 stack)",
            goupil_config.http.port, IPStackNames[(int)goupil_config.http.ip_stack]);

    // We need to send keep-alive notices to SSE clients
    while (!WaitForInterruption(goupil_config.sse_keep_alive)) {
        PushEvent(EventType::KeepAlive);
    }

    // Resume and disconnect SSE clients
    push_run = false;
    while (push_count > 0) {
        PushEvents(0);
        WaitForDelay(20);
    }

    LogInfo("Exit");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunGoupil(argc, argv); }
