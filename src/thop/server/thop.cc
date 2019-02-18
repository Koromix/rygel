// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../lib/libsodium/src/libsodium/include/sodium.h"
#include "../../libcc/libcc.hh"
#include "thop.hh"
#include "config.hh"
#include "structure.hh"
#include "mco.hh"
#include "mco_casemix.hh"
#include "mco_info.hh"
#include "user.hh"
#include "../../wrappers/http.hh"
#include "../../packer/libasset.hh"

struct CatalogSet {
    HeapArray<asset_Asset> catalogs;
    LinkedAllocator alloc;
};

struct Route {
    enum class Matching {
        Exact,
        Walk
    };
    enum class Type {
        Asset,
        Function
    };

    const char *method;
    Span<const char> url;
    Matching matching;

    Type type;
    union {
        struct {
            asset_Asset asset;
            const char *mime_type;
        } st;

        int (*func)(const http_Request &request, const User *user, http_Response *out_response);
    } u;

    HASH_TABLE_HANDLER(Route, url);
};

Config thop_config;
bool thop_has_casemix;

StructureSet thop_structure_set;
UserSet thop_user_set;

static Span<const asset_Asset> assets;
#ifndef NDEBUG
static asset_AssetSet asset_set;
#else
extern const Span<const asset_Asset> packer_assets;
#endif
static CatalogSet catalog_set;

static HashTable<Span<const char>, Route> routes;
static BlockAllocator routes_alloc;
static char etag[64];

static bool InitCatalogSet(Span<const char *const> table_directories)
{
    BlockAllocator temp_alloc;

    HeapArray<const char *> filenames;
    {
        bool success = true;
        for (const char *resource_dir: table_directories) {
            const char *desc_dir = Fmt(&temp_alloc, "%1%/catalogs", resource_dir).ptr;
            if (TestPath(desc_dir, FileType::Directory)) {
                success &= EnumerateDirectoryFiles(desc_dir, "*.json", 1024, &temp_alloc, &filenames);
            }
        }
        if (!success)
            return false;
    }

    if (!filenames.len) {
        LogError("No catalog file specified or found");
    }

    for (const char *filename: filenames) {
        asset_Asset catalog = {};

        const char *name = SplitStrReverseAny(filename, PATH_SEPARATORS).ptr;
        Assert(name[0]);

        HeapArray<uint8_t> buf(&catalog_set.alloc);
        {
            StreamReader reader(filename);
            StreamWriter writer(&buf, nullptr, CompressionType::Gzip);
            if (!SpliceStream(&reader, Megabytes(8), &writer))
                return false;
            if (!writer.Close())
                return false;
        }

        catalog.name = DuplicateString(name, &catalog_set.alloc).ptr;
        catalog.data = buf.Leak();
        catalog.compression_type = CompressionType::Gzip;

        catalog_set.catalogs.Append(catalog);
    }

    return true;
}

static bool InitUsers(const char *profile_directory)
{
    BlockAllocator temp_alloc;

    LogInfo("Load users");

    if (const char *filename = Fmt(&temp_alloc, "%1%/users.ini", profile_directory).ptr;
            !LoadUserSet(filename, thop_structure_set, &thop_user_set))
        return false;

    // Everyone can use the default dispense mode
    for (User &user: thop_user_set.users) {
        user.mco_dispense_modes |= 1 << (int)thop_config.mco_dispense_mode;
    }

    return true;
}

static bool PatchTextFile(StreamReader &st, HeapArray<uint8_t> *out_buf)
{
    StreamWriter writer(out_buf, nullptr, st.compression.type);

    HeapArray<char> buf;
    if (st.ReadAll(Megabytes(1), &buf) < 0)
        return false;
    buf.Append(0);

    Span<const char> html = buf.Take(0, buf.len - 1);
    do {
        Span<const char> part = SplitStr(html, '$', &html);

        writer.Write(part);

        if (html.ptr[0] == '{') {
            Span<const char> var = MakeSpan(html.ptr,
                                            strspn(html.ptr + 1, "ABCDEFGHIJKLMNOPQRSTUVWXYZ_") + 2);

            if (var == "{THOP_BASE_URL}") {
                writer.Write(thop_config.base_url);
                html = html.Take(var.len, html.len - var.len);
            } else if (var == "{THOP_SHOW_USER}") {
                writer.Write(thop_has_casemix ? "true" : "false");
                html = html.Take(var.len, html.len - var.len);
            } else {
                writer.Write('$');
            }
        } else if (html.len) {
            writer.Write('$');
        }
    } while (html.len);

    return writer.Close();
}

