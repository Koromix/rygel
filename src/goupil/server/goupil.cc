// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "config.hh"
#include "data.hh"
#include "goupil.hh"
#include "schedule.hh"
#include "../../wrappers/http.hh"
#include "../../felix/libpack/libpack.hh"

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
            pack_Asset asset;
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

Config goupil_config;
SQLiteDatabase goupil_db;

static Span<const pack_Asset> assets;
#ifndef NDEBUG
static const char *assets_filename;
static pack_AssetSet asset_set;
#else
extern "C" const Span<const pack_Asset> pack_assets;
#endif

static HashTable<const char *, Route> routes;
static BlockAllocator routes_alloc;

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
                                    const pack_Asset &asset) {
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
    for (const pack_Asset &asset: assets) {
        Span<const char> extension = GetPathExtension(asset.name);

        if (extension == ".html") {
            pack_Asset html = asset;
            html.data = pack_PatchVariables(html, &routes_alloc,
                                            [](const char *key, StreamWriter *writer) {
                if (TestStr(key, "BASE_URL")) {
                    writer->Write(goupil_config.base_url);
                    return true;
                } else {
                    return false;
                }
            });

            if (TestStr(asset.name, "goupil.html")) {
                add_asset_route("GET", "/", html);
            } else {
                Span<const char> name;
                SplitStrReverse(asset.name, '.', &name);

                const char *url = Fmt(&routes_alloc, "/%1/", name).ptr;
                add_asset_route("GET", url, html);

                const char *redirect_url = Fmt(&routes_alloc, "/%1", name).ptr;
                const char *redirect_location = Fmt(&routes_alloc, "%1%2/", goupil_config.base_url, name).ptr;
                add_redirect_route("GET", redirect_url, 301, redirect_location);
            }
        } else {
            const char *url = Fmt(&routes_alloc, "/static/%1", asset.name).ptr;
            add_asset_route("GET", url, asset);
        }
    }

    // Schedule API
    add_function_route("GET", "/schedule/resources.json", ProduceScheduleResources);
    add_function_route("GET", "/schedule/meetings.json", ProduceScheduleMeetings);
}

static int HandleRequest(const http_Request &request, http_Response *out_response)
{
#ifndef NDEBUG
    if (asset_set.LoadFromLibrary(assets_filename) == pack_LoadStatus::Loaded) {
        LogInfo("Reloaded assets from library");
        assets = asset_set.assets;

        InitRoutes();
    }
#endif

    Route *route = routes.Find(request.url);
    if (!route)
        return http_ProduceErrorPage(404, out_response);

    int code;
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
    if (asset_set.LoadFromLibrary(assets_filename) == pack_LoadStatus::Error)
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

    WaitForConsoleInterruption();

    LogInfo("Exit");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunGoupil(argc, argv); }
