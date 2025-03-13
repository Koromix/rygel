// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "api.hh"
#include "config.hh"
#include "database.hh"
#include "ludivine.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

Config config;
sq_Database db;

static HashMap<const char *, const AssetInfo *> assets_map;
static AssetInfo assets_index;
static BlockAllocator assets_alloc;
static char shared_etag[17];

static bool NameContainsHash(Span<const char> name)
{
    const auto test_char = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };

    name = SplitStr(name, '.');

    Span<const char> prefix;
    Span<const char> hash = SplitStrReverse(name, '-', &prefix);

    if (!prefix.len || !hash.len)
        return false;
    if (!std::all_of(hash.begin(), hash.end(), test_char))
        return false;

    return true;
}

static void InitAssets()
{
    assets_map.Clear();
    assets_alloc.ReleaseAll();

    // Update ETag
    {
        uint64_t buf;
        FillRandomSafe(&buf, RG_SIZE(buf));
        Fmt(shared_etag, "%1", FmtHex(buf).Pad0(-16));
    }

    HeapArray<const char *> bundles;
    const char *js = nullptr;
    const char *css = nullptr;

    for (const AssetInfo &asset: GetEmbedAssets()) {
        if (TestStr(asset.name, "src/ludivine/client/index.html")) {
            assets_index = asset;
            assets_map.Set("/", &assets_index);
        } else if (TestStr(asset.name, "src/ludivine/assets/main/ldv.webp")) {
            assets_map.Set("/favicon.webp", &asset);
        } else {
            Span<const char> name = SplitStrReverseAny(asset.name, RG_PATH_SEPARATORS);

            if (NameContainsHash(name)) {
                const char *url = Fmt(&assets_alloc, "/static/%1", name).ptr;
                assets_map.Set(url, &asset);
            } else {
                const char *url = Fmt(&assets_alloc, "/static/%1/%2", shared_etag, name).ptr;
                assets_map.Set(url, &asset);

                if (name == "app.js") {
                    js = url;
                } else if (name == "app.css") {
                    css = url;
                } else {
                    bundles.Append(url);
                }
            }
        }
    }

    RG_ASSERT(js);
    RG_ASSERT(css);

    assets_index.data = PatchFile(assets_index, &assets_alloc, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "VERSION") {
            writer->Write(FelixVersion);
        } else if (key == "COMPILER") {
            writer->Write(FelixCompiler);
        } else if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "ENV") {
            json_Writer json(writer);

            json.StartObject();
            json.Key("title"); json.String(config.title);
            json.Key("url"); json.String(config.url);
            json.Key("pages"); json.StartArray();
            for (const Config::PageInfo &page: config.pages) {
                json.StartObject();
                json.Key("title"); json.String(page.title);
                json.Key("url"); json.String(page.url);
                json.EndObject();
            }
            json.EndArray();
            json.EndObject();
        } else if (key == "JS") {
            writer->Write(js);
        } else if (key == "CSS") {
            writer->Write(css);
        } else if (key == "BUNDLES") {
            json_Writer json(writer);

            json.StartObject();
            for (const char *bundle: bundles) {
                const char *name = SplitStrReverseAny(bundle, RG_PATH_SEPARATORS).ptr;
                json.Key(name); json.String(bundle);
            }
            json.EndObject();
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });
}

static void AttachStatic(http_IO *io, const AssetInfo &asset, int64_t max_age, const char *etag)
{
    const http_RequestInfo &request = io->Request();
    const char *client_etag = request.GetHeaderValue("If-None-Match");

    if (client_etag && TestStr(client_etag, etag)) {
        io->SendEmpty(304);
    } else {
        const char *mimetype = GetMimeType(GetPathExtension(asset.name));

        io->AddCachingHeaders(max_age, etag);
        io->SendAsset(200, asset.data, mimetype, asset.compression_type);
    }
}

