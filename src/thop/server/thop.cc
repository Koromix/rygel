// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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
#include "thop.hh"
#include "config.hh"
#include "structure.hh"
#include "mco.hh"
#include "mco_casemix.hh"
#include "mco_info.hh"
#include "user.hh"
#include "src/core/http/http.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

struct DictionarySet {
    HeapArray<AssetInfo> dictionaries;
    BlockAllocator alloc;
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

    http_RequestMethod method;
    Span<const char> url;
    Matching matching;

    Type type;
    union {
        struct {
            AssetInfo asset;
            const char *mimetype;
        } st;

        void (*func)(http_IO *io, const User *user);
    } u;

    K_HASHTABLE_HANDLER(Route, url);
};

Config thop_config;
bool thop_has_casemix;

StructureSet thop_structure_set;
UserSet thop_user_set;

char thop_etag[17];

static DictionarySet dictionary_set;
static HashTable<Span<const char>, Route> routes;
static BlockAllocator routes_alloc;

static void ProduceSettings(http_IO *io, const User *user)
{
    if (!user) {
        io->AddCachingHeaders(thop_config.max_age, thop_etag);
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        char buf[128];

        json->StartObject();

        if (user) {
            json->Key("username"); json->String(user->name);
        }

        json->Key("permissions"); json->StartObject();
        {
            unsigned int permissions = user ? user->permissions : 0;

            for (Size i = 0; i < K_LEN(UserPermissionNames); i++) {
                Span<const char> key = json_ConvertToJsonName(UserPermissionNames[i], buf);
                json->Key(key.ptr, (size_t)key.len); json->Bool(permissions & (1 << i));
            }
        }
        json->EndObject();

        json->Key("mco"); json->StartObject();
        {
            json->Key("versions"); json->StartArray();
            for (const mco_TableIndex &index: mco_table_set.indexes) {
                if (!index.valid)
                    continue;

                json->StartObject();
                json->Key("begin_date"); json->String(Fmt(buf, "%1", index.limit_dates[0]).ptr);
                json->Key("end_date"); json->String(Fmt(buf, "%1", index.limit_dates[1]).ptr);
                if (index.changed_tables & ~MaskEnum(mco_TableType::PriceTablePublic)) {
                    json->Key("changed_tables"); json->Bool(true);
                }
                if (index.changed_tables & MaskEnum(mco_TableType::PriceTablePublic)) {
                    json->Key("changed_prices"); json->Bool(true);
                }
                json->EndObject();
            }
            json->EndArray();

            json->Key("casemix"); json->StartObject();
            if (user) {
                json->Key("min_date"); json->String(Fmt(buf, "%1", mco_stay_set_dates[0]).ptr);
                json->Key("max_date"); json->String(Fmt(buf, "%1", mco_stay_set_dates[1]).ptr);

                json->Key("algorithms"); json->StartArray();
                for (Size i = 0; i < K_LEN(mco_DispenseModeOptions); i++) {
                    if (user->CheckMcoDispenseMode((mco_DispenseMode)i)) {
                        const OptionDesc &desc = mco_DispenseModeOptions[i];
                        json->String(desc.name);
                    }
                }
                json->EndArray();

                const OptionDesc &default_desc = mco_DispenseModeOptions[(int)thop_config.mco_dispense_mode];
                json->Key("default_algorithm"); json->String(default_desc.name);
            }
            json->EndObject();
        }
        json->EndObject();

        json->EndObject();
    });
}

static void ProduceStructures(http_IO *io, const User *user)
{
    if (!user) {
        LogError("Not allowed to query structures");
        io->SendError(403);
        return;
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        for (const Structure &structure: thop_structure_set.structures) {
            json->StartObject();

            json->Key("name"); json->String(structure.name);
            json->Key("entities"); json->StartArray();
            for (const StructureEntity &ent: structure.entities) {
                if (user->mco_allowed_units.Find(ent.unit)) {
                    json->StartObject();
                    json->Key("unit"); json->Int(ent.unit.number);
                    json->Key("path"); json->StartArray();
                    {
                        Span<const char> path = ent.path + 1;
                        Span<const char> part;
                        while (path.len) {
                            part = SplitStr(path, '|', &path);
                            json->String(part.ptr, (int)part.len);
                        }
                    }
                    json->EndArray();
                    json->EndObject();
                }
            }
            json->EndArray();

            json->EndObject();
        }

        json->EndArray();
    });
}