static void InitRoutes()
{
    LogInfo("Init routes");

    routes.Clear();
    routes_alloc.ReleaseAll();

    const auto add_asset_route = [&](const char *method, const char *url,
                                     Route::Matching matching, const asset_Asset &asset) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.matching = matching;
        route.type = Route::Type::Asset;
        route.u.st.asset = asset;
        route.u.st.mime_type = http_GetMimeType(GetPathExtension(asset.name));

        routes.Append(route);
    };
    const auto add_function_route = [&](const char *method, const char *url,
                                   int (*func)(const http_Request &request, const User *user,
                                               http_Response *out_response)) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.matching = Route::Matching::Exact;
        route.type = Route::Type::Function;
        route.u.func = func;

        routes.Append(route);
    };

    // Static assets and catalogs
    asset_Asset html = {};
    DebugAssert(assets.len > 0);
    for (const asset_Asset &asset: assets) {
        if (TestStr(asset.name, "thop.html")) {
            html = asset;
        } else if (TestStr(asset.name, "favicon.png")) {
            add_asset_route("GET", "/favicon.png", Route::Matching::Exact, asset);
        } else {
            const char *url = Fmt(&routes_alloc, "/static/%1", asset.name).ptr;
            add_asset_route("GET", url, Route::Matching::Exact, asset);
        }
    }
    for (const asset_Asset &desc: catalog_set.catalogs) {
        const char *url = Fmt(&routes_alloc, "/catalogs/%1", desc.name).ptr;
        add_asset_route("GET", url, Route::Matching::Exact, desc);
    }
    DebugAssert(html.name);

    // Patch HTML
    {
        StreamReader st(html.data, nullptr, html.compression_type);
        HeapArray<uint8_t> buf(&routes_alloc);
        Assert(PatchTextFile(st, &buf));

        html.data = buf.Leak();
    }

    // Root
    add_asset_route("GET", "/", Route::Matching::Exact, html);
    add_asset_route("GET", "/login", Route::Matching::Walk, html);
    add_asset_route("GET", "/mco_casemix", Route::Matching::Walk, html);
    add_asset_route("GET", "/mco_list", Route::Matching::Walk, html);
    add_asset_route("GET", "/mco_pricing", Route::Matching::Walk, html);
    add_asset_route("GET", "/mco_results", Route::Matching::Walk, html);
    add_asset_route("GET", "/mco_tree", Route::Matching::Walk, html);

    // User API
    add_function_route("POST", "/api/connect.json", HandleConnect);
    add_function_route("POST", "/api/disconnect.json", HandleDisconnect);

    // MCO API
    add_function_route("GET", "/api/mco_aggregate.json", ProduceMcoAggregate);
    add_function_route("GET", "/api/mco_results.json", ProduceMcoResults);
    add_function_route("GET", "/api/mco_settings.json", ProduceMcoSettings);
    add_function_route("GET", "/api/mco_diagnoses.json", ProduceMcoDiagnoses);
    add_function_route("GET", "/api/mco_procedures.json", ProduceMcoProcedures);
    add_function_route("GET", "/api/mco_ghm_ghs.json", ProduceMcoGhmGhs);
    add_function_route("GET", "/api/mco_tree.json", ProduceMcoTree);

    // We can use a global ETag because everything is in the binary
    {
        uint64_t buf[2];
        randombytes_buf(&buf, SIZE(buf));
        Fmt(etag, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }
}

static int HandleRequest(const http_Request &request, http_Response *out_response)
{
#ifndef NDEBUG
    if (asset_set.LoadFromLibrary(GetApplicationDirectory(), "thop_assets") == asset_LoadStatus::Loaded) {
        LogInfo("Reloaded assets from library");
        assets = asset_set.assets;

        InitRoutes();
    }
#endif

    // Find user information
    bool user_mismatch = false;
    const User *user = CheckSessionUser(request, &user_mismatch);

    // Send these headers whenever possible
    DEFER {
        MHD_add_response_header(*out_response, "Referrer-Policy", "no-referrer");
        if (user_mismatch) {
            DeleteSessionCookies(out_response);
        }
    };

    // Handle server-side cache validation (ETag)
    if (!(out_response->flags & (int)http_Response::Flag::DisableETag)) {
        const char *client_etag = request.GetHeaderValue("If-None-Match");
        if (client_etag && TestStr(client_etag, etag)) {
            *out_response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            return 304;
        }
    }

    // Find appropriate route
    const Route *route;
    {
        Span<const char> url = request.url;

        route = routes.Find(url);
        if (!route || !TestStr(route->method, request.method)) {
            while (url.len > 1) {
                SplitStrReverse(url, '/', &url);

                const Route *walk_route = routes.Find(url);
                if (walk_route && walk_route->matching == Route::Matching::Walk &&
                        TestStr(walk_route->method, request.method)) {
                    route = walk_route;
                    break;
                }
            }

            if (!route)
                return http_ProduceErrorPage(404, out_response);
        }
    }

    // Execute route
    int code = 0;
    switch (route->type) {
        case Route::Type::Asset: {
            code = http_ProduceStaticAsset(route->u.st.asset.data, route->u.st.asset.compression_type,
                                           route->u.st.mime_type, request.compression_type, out_response);
        } break;

        case Route::Type::Function: {
            code = route->u.func(request, user, out_response);
        } break;
    }
    DebugAssert(code);

    // Send cache information
#ifndef NDEBUG
    out_response->flags |= (int)http_Response::Flag::DisableCache;
#endif
    out_response->AddCachingHeaders(thop_config.max_age, etag);

    return code;
}

