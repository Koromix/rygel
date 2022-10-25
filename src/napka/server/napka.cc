// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "config.hh"
#include "src/core/libnet/libnet.hh"

namespace RG {

static Config napka_config;

static HashMap<const char *, const AssetInfo *> assets_map;
static HeapArray<const char *> assets_for_cache;
static LinkedAllocator assets_alloc;
static char shared_etag[17];

static AssetInfo *PatchVariables(const AssetInfo &asset)
{
    AssetInfo *copy = AllocateOne<AssetInfo>(&assets_alloc);

    *copy = asset;
    copy->data = PatchAsset(*copy, &assets_alloc, [](const char *key, StreamWriter *writer) {
        if (TestStr(key, "VERSION")) {
            writer->Write(FelixVersion);
        } else if (TestStr(key, "COMPILER")) {
            writer->Write(FelixCompiler);
        } else if (TestStr(key, "BASE_URL")) {
            writer->Write("/");
        } else {
            Print(writer, "{%1}", key);
        }
    });

    return copy;
}

static void InitAssets()
{
    assets_map.Clear();
    assets_for_cache.Clear();
    assets_alloc.ReleaseAll();

    // Update ETag
    {
        uint64_t buf;
        FillRandomSafe(&buf, RG_SIZE(buf));
        Fmt(shared_etag, "%1", FmtHex(buf).Pad0(-16));
    }

    for (const AssetInfo &asset: GetPackedAssets()) {
        if (TestStr(asset.name, "src/napka/client/napka.html")) {
            AssetInfo *copy = PatchVariables(asset);
            assets_map.Set("/", copy);
            assets_for_cache.Append("/");
        } else if (TestStr(asset.name, "src/napka/client/assets/favicon.png")) {
            assets_map.Set("/favicon.png", &asset);
            assets_for_cache.Append("/favicon.png");
        } else if (StartsWith(asset.name, "src/napka/client/") ||
                   StartsWith(asset.name, "vendor/")) {
            const char *name = SplitStrReverseAny(asset.name, RG_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&assets_alloc, "/static/%1", name).ptr;

            assets_map.Set(url, &asset);
            assets_for_cache.Append(url);
        }
    }
}

static void AttachStatic(const AssetInfo &asset, int max_age, const char *etag,
                         const http_RequestInfo &request, http_IO *io)
{
    const char *client_etag = request.GetHeaderValue("If-None-Match");

    if (client_etag && TestStr(client_etag, etag)) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        io->AttachResponse(304, response);
    } else {
        const char *mimetype = http_GetMimeType(GetPathExtension(asset.name));

        io->AttachBinary(200, asset.data, mimetype, asset.compression_type);
        io->AddCachingHeaders(max_age, etag);

        if (asset.source_map) {
            io->AddHeader("SourceMap", asset.source_map);
        }
    }
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
#ifdef FELIX_HOT_ASSETS
    // This is not actually thread safe, because it may release memory from an asset
    // that is being used by another thread. This code only runs in development builds
    // and it pretty much never goes wrong so it is kind of OK.
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        if (ReloadAssets()) {
            LogInfo("Reload assets");
            InitAssets();
        }
    }
#endif

    /*if (napka_config.require_host) {
        const char *host = request.GetHeaderValue("Host");

        if (!host) {
            LogError("Request is missing required Host header");
            io->AttachError(400);
            return;
        }
        if (!TestStr(host, napka_config.require_host)) {
            LogError("Unexpected Host header '%1'", host);
            io->AttachError(403);
            return;
        }
    }*/

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");
    io->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    io->AddHeader("X-Robots-Tag", "noindex");
    io->AddHeader("Permissions-Policy", "interest-cohort=()");

    // Try static assets first
    {
        const AssetInfo *asset = assets_map.FindValue(request.url, nullptr);

        if (asset) {
            AttachStatic(*asset, 0, shared_etag, request, io);
            return;
        };
    }

    io->AttachError(404);
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    // Options
    const char *config_filename = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %2)%!0)",
                FelixTarget, napka_config.http.port);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Find config filename
    {
        OptionParser opt(argc, argv, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Load config file
    if (config_filename && !LoadConfig(config_filename, &napka_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &napka_config.http.port))
                    return 1;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

    // Init assets
    LogInfo("Init assets");
    InitAssets();

    // Run!
    LogInfo("Init HTTP server");
    http_Daemon daemon;
    if (!daemon.Start(napka_config.http, HandleRequest))
        return 1;

#ifdef __linux__
    if (!NotifySystemd())
        return 1;
#endif

    // Run until exit
    if (WaitForInterrupt() == WaitForResult::Interrupt) {
        LogInfo("Exit requested");
    }
    LogDebug("Stop HTTP server");
    daemon.Stop();

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