static bool InitDictionarySet(Span<const char *const> table_directories)
{
    BlockAllocator temp_alloc;

    HeapArray<const char *> filenames;
    {
        bool success = true;
        for (const char *resource_dir: table_directories) {
            const char *desc_dir = Fmt(&temp_alloc, "%1%/dictionaries", resource_dir).ptr;
            if (TestFile(desc_dir, FileType::Directory)) {
                success &= EnumerateFiles(desc_dir, "*.json", 0, 1024, &temp_alloc, &filenames);
            }
        }
        if (!success)
            return false;
    }

    if (!filenames.len) {
        LogError("No dictionary file specified or found");
    }

    for (const char *filename: filenames) {
        AssetInfo dict = {};

        const char *name = SplitStrReverseAny(filename, K_PATH_SEPARATORS).ptr;
        K_ASSERT(name[0]);

        HeapArray<uint8_t> buf(&dictionary_set.alloc);
        {
            buf.Grow(Mebibytes(1));

            StreamReader reader(filename);
            StreamWriter writer(&buf, "<json>", 0, CompressionType::Gzip);
            if (!SpliceStream(&reader, Megabytes(16), &writer))
                return false;
            if (!writer.Close())
                return false;
        }

        dict.name = DuplicateString(name, &dictionary_set.alloc).ptr;
        dict.data = buf.Leak();
        dict.compression_type = CompressionType::Gzip;

        dictionary_set.dictionaries.Append(dict);
    }

    return true;
}

static void InitRoutes()
{
    LogInfo("Init routes");

    routes.Clear();
    routes_alloc.Reset();

    const auto add_asset_route = [&](http_RequestMethod method, const char *url,
                                     Route::Matching matching, const AssetInfo &asset) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.matching = matching;
        route.type = Route::Type::Asset;
        route.u.st.asset = asset;
        route.u.st.mimetype = GetMimeType(GetPathExtension(asset.name));

        routes.Set(route);
    };
    const auto add_function_route = [&](http_RequestMethod method, const char *url,
                                        void (*func)(http_IO *io, const User *user)) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.matching = Route::Matching::Exact;
        route.type = Route::Type::Function;
        route.u.func = func;

        routes.Set(route);
    };

    Span<const AssetInfo> assets = GetEmbedAssets();
    K_ASSERT(assets.len > 0);

    // We can use a global ETag because everything is in the binary
    {
        uint64_t buf;
        FillRandomSafe(&buf, K_SIZE(buf));
        Fmt(thop_etag, "%1", FmtHex(buf).Pad0(-16));
    }

    // Static assets and dictionaries
    AssetInfo html = {};
    for (const AssetInfo &asset: assets) {
        if (TestStr(asset.name, "src/thop/client/thop.html")) {
            html = asset;
        } else if (TestStr(asset.name, "src/thop/client/images/favicon.png")) {
            add_asset_route(http_RequestMethod::Get, "/favicon.png", Route::Matching::Exact, asset);
        } else {
            const char *basename = SplitStrReverseAny(asset.name, K_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&routes_alloc, "/static/%1", basename).ptr;

            add_asset_route(http_RequestMethod::Get, url, Route::Matching::Exact, asset);
        }
    }
    for (const AssetInfo &desc: dictionary_set.dictionaries) {
        const char *url = Fmt(&routes_alloc, "/dictionaries/%1", desc.name).ptr;
        add_asset_route(http_RequestMethod::Get, url, Route::Matching::Exact, desc);
    }
    K_ASSERT(html.name);

    // Patch HTML
    html.data = PatchFile(html, &routes_alloc, [](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "VERSION") {
            writer->Write(FelixVersion);
        } else if (key == "COMPILER") {
            writer->Write(FelixCompiler);
        } else if (key == "BASE_URL") {
            writer->Write(thop_config.base_url);
        } else if (key == "BUSTER") {
            writer->Write(thop_etag);
        } else if (key == "HAS_USERS") {
            writer->Write(thop_has_casemix ? "true" : "false");
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    // Root
    add_asset_route(http_RequestMethod::Get, "/", Route::Matching::Exact, html);
    add_asset_route(http_RequestMethod::Get, "/mco_info", Route::Matching::Walk, html);
    if (thop_has_casemix) {
        add_asset_route(http_RequestMethod::Get, "/mco_casemix", Route::Matching::Walk, html);
        add_asset_route(http_RequestMethod::Get, "/user", Route::Matching::Walk, html);
    }

    // Common API
    add_function_route(http_RequestMethod::Get, "/api/user/settings", ProduceSettings);
    add_function_route(http_RequestMethod::Post, "/api/user/login", HandleLogin);
    add_function_route(http_RequestMethod::Post, "/api/user/logout", HandleLogout);
    add_function_route(http_RequestMethod::Get, "/api/structures", ProduceStructures);

    // MCO information API
    add_function_route(http_RequestMethod::Get, "/api/mco/diagnoses", ProduceMcoDiagnoses);
    add_function_route(http_RequestMethod::Get, "/api/mco/procedures", ProduceMcoProcedures);
    add_function_route(http_RequestMethod::Get, "/api/mco/ghmghs", ProduceMcoGhmGhs);
    add_function_route(http_RequestMethod::Get, "/api/mco/tree", ProduceMcoTree);
    add_function_route(http_RequestMethod::Get, "/api/mco/highlight", ProduceMcoHighlight);

    // MCO casemix API
    add_function_route(http_RequestMethod::Get, "/api/mco/aggregate", ProduceMcoAggregate);
    add_function_route(http_RequestMethod::Get, "/api/mco/results", ProduceMcoResults);
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
            InitRoutes();
        }
    }
