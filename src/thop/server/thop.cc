// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "thop.hh"
#include "config.hh"
#include "structure.hh"
#include "mco.hh"
#include "mco_casemix.hh"
#include "mco_info.hh"
#include "user.hh"
#include "../../web/libhttp/libhttp.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

struct DictionarySet {
    HeapArray<AssetInfo> dictionaries;
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

    http_RequestMethod method;
    Span<const char> url;
    Matching matching;

    Type type;
    union {
        struct {
            AssetInfo asset;
            const char *mime_type;
        } st;

        void (*func)(const http_RequestInfo &request, const User *user, http_IO *io);
    } u;

    RG_HASHTABLE_HANDLER(Route, url);
};

Config thop_config;
bool thop_has_casemix;

StructureSet thop_structure_set;
UserSet thop_user_set;

char thop_etag[33];

static DictionarySet dictionary_set;
static HashTable<Span<const char>, Route> routes;
static BlockAllocator routes_alloc;

static void ProduceSettings(const http_RequestInfo &request, const User *user, http_IO *io)
{
    http_JsonPageBuilder json(request.compression_type);
    char buf[32];

    json.StartObject();

    if (user) {
        json.Key("username"); json.String(user->name);
    }

    json.Key("permissions"); json.StartObject();
    {
        unsigned int permissions = user ? user->permissions : 0;
        for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
            char js_name[64];
            ConvertToJsonName(UserPermissionNames[i], js_name);

            json.Key(js_name); json.Bool(permissions & (1 << i));
        }
    }
    json.EndObject();

    json.Key("mco"); json.StartObject();
    {
        json.Key("versions"); json.StartArray();
        for (const mco_TableIndex &index: mco_table_set.indexes) {
            if (!index.valid)
                continue;

            char buf[32];

            json.StartObject();
            json.Key("begin_date"); json.String(Fmt(buf, "%1", index.limit_dates[0]).ptr);
            json.Key("end_date"); json.String(Fmt(buf, "%1", index.limit_dates[1]).ptr);
            if (index.changed_tables & ~MaskEnum(mco_TableType::PriceTablePublic)) {
                json.Key("changed_tables"); json.Bool(true);
            }
            if (index.changed_tables & MaskEnum(mco_TableType::PriceTablePublic)) {
                json.Key("changed_prices"); json.Bool(true);
            }
            json.EndObject();
        }
        json.EndArray();

        json.Key("casemix"); json.StartObject();
        if (user) {
            json.Key("min_date"); json.String(Fmt(buf, "%1", mco_stay_set_dates[0]).ptr);
            json.Key("max_date"); json.String(Fmt(buf, "%1", mco_stay_set_dates[1]).ptr);

            json.Key("algorithms"); json.StartArray();
            for (Size i = 0; i < RG_LEN(mco_DispenseModeOptions); i++) {
                if (user->CheckMcoDispenseMode((mco_DispenseMode)i)) {
                    const OptionDesc &desc = mco_DispenseModeOptions[i];
                    json.String(desc.name);
                }
            }
            json.EndArray();

            const OptionDesc &default_desc = mco_DispenseModeOptions[(int)thop_config.mco_dispense_mode];
            json.Key("default_algorithm"); json.String(default_desc.name);
        }
        json.EndObject();
    }
    json.EndObject();

    json.EndObject();

    if (!user) {
        io->AddCachingHeaders(thop_config.max_age, thop_etag);
    }
    json.Finish(io);
}

