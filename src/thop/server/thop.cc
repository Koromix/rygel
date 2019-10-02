// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
#include "../../libcc/libcc.hh"
#include "thop.hh"
#include "config.hh"
#include "structure.hh"
#include "mco.hh"
#include "mco_casemix.hh"
#include "mco_info.hh"
#include "user.hh"
#include "../../wrappers/http.hh"

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

    const char *method;
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

    RG_HASH_TABLE_HANDLER(Route, url);
};

Config thop_config;
bool thop_has_casemix;

StructureSet thop_structure_set;
UserSet thop_user_set;

static Span<const AssetInfo> assets;
#ifndef NDEBUG
static const char *assets_filename;
static AssetSet asset_set;
#else
extern "C" const Span<const AssetInfo> pack_assets;
#endif
static DictionarySet dictionary_set;

static HashTable<Span<const char>, Route> routes;
static BlockAllocator routes_alloc;
static char etag[64];

static Size ConvertToJsName(const char *name, Span<char> out_buf)
{
    // This is used for static strings (e.g. permission names), and the Span<char>
    // output buffer will abort debug builds on out-of-bounds access.

    if (name[0]) {
        out_buf[0] = LowerAscii(name[0]);

        Size j = 1;
        for (Size i = 1; name[i]; i++) {
            if (name[i] >= 'A' && name[i] <= 'Z') {
                out_buf[j++] = '_';
                out_buf[j++] = LowerAscii(name[i]);
            } else {
                out_buf[j++] = name[i];
            }
        }
        out_buf[j] = 0;

        return j;
    } else {
        out_buf[0] = 0;
        return 0;
    }
}

