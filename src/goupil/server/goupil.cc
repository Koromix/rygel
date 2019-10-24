// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
#include "../../libcc/libcc.hh"
#include "config.hh"
#include "data.hh"
#include "goupil.hh"
#include "schedule.hh"
#include "../../wrappers/http.hh"

namespace RG {

struct Route {
    enum class Type {
        Asset,
        Redirect,
        Function
    };

    const char *method;
    const char *url;

    Type type;
    union {
        struct {
            AssetInfo asset;
            const char *mime_type;
        } st;
        struct {
            int code;
            const char *location;
        } redirect;
        void (*func)(const http_RequestInfo &request, http_IO *io);
    } u;

    RG_HASH_TABLE_HANDLER(Route, url);
};

struct PushContext {
    PushContext *next;

    std::mutex mutex;
    MHD_Connection *conn;

    unsigned int events;
};

Config goupil_config;
SQLiteDatabase goupil_db;

static Span<const AssetInfo> assets;
#ifndef NDEBUG
static const char *assets_filename;
static AssetSet asset_set;
#else
extern "C" const Span<const AssetInfo> pack_assets;
#endif

static HashTable<const char *, Route> routes;
static BlockAllocator routes_alloc;
static char etag[64];

static std::atomic_bool push_run = true;
static std::atomic<PushContext *> push_head;
static std::atomic_int push_count;

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

static void ProduceEvents(const http_RequestInfo &request, http_IO *io)
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

static void InitRoutes()
{
    LogInfo("Init routes");

    routes.Clear();
    routes_alloc.ReleaseAll();

    const auto add_asset_route = [](const char *method, const char *url,
                                    const AssetInfo &asset) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.type = Route::Type::Asset;
        route.u.st.asset = asset;
        route.u.st.mime_type = http_GetMimeType(GetPathExtension(asset.name));

        routes.Append(route);
    };
    const auto add_function_route = [&](const char *method, const char *url,
                                        void (*func)(const http_RequestInfo &request, http_IO *io)) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.type = Route::Type::Function;
        route.u.func = func;

        routes.Append(route);
    };

    // Static assets
    AssetInfo html = {};
    for (const AssetInfo &asset: assets) {
        if (TestStr(asset.name, "goupil.html")) {
            html = asset;
            html.data = PatchAssetVariables(html, &routes_alloc,
                                            [](const char *key, StreamWriter *writer) {
                if (TestStr(key, "VERSION")) {
                    writer->Write(BuildVersion);
                    return true;
                } else if (TestStr(key, "BASE_URL")) {
                    writer->Write(goupil_config.http.base_url);
                    return true;
                } else if (TestStr(key, "APP_KEY")) {
                    writer->Write(goupil_config.app_key);
                    return true;
                } else {
                    return false;
                }
            });
        } else {
            const char *url = Fmt(&routes_alloc, "/static/%1", asset.name).ptr;
            add_asset_route("GET", url, asset);
        }
    }
    RG_ASSERT(html.name);

    // Pages
    add_asset_route("GET", "/", html);

    // API
    add_function_route("GET", "/api/events.json", ProduceEvents);
    add_function_route("GET", "/api/schedule/resources.json", ProduceScheduleResources);
    add_function_route("GET", "/api/schedule/meetings.json", ProduceScheduleMeetings);

    // We can use a global ETag because everything is in the binary
    {
        uint64_t buf[2];
        randombytes_buf(&buf, RG_SIZE(buf));
        Fmt(etag, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
#ifndef NDEBUG
    if (asset_set.LoadFromLibrary(assets_filename) == AssetLoadStatus::Loaded) {
        LogInfo("Reloaded assets from library");
        assets = asset_set.assets;

        InitRoutes();
    }
#endif

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");

    // Handle server-side cache validation (ETag)
    {
        const char *client_etag = request.GetHeaderValue("If-None-Match");
        if (client_etag && TestStr(client_etag, etag)) {
            MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            io->AttachResponse(304, response);
            return;
        }
    }

    // Find appropriate route
    Route *route = routes.Find(request.url);
    if (!route || !TestStr(route->method, request.method)) {
        io->AttachError(404);
        return;
    }

    // Execute route
    switch (route->type) {
        case Route::Type::Asset: {
            io->AttachBinary(200, route->u.st.asset.data, route->u.st.mime_type,
                             route->u.st.asset.compression_type);
            io->flags |= (int)http_IO::Flag::EnableCache;

#ifndef NDEBUG
            io->flags &= ~(unsigned int)http_IO::Flag::EnableCache;
#endif
            io->AddCachingHeaders(goupil_config.max_age, etag);

            if (route->u.st.asset.source_map) {
                io->AddHeader("SourceMap", route->u.st.asset.source_map);
            }
        } break;

        case Route::Type::Redirect: {
            MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            io->AddHeader("Location", route->u.redirect.location);
            io->AttachResponse(301, response);

            // Avoid cache headers
            return;
        } break;

        case Route::Type::Function: {
            io->RunAsync([=](const http_RequestInfo &request, http_IO *io) {
                route->u.func(request, io);

#ifndef NDEBUG
                io->flags &= ~(unsigned int)http_IO::Flag::EnableCache;
#endif
                io->AddCachingHeaders(goupil_config.max_age, etag);
            });
        } break;
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
                                 (default: %1))
        --base_url <url>         Change base URL
                                 (default: %2))

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
    } else if (dev_key) {
        goupil_config.app_key = dev_key;
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

    // Init routes
#ifndef NDEBUG
    assets_filename = Fmt(&temp_alloc, "%1%/goupil_assets%2",
                          GetApplicationDirectory(), RG_SHARED_LIBRARY_EXTENSION).ptr;
    if (asset_set.LoadFromLibrary(assets_filename) == AssetLoadStatus::Error)
        return 1;
    assets = asset_set.assets;
#else
    assets = pack_assets;
#endif
    InitRoutes();

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