static void HandleRequest(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

#if defined(FELIX_HOT_ASSETS)
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

    if (config.require_host) {
        const char *host = request.GetHeaderValue("Host");

        if (!host) {
            LogError("Request is missing required Host header");
            io->SendError(400);
            return;
        }
        if (!TestStr(host, config.require_host)) {
            LogError("Unexpected Host header '%1'", host);
            io->SendError(403);
            return;
        }
    }

    // CSRF protection
    if (request.method != http_RequestMethod::Get && !http_PreventCSRF(io))
        return;

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");
    io->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    io->AddHeader("Cross-Origin-Embedder-Policy", "require-corp");
    io->AddHeader("X-Robots-Tag", "noindex");
    io->AddHeader("Permissions-Policy", "interest-cohort=()");

    // API endpoint?
    if (StartsWith(request.path, "/api/")) {
        if (TestStr(request.path, "/api/register") && request.method == http_RequestMethod::Post) {
            HandleRegister(io);
        } else if (TestStr(request.path, "/api/login") && request.method == http_RequestMethod::Post) {
            HandleLogin(io);
        } else if (TestStr(request.path, "/api/download") && request.method == http_RequestMethod::Get) {
            HandleDownload(io);
        } else if (TestStr(request.path, "/api/upload") && request.method == http_RequestMethod::Put) {
            HandleUpload(io);
        } else if (TestStr(request.path, "/api/remind") && request.method == http_RequestMethod::Post) {
            HandleRemind(io);
        } else {
            io->SendError(404);
        }

        return;
    }

    // External static asset?
    if (config.static_directory) {
        const char *filename = nullptr;

        if (TestStr(request.path, "/")) {
            filename = Fmt(io->Allocator(), "%1/index.html", config.static_directory).ptr;
        } else {
            filename = Fmt(io->Allocator(), "%1%2", config.static_directory, request.path).ptr;
        }

        bool exists = TestFile(filename);

        if (!exists && !strchr(request.path + 1, '/') && !strchr(request.path + 1, '.')) {
            filename = Fmt(io->Allocator(), "%1.html", filename).ptr;
            exists = TestFile(filename);
        }

        if (exists) {
            Span<const char> extension = GetPathExtension(filename);
            const char *mimetype = GetMimeType(extension);

            io->SendFile(200, filename, mimetype);
            return;
        }
    }

    // Embedded static asset?
    {
        const char *path = request.path;
        const char *ext = GetPathExtension(path).ptr;

        if (!ext[0] || TestStr(ext, ".html")) {
            path = "/";
        }

        const AssetInfo *asset = assets_map.FindValue(path, nullptr);

        if (asset) {
            int64_t max_age = StartsWith(request.path, "/static/") ? (365ll * 86400000) : 0;
            AttachStatic(io, *asset, max_age, shared_etag);

            return;
        }
    }

    io->SendError(404);
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "ludivine.ini";

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: %2)%!0

    %!..+-p, --port port%!0                Change web server port
                                   %!D..(default: %3)%!0)",
                FelixTarget, config_filename, config.http.port);
    };

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }

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
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                if (IsDirectory(opt.current_value)) {
                    config_filename = Fmt(&temp_alloc, "%1%/ludivine.ini", TrimStrRight(opt.current_value, RG_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Load config file
    if (!LoadConfig(config_filename, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-p", "--port", OptionType::Value)) {
                if (!config.http.SetPortOrPath(opt.current_value))
                    return 1;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        if (!config.Validate())
            return 1;
    }

    LogInfo("Init data");
    if (!db.Open(config.database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return 1;
    if (!db.SetWAL(true))
        return false;
    if (!db.SetSynchronousFull(config.sync_full))
        return false;
    if (!MigrateDatabase(&db))
        return 1;
    if (!MakeDirectory(config.vault_directory, false))
        return 1;

    LogInfo("Init messaging");
    if (!InitSMTP(config.smtp))
        return 1;

    LogInfo("Init assets");
    InitAssets();

    // Run!
    LogInfo("Init HTTP server");
    http_Daemon daemon;
    if (!daemon.Bind(config.http))
        return 1;
    if (!daemon.Start(HandleRequest))
        return 1;

#if defined(__linux__)
    if (!NotifySystemd())
        return 1;
#endif

    // Run periodic tasks until exit
    int status = 0;
    {
        bool run = true;
        int timeout = 300 * 1000;

        while (run) {
            WaitForResult ret = WaitForInterrupt(timeout);

            if (ret == WaitForResult::Exit) {
                LogInfo("Exit requested");
                run = false;
            } else if (ret == WaitForResult::Interrupt) {
                LogInfo("Process interrupted");
                status = 1;
                run = false;
            }
        }
    }

    LogDebug("Stop HTTP server");
    daemon.Stop();

    return status;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