static void ProduceSettings(const http_RequestInfo &request, const User *user, http_IO *io)
{
    if (!user) {
        io->flags |= (int)http_IO::Flag::EnableCache;
    }

    http_JsonPageBuilder json(request.compression_type);
    char buf[32];

    json.StartObject();

    json.Key("permissions"); json.StartObject();
    {
        unsigned int permissions = user ? user->permissions : 0;
        for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
            char js_name[64];
            ConvertToJsName(UserPermissionNames[i], js_name);

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
                        json.String(part.ptr, part.len);
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
            if (!SpliceStream(&reader, Megabytes(8), &writer))
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

static void InitRoutes()
{
    LogInfo("Init routes");

    routes.Clear();
    routes_alloc.ReleaseAll();

    const auto add_asset_route = [&](const char *method, const char *url,
                                     Route::Matching matching, const AssetInfo &asset) {
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
                                        void (*func)(const http_RequestInfo &request, const User *user,
                                                     http_IO *io)) {
        Route route = {};

        route.method = method;
        route.url = url;
        route.matching = Route::Matching::Exact;
        route.type = Route::Type::Function;
        route.u.func = func;

        routes.Append(route);
    };

    // Static assets and dictionaries
    AssetInfo html = {};
    RG_ASSERT(assets.len > 0);
    for (const AssetInfo &asset: assets) {
        if (TestStr(asset.name, "thop.html")) {
            html = asset;
        } else if (TestStr(asset.name, "favicon.png")) {
            add_asset_route("GET", "/favicon.png", Route::Matching::Exact, asset);
        } else {
            const char *url = Fmt(&routes_alloc, "/static/%1", asset.name).ptr;
            add_asset_route("GET", url, Route::Matching::Exact, asset);
        }
    }
    for (const AssetInfo &desc: dictionary_set.dictionaries) {
        const char *url = Fmt(&routes_alloc, "/dictionaries/%1", desc.name).ptr;
        add_asset_route("GET", url, Route::Matching::Exact, desc);
    }
    RG_ASSERT(html.name);

    // Patch HTML
    html.data = PatchAssetVariables(html, &routes_alloc,
                                    [](const char *key, StreamWriter *writer) {
        if (TestStr(key, "VERSION")) {
            writer->Write(BuildVersion ? BuildVersion : "");
            return true;
        } else if (TestStr(key, "BASE_URL")) {
            writer->Write(thop_config.http.base_url);
            return true;
        } else if (TestStr(key, "HAS_USERS")) {
            writer->Write(thop_has_casemix ? "true" : "false");
            return true;
        } else {
            return false;
        }
    });

    // Root
    add_asset_route("GET", "/", Route::Matching::Exact, html);
    add_asset_route("GET", "/mco_info", Route::Matching::Walk, html);
    if (thop_has_casemix) {
        add_asset_route("GET", "/mco_casemix", Route::Matching::Walk, html);
        add_asset_route("GET", "/user", Route::Matching::Walk, html);
    }

    // Common API
    add_function_route("GET", "/api/settings.json", ProduceSettings);
    add_function_route("POST", "/api/login.json", HandleLogin);
    add_function_route("POST", "/api/logout.json", HandleLogout);
    add_function_route("GET", "/api/structures.json", ProduceStructures);

    // MCO API
    add_function_route("GET", "/api/mco_diagnoses.json", ProduceMcoDiagnoses);
    add_function_route("GET", "/api/mco_procedures.json", ProduceMcoProcedures);
    add_function_route("GET", "/api/mco_ghmghs.json", ProduceMcoGhmGhs);
    add_function_route("GET", "/api/mco_tree.json", ProduceMcoTree);
    add_function_route("GET", "/api/mco_aggregate.json", ProduceMcoAggregate);
    add_function_route("GET", "/api/mco_results.json", ProduceMcoResults);

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

    // Find user information
    const User *user = CheckSessionUser(request, io);

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

            if (!route) {
                io->AttachError(404);
                return;
            }
        }
    }

    // Execute route
    switch (route->type) {
        case Route::Type::Asset: {
            io->AttachBinary(route->u.st.asset.data, route->u.st.mime_type,
                             route->u.st.asset.compression_type);
            io->flags |= (int)http_IO::Flag::EnableCache;

#ifndef NDEBUG
            io->flags &= ~(unsigned int)http_IO::Flag::EnableCache;
#endif
            io->AddCachingHeaders(thop_config.max_age, etag);

            if (route->u.st.asset.source_map) {
                io->AddHeader("SourceMap", route->u.st.asset.source_map);
            }
        } break;

        case Route::Type::Function: {
            io->RunAsync([=](const http_RequestInfo &request, http_IO *io) {
                route->u.func(request, user, io);

#ifndef NDEBUG
                io->flags &= ~(unsigned int)http_IO::Flag::EnableCache;
#endif
                io->AddCachingHeaders(thop_config.max_age, etag);
            });
        } break;
    }
}

int RunThop(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    const auto print_usage = [](FILE *fp) {
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
                thop_config.http.port, thop_config.http.base_url);
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
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
                config_filename = opt.current_value;
            }
        }

        if (!config_filename) {
            const char *app_directory = GetApplicationDirectory();
            if (app_directory) {
                const char *test_filename = Fmt(&thop_config.str_alloc, "%1%/profile/thop.ini", app_directory).ptr;
                if (TestFile(test_filename, FileType::File)) {
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
                if (!ParseDec(opt.current_value, &thop_config.http.port))
                    return 1;
            } else if (opt.Test("--base_url", OptionType::Value)) {
                thop_config.http.base_url = opt.current_value;
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
    if (!InitDictionarySet(thop_config.table_directories))
        return 1;
    if (!InitMcoTables(thop_config.table_directories))
        return 1;
    if (thop_has_casemix) {
        if (!InitMcoStays(thop_config.mco_stay_directories, thop_config.mco_stay_filenames))
            return 1;
    }

    // Init routes
#ifndef NDEBUG
    assets_filename = Fmt(&temp_alloc, "%1%/thop_assets%2",
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
    if (!daemon.Start(thop_config.http, HandleRequest))
        return 1;
    LogInfo("Listening on port %1 (%2 stack)",
            thop_config.http.port, IPStackNames[(int)thop_config.http.ip_stack]);

    WaitForInterruption();

    LogInfo("Exit");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunThop(argc, argv); }
