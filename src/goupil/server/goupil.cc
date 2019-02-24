// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "config.hh"
#include "../../wrappers/http.hh"
#include "../../packer/libpacker.hh"

struct Route {
    const char *method;
    const char *url;

    pack_Asset asset;
    const char *mime_type;

    HASH_TABLE_HANDLER(Route, url);
};

static Config goupil_config;

static Span<const pack_Asset> assets;
#ifndef NDEBUG
static const char *assets_filename;
static pack_AssetSet asset_set;
#else
extern const Span<const pack_Asset> pack_assets;
#endif

static HashTable<const char *, Route> routes;
static BlockAllocator routes_alloc;

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

            if (var == "{GOUPIL_BASE_URL}") {
                writer.Write(goupil_config.base_url);
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

    const auto add_asset_route = [](const char *method, const char *url,
                                 const pack_Asset &asset) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.asset = asset;
        route.mime_type = http_GetMimeType(GetPathExtension(asset.name));

        routes.Append(route);
    };

    // Static assets
    pack_Asset html = {};
    for (const pack_Asset &asset: assets) {
        if (TestStr(asset.name, "goupil.html")) {
            html = asset;
        } else {
            const char *url = Fmt(&routes_alloc, "/static/%1", asset.name).ptr;
            add_asset_route("GET", url, asset);
        }
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
    add_asset_route("GET", "/", html);
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
    if (!route) {
        return http_ProduceErrorPage(404, out_response);
    }

    int code = http_ProduceStaticAsset(route->asset.data, route->asset.compression_type,
                                       route->mime_type, request.compression_type, out_response);
    if (route->asset.source_map) {
        MHD_add_response_header(*out_response, "SourceMap", route->asset.source_map);
    }

    return code;
}

int main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil [options]

Options:
    -C, --config_file <file>     Set configuration file
    -P, --profile_dir <dir>      Set profile directory

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
    if (config_filename && !LoadConfig(config_filename, &goupil_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-P", "--profile_dir", OptionType::Value)) {
                goupil_config.profile_directory = opt.current_value;
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

    // Configuration errors
    if (!goupil_config.profile_directory) {
        LogError("Profile directory is missing");
        return 1;
    }

    // Init routes
#ifndef NDEBUG
    assets_filename = Fmt(&temp_alloc, "%1%/goupil_assets%2",
                          GetApplicationDirectory(), SHARED_LIBRARY_EXTENSION).ptr;
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