static void ProduceStructures(const http_RequestInfo &request, const User *user, http_IO *io)
{
    if (!user) {
        LogError("Not allowed to query structures");
        io->AttachError(403);
        return;
    }

    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    for (const Structure &structure: thop_structure_set.structures) {
        json.StartObject();
        json.Key("name"); json.String(structure.name);
        json.Key("entities"); json.StartArray();
        for (const StructureEntity &ent: structure.entities) {
            if (user->mco_allowed_units.Find(ent.unit)) {
                json.StartObject();
                json.Key("unit"); json.Int(ent.unit.number);
                json.Key("path"); json.StartArray();
                {
                    Span<const char> path = ent.path + 1;
                    Span<const char> part;
                    while (path.len) {
                        part = SplitStr(path, '|', &path);
                        json.String(part.ptr, (int)part.len);
                    }
                }
                json.EndArray();
                json.EndObject();
            }
        }
        json.EndArray();
        json.EndObject();
    }
    json.EndArray();

    json.Finish(io);
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

        const char *name = SplitStrReverseAny(filename, RG_PATH_SEPARATORS).ptr;
        RG_ASSERT(name[0]);

        HeapArray<uint8_t> buf(&dictionary_set.alloc);
        {
            StreamReader reader(filename);
            StreamWriter writer(&buf, nullptr, CompressionType::Gzip);
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
    routes_alloc.ReleaseAll();

    const auto add_asset_route = [&](http_RequestMethod method, const char *url,
                                     Route::Matching matching, const AssetInfo &asset) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.matching = matching;
        route.type = Route::Type::Asset;
        route.u.st.asset = asset;
        route.u.st.mime_type = http_GetMimeType(GetPathExtension(asset.name));

        routes.Set(route);
    };
    const auto add_function_route = [&](http_RequestMethod method, const char *url,
                                        void (*func)(const http_RequestInfo &request, const User *user,
                                                     http_IO *io)) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.matching = Route::Matching::Exact;
        route.type = Route::Type::Function;
        route.u.func = func;

        routes.Set(route);
    };

    Span<const AssetInfo> assets = GetPackedAssets();
    RG_ASSERT(assets.len > 0);

    // Static assets and dictionaries
    AssetInfo html = {};
    for (const AssetInfo &asset: assets) {
        if (TestStr(asset.name, "src/thop/client/thop.html")) {
            html = asset;
        } else if (TestStr(asset.name, "src/thop/client/images/favicon.png")) {
            add_asset_route(http_RequestMethod::Get, "/favicon.png", Route::Matching::Exact, asset);
        } else {
            const char *basename = SplitStrReverseAny(asset.name, RG_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&routes_alloc, "/static/%1", basename).ptr;

            add_asset_route(http_RequestMethod::Get, url, Route::Matching::Exact, asset);
        }
    }
    for (const AssetInfo &desc: dictionary_set.dictionaries) {
        const char *url = Fmt(&routes_alloc, "/dictionaries/%1", desc.name).ptr;
        add_asset_route(http_RequestMethod::Get, url, Route::Matching::Exact, desc);
    }
    RG_ASSERT(html.name);

    // Patch HTML
    html.data = PatchAsset(html, &routes_alloc, [](const char *key, StreamWriter *writer) {
        if (TestStr(key, "VERSION")) {
            writer->Write(FelixVersion);
        } else if (TestStr(key, "BASE_URL")) {
            writer->Write(thop_config.base_url);
        } else if (TestStr(key, "HAS_USERS")) {
            writer->Write(thop_has_casemix ? "true" : "false");
        } else {
            Print(writer, "{%1}", key);
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

    // We can use a global ETag because everything is in the binary
    {
        uint64_t buf[2];
        randombytes_buf(&buf, RG_SIZE(buf));
        Fmt(thop_etag, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
#ifndef NDEBUG
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        if (ReloadAssets()) {
            InitRoutes();
        }
    }
#endif

    // Find user information
    const User *user = CheckSessionUser(request, io);

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");

    // Handle server-side cache validation (ETag)
    {
        const char *etag = request.GetHeaderValue("If-None-Match");
        if (etag && TestStr(etag, thop_etag)) {
            MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            io->AttachResponse(304, response);
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
                if (request.url[offset] != thop_config.base_url[offset]) {
                    if (!request.url[offset] && thop_config.base_url[offset] == '/' && !thop_config.base_url[offset + 1]) {
                        io->AddHeader("Location", thop_config.base_url);
                        io->AttachNothing(301);
                        return;
                    } else {
                        io->AttachError(404);
                        return;
                    }
                }

                offset++;
            }

            url = request.url + offset - 1;
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
                io->AttachError(404);
                return;
            }
        }
    }

    // Execute route
    switch (route->type) {
        case Route::Type::Asset: {
            io->AttachBinary(200, route->u.st.asset.data, route->u.st.mime_type,
                             route->u.st.asset.compression_type);

            io->AddCachingHeaders(thop_config.max_age, thop_etag);
            if (route->u.st.asset.source_map) {
                io->AddHeader("SourceMap", route->u.st.asset.source_map);
            }
        } break;

        case Route::Type::Function: {
            route->u.func(request, user, io);
        } break;
    }
}

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "thop.ini";

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options] [stay_file ..]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

        %!..+--profile_dir <dir>%!0      Set profile directory
        %!..+--table_dir <dir>%!0        Add table directory

        %!..+--mco_auth_file <file>%!0   Set MCO authorization file
                                 %!D..(default: <profile_dir>%/mco_authorizations.ini
                                           <profile_dir>%/mco_authorizations.txt)%!0

        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %3)%!0
        %!..+--base_url <url>%!0         Change base URL
                                 %!D..(default: %4)%!0)",
                FelixTarget, config_filename, thop_config.http.port, thop_config.base_url);
    };

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %2", FelixTarget, FelixVersion);
        return 0;
    }

    // Find config filename
    {
        OptionParser opt(argc, argv, (int)OptionParser::Flag::SkipNonOptions);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
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
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &thop_config.http.port))
                    return 1;
            } else if (opt.Test("--base_url", OptionType::Value)) {
                thop_config.base_url = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        opt.ConsumeNonOptions(&thop_config.mco_stay_filenames);

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
    http_Daemon daemon;
    if (!daemon.Start(thop_config.http, HandleRequest))
        return 1;
#ifndef _WIN32
    if (thop_config.http.sock_type == SocketType::Unix) {
        LogInfo("Listening on socket '%1' (Unix stack)", thop_config.http.unix_path);
    } else
#endif
    LogInfo("Listening on port %1 (%2 stack)",
            thop_config.http.port, SocketTypeNames[(int)thop_config.http.sock_type]);

#ifdef __linux__
    if (!NotifySystemd())
        return 1;
#endif

    WaitForInterrupt();

    // Make sure the "Exit" message comes after the daemon has effectively stopped
    daemon.Stop();
    LogInfo("Exit");

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