int main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: thop [options] [stay_file ..]

Options:
    -C, --config_file <file>     Set configuration file
                                 (default: <executable_dir>%/profile%/thop.ini)

        --profile_dir <dir>      Set profile directory
        --table_dir <dir>        Add table directory

        --mco_auth_file <file>   Set MCO authorization file
                                 (default: <profile_dir>%/mco_authorizations.ini
                                           <profile_dir>%/mco_authorizations.txt)

        --port <port>            Change web server port
                                 (default: %1))
        --base_url <url>         Change base URL
                                 (default: %2))",
                thop_config.port, thop_config.base_url);
    };

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }

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

        if (!config_filename) {
            const char *app_directory = GetApplicationDirectory();
            if (app_directory) {
                const char *test_filename = Fmt(&thop_config.str_alloc, "%1%/profile/thop.ini", app_directory).ptr;
                if (TestPath(test_filename, FileType::File)) {
                    config_filename = test_filename;
                }
            }
        }
    }

    // Load config file
    if (config_filename && !LoadConfig(config_filename, &thop_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--profile_dir", OptionType::Value)) {
                thop_config.profile_directory = opt.current_value;
            } else if (opt.Test("--table_dir", OptionType::Value)) {
                thop_config.table_directories.Append(opt.current_value);
            } else if (opt.Test("--mco_auth_file", OptionType::Value)) {
                thop_config.mco_authorization_filename = opt.current_value;
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseDec(opt.current_value, &thop_config.port))
                    return 1;
            } else if (opt.Test("--base_url", OptionType::Value)) {
                thop_config.base_url = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        opt.ConsumeNonOptions(&thop_config.mco_stay_filenames);
    }

    // Configuration errors
    {
        bool valid = true;

        if (!thop_config.profile_directory) {
            LogError("Profile directory is missing");
            valid = false;
        }
        if (!thop_config.table_directories.len) {
            LogError("No table directory is specified");
            valid = false;
        }

        if (!valid)
            return 1;
    }

    // Do we have any site-specific (sensitive) data?
    thop_has_casemix = thop_config.mco_stay_directories.len || thop_config.mco_stay_filenames.len;

    // Init main data
    if (thop_has_casemix) {
        if (!InitMcoProfile(thop_config.profile_directory, thop_config.mco_authorization_filename))
            return 1;
        if (!InitUsers(thop_config.profile_directory))
            return 1;
    }
    if (!InitCatalogSet(thop_config.table_directories))
        return 1;
    if (!InitMcoTables(thop_config.table_directories))
        return 1;
    if (thop_has_casemix) {
        if (!InitMcoStays(thop_config.mco_stay_directories, thop_config.mco_stay_filenames))
            return 1;
    }

    // Init routes
#ifndef NDEBUG
    if (asset_set.LoadFromLibrary(GetApplicationDirectory(), "thop_assets") == asset_LoadStatus::Error)
        return 1;
    assets = asset_set.assets;
#else
    assets = packer_assets;
#endif
    InitRoutes();

    // Run!
    http_Daemon daemon;
    daemon.handle_func = HandleRequest;
    if (!daemon.Start(thop_config.ip_stack, thop_config.port, thop_config.threads,
                      thop_config.base_url))
        return 1;
    LogInfo("Listening on port %1 (%2 stack)",
            thop_config.port, IPStackNames[(int)thop_config.ip_stack]);

    WaitForConsoleInterruption();

    LogInfo("Exit");
    return 0;
}