#endif

    if (thop_config.require_host) {
        const char *host = request.GetQueryValue("Host");

        if (!host) {
            LogError("Request is missing required Host header");
            io->SendError(400);
            return;
        }
        if (!TestStr(host, thop_config.require_host)) {
            LogError("Unexpected Host header '%1'", host);
            io->SendError(403);
            return;
        }
    }

    // Find user information
    const User *user = CheckSessionUser(io);

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");
    io->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    io->AddHeader("X-Robots-Tag", "noindex");
    io->AddHeader("Permissions-Policy", "interest-cohort=()");

    // Handle server-side cache validation (ETag)
    {
        const char *etag = request.GetQueryValue("If-None-Match");
        if (etag && TestStr(etag, thop_etag)) {
            io->SendEmpty(304);
            return;
        }
    }

    // Find appropriate route
    const Route *route;
    {
        Span<const char> url;
        {
            Size offset = 0;

            // Trim URL prefix (base_url setting)
            while (thop_config.base_url[offset]) {
                if (request.path[offset] != thop_config.base_url[offset]) {
                    if (!request.path[offset] && thop_config.base_url[offset] == '/' && !thop_config.base_url[offset + 1]) {
                        io->AddHeader("Location", thop_config.base_url);
                        io->SendEmpty(301);
                        return;
                    } else {
                        io->SendError(404);
                        return;
                    }
                }

                offset++;
            }

            url = request.path + offset - 1;
        }

        route = routes.Find(url);
        if (!route || route->method != request.method) {
            while (url.len > 1) {
                SplitStrReverse(url, '/', &url);

                const Route *walk_route = routes.Find(url);
                if (walk_route && walk_route->matching == Route::Matching::Walk &&
                                  walk_route->method == request.method) {
                    route = walk_route;
                    break;
                }
            }

            if (!route) {
                io->SendError(404);
                return;
            }
        }
    }

    // Execute route
    switch (route->type) {
        case Route::Type::Asset: {
            io->AddCachingHeaders(thop_config.max_age, thop_etag);
            io->SendAsset(200, route->u.st.asset.data, route->u.st.mimetype, route->u.st.asset.compression_type);
        } break;

        case Route::Type::Function: {
            // CSRF protection
            if (!http_PreventCSRF(io))
                return;

            route->u.func(io, user);
        } break;
    }
}

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "thop.ini";

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...] [stay_file...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: %2)%!0

        %!..+--profile_dir directory%!0    Set profile directory
        %!..+--table_dir directory%!0      Add table directory

        %!..+--mco_auth_file filename%!0   Set MCO authorization file
                                   %!D..(default: <profile_dir>%/mco_authorizations.ini
                                             <profile_dir>%/mco_authorizations.txt)%!0

    %!..+-p, --port port%!0                Change web server port
                                   %!D..(default: %3)%!0
        %!..+--bind IP%!0                  Bind to specific IP
        %!..+--base_url URL%!0             Change base URL
                                   %!D..(default: %4)%!0)",
                FelixTarget, config_filename, thop_config.http.port, thop_config.base_url);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn(T("Compiler: %1"), FelixCompiler);
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
                    config_filename = Fmt(&temp_alloc, "%1%/thop.ini", TrimStrRight(opt.current_value, K_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Load config file
    if (!LoadConfig(config_filename, &thop_config))
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
            } else if (opt.Test("-p", "--port", OptionType::Value)) {
                if (!thop_config.http.SetPortOrPath(opt.current_value))
                    return 1;
            } else if (opt.Test("--bind", OptionType::Value)) {
                thop_config.http.bind_addr = opt.current_value;
            } else if (opt.Test("--base_url", OptionType::Value)) {
                thop_config.base_url = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.ConsumeNonOptions(&thop_config.mco_stay_filenames);
        opt.LogUnusedArguments();

        // We may have changed some stuff (such as base_url), so revalidate
        if (!thop_config.Validate())
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
    if (!InitDictionarySet(thop_config.table_directories))
        return 1;
    if (!InitMcoTables(thop_config.table_directories))
        return 1;
    if (thop_has_casemix && !InitMcoStays(thop_config.mco_stay_directories, thop_config.mco_stay_filenames))
        return 1;

    // Init routes
    InitRoutes();

    // Run!
    LogInfo("Init HTTP server");
    http_Daemon daemon;
    if (!daemon.Bind(thop_config.http))
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
            WaitResult ret = WaitEvents(timeout);

            if (ret == WaitResult::Exit) {
                LogInfo("Exit requested");
                run = false;
            } else if (ret == WaitResult::Interrupt) {
                LogInfo("Process interrupted");
                status = 1;
                run = false;
            } else if (ret == WaitResult::Error) {
                status = 1;
                run = false;
            }

            LogDebug("Prune sessions");
            PruneSessions();
        }
    }

    LogInfo("Stop HTTP server");
    daemon.Stop();

    return status;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
