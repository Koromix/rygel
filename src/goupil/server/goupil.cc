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
        int (*func)(const http_Request &request, http_Response *out_response);
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

static int ProduceEvents(const http_Request &request, http_Response *out_response)
{
    // TODO: Use the allocator buried in http_Request?
    PushContext *ctx = new PushContext();
    ctx->conn = request.conn;

    MHD_Response *response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 1024,
                                                               SendPendingEvents, ctx, FreePushContext);
    out_response->response.reset(response);
    MHD_add_response_header(*out_response, "Content-Type", "text/event-stream");

    push_count++;

    return 200;
}

static bool InitDatabase(const char *filename)
{
    Span<const char> extension = GetPathExtension(filename);

    if (extension == ".db") {
        return goupil_db.Open(filename, SQLITE_OPEN_READWRITE);
    } else if (extension == ".sql") {
        if (!goupil_db.Open(":memory:", SQLITE_OPEN_READWRITE))
            return false;
        if (!goupil_db.CreateSchema())
            return false;

        HeapArray<char> sql;
        if (!ReadFile(filename, Megabytes(1), &sql))
            return false;
        sql.Append(0);

        char *error = nullptr;
        if (sqlite3_exec(goupil_db, sql.ptr, nullptr, nullptr, &error) != SQLITE_OK) {
            LogError("SQLite request failed: %1", error);
            sqlite3_free(error);

            return false;
        }

        return true;
    } else {
        LogError("Unknown database extension '%1'", extension);
        return false;
    }
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
    const auto add_redirect_route = [](const char *method, const char *url, int code,
                                       const char *location) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.type = Route::Type::Redirect;
        route.u.redirect.code = code;
        route.u.redirect.location = location;

        routes.Append(route);
    };
    const auto add_function_route = [&](const char *method, const char *url,
                                        int (*func)(const http_Request &request, http_Response *out_response)) {
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
                if (TestStr(key, "BASE_URL")) {
                    writer->Write(goupil_config.base_url);
                    return true;
                } else if (TestStr(key, "PROJECT_KEY")) {
                    writer->Write(goupil_config.project_key);
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
    RG_ASSERT_DEBUG(html.name);

    // Main pages
    add_asset_route("GET", "/", html);
    add_redirect_route("GET", "/autoform", 301, "autoform/");
    add_asset_route("GET", "/autoform/", html);
    add_redirect_route("GET", "/schedule", 301, "schedule/");
    add_asset_route("GET", "/schedule/", html);

    // General API
    add_function_route("GET", "/goupil/events.json", ProduceEvents);

    // Schedule API
    add_function_route("GET", "/schedule/resources.json", ProduceScheduleResources);
    add_function_route("GET", "/schedule/meetings.json", ProduceScheduleMeetings);

    // We can use a global ETag because everything is in the binary
    {
        uint64_t buf[2];
        randombytes_buf(&buf, RG_SIZE(buf));
        Fmt(etag, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }
}

static int HandleRequest(const http_Request &request, http_Response *out_response)
{
#ifndef NDEBUG
    if (asset_set.LoadFromLibrary(assets_filename) == AssetLoadStatus::Loaded) {
        LogInfo("Reloaded assets from library");
        assets = asset_set.assets;

        InitRoutes();
    }
#endif

    // Send these headers whenever possible
    RG_DEFER { MHD_add_response_header(*out_response, "Referrer-Policy", "no-referrer"); };

    // Handle server-side cache validation (ETag)
    {
        const char *client_etag = request.GetHeaderValue("If-None-Match");
        if (client_etag && TestStr(client_etag, etag)) {
            *out_response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            return 304;
        }
    }

    // Find appropriate route
    Route *route = routes.Find(request.url);
    if (!route)
        return http_ProduceErrorPage(404, out_response);

    // Execute route
    int code = 0;
    switch (route->type) {
        case Route::Type::Asset: {
            code = http_ProduceStaticAsset(route->u.st.asset.data, route->u.st.asset.compression_type,
                                           route->u.st.mime_type, request.compression_type, out_response);
            if (route->u.st.asset.source_map) {
                MHD_add_response_header(*out_response, "SourceMap", route->u.st.asset.source_map);
            }
        } break;

        case Route::Type::Redirect: {
            MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            out_response->response.reset(response);

            MHD_add_response_header(response, "Location", route->u.redirect.location);

            return 301;
        } break;

        case Route::Type::Function: {
            code = route->u.func(request, out_response);
        } break;
    }
    RG_ASSERT_DEBUG(code);

    // Send cache information
#ifndef NDEBUG
    out_response->flags &= ~(unsigned int)http_Response::Flag::EnableCache;
#endif
    out_response->AddCachingHeaders(goupil_config.max_age, etag);

    return code;
}

int RunGoupil(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil [options]

Options:
    -C, --config_file <file>     Set configuration file

        --port <port>            Change web server port
                                 (default: %1))
        --base_url <url>         Change base URL
                                 (default: %2))",
                goupil_config.port, goupil_config.base_url);
    };

    // Find config filename
    const char *config_filename = nullptr;
    {
        OptionParser opt(argc, argv, (int)OptionParser::Flag::SkipNonOptions);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
                config_filename = opt.current_value;
            }
        }
    }

    // Load config file
    if (!config_filename) {
        LogError("Configuration file not specified");
        return 1;
    }
    if (!LoadConfig(config_filename, &goupil_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseDec(opt.current_value, &goupil_config.port))
                    return 1;
            } else if (opt.Test("--base_url", OptionType::Value)) {
                goupil_config.base_url = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    // Check project configuration
    if (!goupil_config.project_key || !goupil_config.project_key[0]) {
        LogError("Project key must not be empty");
        return 1;
    }

    // Init database
    if (!goupil_config.database_filename) {
        LogError("Database file not specified");
        return 1;
    }
    if (!InitDatabase(goupil_config.database_filename))
        return 1;

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
    daemon.handle_func = HandleRequest;
    if (!daemon.Start(goupil_config.ip_stack, goupil_config.port, goupil_config.threads,
                      goupil_config.base_url))
        return 1;
    LogInfo("Listening on port %1 (%2 stack)",
            goupil_config.port, IPStackNames[(int)goupil_config.ip_stack]);

    // We need to send keep-alive notices to SSE clients
    while (!WaitForConsoleInterruption(goupil_config.sse_keep_alive)) {
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
