// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "drdw.hh"
#include "../packer/packer.hh"

#ifndef _WIN32
    #include <dlfcn.h>
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

struct DescSet {
    HeapArray<PackerAsset> descs;
    LinkedAllocator alloc;
};

struct Route {
    enum class Type {
        Static,
        Function
    };
    enum class Matching {
        Exact,
        Walk
    };

    Span<const char> url;
    Matching matching;

    Type type;
    union {
        struct {
            PackerAsset asset;
            const char *mime_type;
        } st;

        Response (*func)(MHD_Connection *conn, const char *url, CompressionType compression_type);
    } u;

    Route() = default;
    Route(const char *url, Matching matching, const PackerAsset &asset, const char *mime_type)
        : url(url), matching(matching), type(Type::Static)
    {
        u.st.asset = asset;
        u.st.mime_type = mime_type;
    }
    Route(const char *url, Matching matching,
          Response (*func)(MHD_Connection *conn, const char *url, CompressionType compression_type))
        : url(url), matching(matching), type(Type::Function)
    {
        u.func = func;
    }

    HASH_TABLE_HANDLER(Route, url);
};

const mco_TableSet *drdw_table_set;
HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint>> drdw_constraints_set;
HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint> *> drdw_index_to_constraints;
const mco_AuthorizationSet *drdw_authorization_set;
mco_StaySet drdw_stay_set;

static DescSet desc_set;
#ifndef NDEBUG
static HeapArray<PackerAsset> packer_assets;
static LinkedAllocator packer_alloc;
#else
extern const Span<const PackerAsset> packer_assets;
#endif

static HashTable<Span<const char>, Route> routes;
static LinkedAllocator routes_alloc;
static char etag[64];

const mco_TableIndex *GetIndexFromQueryString(MHD_Connection *conn, const char *redirect_url,
                                              Response *out_response)
{
    Date date = {};
    {
        const char *date_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "date");
        if (date_str) {
            date = Date::FromString(date_str);
        } else {
            LogError("Missing 'date' parameter");
        }
        if (!date.value) {
            *out_response = CreateErrorPage(422);
            return nullptr;
        }
    }

    const mco_TableIndex *index = drdw_table_set->FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        *out_response = CreateErrorPage(404);
        return nullptr;
    }

    // Redirect to the canonical URL for this version, to improve client-side caching
    if (redirect_url && date != index->limit_dates[0]) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);

        {
            char url_buf[64];
            Fmt(url_buf, "%1?date=%2", redirect_url, index->limit_dates[0]);
            MHD_add_response_header(response, "Location", url_buf);
        }

        *out_response = {303, response};
        return nullptr;
    }

    return index;
}

static const char *GetMimeType(Span<const char> path)
{
    Span<const char> extension = GetPathExtension(path);

    if (extension == ".css") {
        return "text/css";
    } else if (extension == ".html") {
        return "text/html";
    } else if (extension == ".ico") {
        return "image/vnd.microsoft.icon";
    } else if (extension == ".js") {
        return "application/javascript";
    } else if (extension == ".json") {
        return "application/json";
    } else if (extension == ".png") {
        return "image/png";
    } else {
        LogError("Unknown MIME type for path '%1'", path);
        return "application/octet-stream";
    }
}

static bool InitDescSet(Span<const char *const> resource_directories,
                        Span<const char *const> desc_directories,
                        DescSet *out_set)
{
    LinkedAllocator temp_alloc;

    HeapArray<const char *> filenames;
    {
        bool success = true;
        for (const char *resource_dir: resource_directories) {
            const char *desc_dir = Fmt(&temp_alloc, "%1%/concepts", resource_dir).ptr;
            if (TestPath(desc_dir, FileType::Directory)) {
                success &= EnumerateDirectoryFiles(desc_dir, "*.json", &temp_alloc, &filenames, 1024);
            }
        }
        for (const char *dir: desc_directories) {
            success &= EnumerateDirectoryFiles(dir, "*.json", &temp_alloc, &filenames, 1024);
        }
        if (!success)
            return false;
    }

    if (!filenames.len) {
        LogError("No desc file specified or found");
    }

    for (const char *filename: filenames) {
        PackerAsset desc = {};

        const char *name = SplitStrReverseAny(filename, PATH_SEPARATORS).ptr;
        Assert(name[0]);

        HeapArray<uint8_t> buf(&out_set->alloc);
        {
            StreamReader reader(filename);
            StreamWriter writer(&buf, nullptr, CompressionType::Gzip);
            if (!SpliceStream(&reader, Megabytes(8), &writer))
                return false;
            if (!writer.Close())
                return false;
        }

        desc.name = DuplicateString(&out_set->alloc, name).ptr;
        desc.data = buf.Leak();
        desc.compression_type = CompressionType::Gzip;

        out_set->descs.Append(desc);
    }

    return true;
}

static void ReleaseCallback(void *ptr)
{
    Allocator::Release(nullptr, ptr, -1);
}

static void AddContentEncodingHeader(MHD_Response *response, CompressionType compression_type)
{
    switch (compression_type) {
        case CompressionType::None: {} break;
        case CompressionType::Zlib:
            { MHD_add_response_header(response, "Content-Encoding", "deflate"); } break;
        case CompressionType::Gzip:
            { MHD_add_response_header(response, "Content-Encoding", "gzip"); } break;
    }
}

Response CreateErrorPage(int code)
{
    Span<char> page = Fmt((Allocator *)nullptr, "Error %1: %2", code,
                          MHD_get_reason_phrase_for((unsigned int)code));
    MHD_Response *response = MHD_create_response_from_heap((size_t)page.len, page.ptr,
                                                           ReleaseCallback);
    return {code, response};
}

MHD_Response *BuildJson(CompressionType compression_type,
                        std::function<bool(rapidjson::Writer<JsonStreamWriter> &)> func)
{
    HeapArray<uint8_t> buffer;
    {
        StreamWriter st(&buffer, nullptr, compression_type);
        JsonStreamWriter json_stream(&st);
        rapidjson::Writer<JsonStreamWriter> writer(json_stream);

        if (!func(writer))
            return nullptr;
    }

    MHD_Response *response = MHD_create_response_from_heap((size_t)buffer.len, buffer.ptr,
                                                           ReleaseCallback);
    buffer.Leak();

    MHD_add_response_header(response, "Content-Type", "application/json");
    AddContentEncodingHeader(response, compression_type);

    return response;
}

static Response ProduceStaticAsset(const Route &route, CompressionType compression_type)
{
    DebugAssert(route.type == Route::Type::Static);

    MHD_Response *response;
    if (compression_type != route.u.st.asset.compression_type) {
        HeapArray<uint8_t> buf;
        {
            StreamReader reader(route.u.st.asset.data, nullptr, route.u.st.asset.compression_type);
            StreamWriter writer(&buf, nullptr, compression_type);
            if (!SpliceStream(&reader, Megabytes(8), &writer))
                return CreateErrorPage(500);
            if (!writer.Close())
                return CreateErrorPage(500);
        }

        response = MHD_create_response_from_heap((size_t)buf.len, (void *)buf.ptr, ReleaseCallback);
        buf.Leak();
    } else {
        response = MHD_create_response_from_buffer((size_t)route.u.st.asset.data.len,
                                                   (void *)route.u.st.asset.data.ptr,
                                                   MHD_RESPMEM_PERSISTENT);
    }

    AddContentEncodingHeader(response, compression_type);
    if (route.u.st.mime_type) {
        MHD_add_response_header(response, "Content-Type", route.u.st.mime_type);
    }

    return {200, response};
}

// Mostly compliant, respects 'q=0' weights but it does not care about ordering beyond that. The
// caller is free to choose a preferred encoding among acceptable ones.
static uint32_t ParseAcceptableEncodings(Span<const char> encodings)
{
    encodings = TrimStr(encodings);

    uint32_t acceptable_encodings;
    if (encodings.len) {
        uint32_t low_priority = 1 << (int)CompressionType::None;
        uint32_t high_priority = 0;
        while (encodings.len) {
            Span<const char> quality;
            Span<const char> encoding = TrimStr(SplitStr(encodings, ',', &encodings));
            encoding = TrimStr(SplitStr(encoding, ';', &quality));
            quality = TrimStr(quality);

            if (encoding == "identity") {
                high_priority = ApplyMask(high_priority, 1u << (int)CompressionType::None, quality != "q=0");
                low_priority = ApplyMask(high_priority, 1u << (int)CompressionType::None, quality != "q=0");
            } else if (encoding == "gzip") {
                high_priority = ApplyMask(high_priority, 1u << (int)CompressionType::Gzip, quality != "q=0");
                low_priority = ApplyMask(low_priority, 1u << (int)CompressionType::Gzip, quality != "q=0");
            } else if (encoding == "deflate") {
                high_priority = ApplyMask(high_priority, 1u << (int)CompressionType::Zlib, quality != "q=0");
                low_priority = ApplyMask(low_priority, 1u << (int)CompressionType::Zlib, quality != "q=0");
            } else if (encoding == "*") {
                low_priority = ApplyMask(low_priority, UINT32_MAX, quality != "q=0");
            }
        }

        acceptable_encodings = high_priority | low_priority;
    } else {
        acceptable_encodings = UINT32_MAX;
    }

    return acceptable_encodings;
}

static void InitRoutes()
{
    routes.Clear();
    routes_alloc.ReleaseAll();

    // Static assets
    Assert(packer_assets.len > 0);
    for (const PackerAsset &asset: packer_assets) {
        const char *url = Fmt(&routes_alloc, "/static/%1", asset.name).ptr;
        routes.Set({url, Route::Matching::Exact, asset, GetMimeType(asset.name)});
    }

    // Special cases
    {
        Route *html = routes.Find("/static/drdw.html");
        Assert(html);
        routes.Set({"/", Route::Matching::Exact, html->u.st.asset, html->u.st.mime_type});
        routes.Set({"/pricing", Route::Matching::Walk, html->u.st.asset, html->u.st.mime_type});
        routes.Set({"/list", Route::Matching::Walk, html->u.st.asset, html->u.st.mime_type});
        routes.Set({"/casemix", Route::Matching::Walk, html->u.st.asset, html->u.st.mime_type});
        routes.Remove(html);

        Route *favicon = routes.Find("/static/favicon.ico");
        if (favicon) {
            routes.Set({"/favicon.ico", Route::Matching::Exact, favicon->u.st.asset,
                        favicon->u.st.mime_type});
            routes.Remove(favicon);
        }
    }

    // API
    routes.Set({"/api/indexes.json", Route::Matching::Exact, ProduceIndexes});
    routes.Set({"/api/casemix.json", Route::Matching::Exact, ProduceCaseMix});
    routes.Set({"/api/classifier_tree.json", Route::Matching::Exact, ProduceClassifierTree});
    routes.Set({"/api/diagnoses.json", Route::Matching::Exact, ProduceDiagnoses});
    routes.Set({"/api/procedures.json", Route::Matching::Exact, ProduceProcedures});
    routes.Set({"/api/ghm_ghs.json", Route::Matching::Exact, ProduceGhmGhs});
    for (const PackerAsset &desc: desc_set.descs) {
        const char *url = Fmt(&routes_alloc, "/concepts/%1", desc.name).ptr;
        routes.Set({url, Route::Matching::Exact, desc, GetMimeType(url)});
    }

    // We can use a global ETag because everything is in the binary
    {
        time_t now;
        time(&now);
        Fmt(etag, "%1", now);
    }
}

#ifndef NDEBUG
static bool UpdateStaticAssets()
{
    LinkedAllocator temp_alloc;

    const char *filename = nullptr;
    const Span<const PackerAsset> *lib_assets = nullptr;
#ifdef _WIN32
    Assert(GetApplicationDirectory());
    filename = Fmt(&temp_alloc, "%1%/drdw_assets.dll", GetApplicationDirectory()).ptr;
    {
        static FILETIME last_time;

        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (!GetFileAttributesEx(filename, GetFileExInfoStandard, &attr)) {
            LogError("Cannot stat file '%1'", filename);
            return false;
        }

        if (attr.ftLastWriteTime.dwHighDateTime == last_time.dwHighDateTime &&
                attr.ftLastWriteTime.dwLowDateTime == last_time.dwLowDateTime)
            return true;
        last_time = attr.ftLastWriteTime;
    }

    HMODULE h = LoadLibrary(filename);
    if (!h) {
        LogError("Cannot load library '%1'", filename);
        return false;
    }
    DEFER { FreeLibrary(h); };

    lib_assets = (const Span<const PackerAsset> *)GetProcAddress(h, "packer_assets");
#else
    Assert(GetApplicationDirectory());
    filename = Fmt(&temp_alloc, "%1%/drdw_assets.so", GetApplicationDirectory()).ptr;
    {
        static struct timespec last_time;

        struct stat sb;
        if (stat(filename, &sb) < 0) {
            LogError("Cannot stat file '%1'", filename);
            return false;
        }

#ifdef __APPLE__
        if (sb.st_mtimespec.tv_sec == last_time.tv_sec &&
                sb.st_mtimespec.tv_nsec == last_time.tv_nsec)
            return true;
        last_time = sb.st_mtimespec;
#else
        if (sb.st_mtim.tv_sec == last_time.tv_sec &&
                sb.st_mtim.tv_nsec == last_time.tv_nsec)
            return true;
        last_time = sb.st_mtim;
#endif
    }

    void *h = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    if (!h) {
        LogError("Cannot load library '%1': %2", filename, dlerror());
        return false;
    }
    DEFER { dlclose(h); };

    lib_assets = (const Span<const PackerAsset> *)dlsym(h, "packer_assets");
#endif
    if (!lib_assets) {
        LogError("Cannot find symbol 'packer_assets' in library '%1'", filename);
        return false;
    }

    packer_assets.Clear();
    packer_alloc.ReleaseAll();
    for (const PackerAsset &asset: *lib_assets) {
        PackerAsset asset_copy;
        asset_copy.name = DuplicateString(&packer_alloc, asset.name).ptr;
        uint8_t *data_ptr = (uint8_t *)Allocator::Allocate(&packer_alloc, asset.data.len);
        memcpy(data_ptr, asset.data.ptr, (size_t)asset.data.len);
        asset_copy.data = {data_ptr, asset.data.len};
        asset_copy.compression_type = asset.compression_type;
        packer_assets.Append(asset_copy);
    }

    InitRoutes();

    LogInfo("Loaded assets from '%1'", filename);
    return true;
}
#endif

static int HandleHttpConnection(void *, MHD_Connection *conn, const char *url, const char *,
                                const char *, const char *, size_t *, void **)
{
#ifndef NDEBUG
    UpdateStaticAssets();
#endif

    // Handle server-side cache validation (ETag)
    {
        const char *client_etag = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "If-None-Match");
        if (client_etag && TestStr(client_etag, etag)) {
            MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            DEFER { MHD_destroy_response(response); };

            return MHD_queue_response(conn, 304, response);
        }
    }

    // Negociate content encoding
    CompressionType compression_type;
    {
        uint32_t acceptable_encodings =
            ParseAcceptableEncodings(MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "Accept-Encoding"));

        if (acceptable_encodings & (1 << (int)CompressionType::Gzip)) {
            compression_type = CompressionType::Gzip;
        } else if (acceptable_encodings) {
            compression_type = (CompressionType)CountTrailingZeros(acceptable_encodings);
        } else {
            MHD_Response *response = CreateErrorPage(406).response;
            DEFER { MHD_destroy_response(response); };
            return MHD_queue_response(conn, 406, response);
        }
    }

    // Find appropriate route
    Route *route;
    {
        Span<const char> url2 = url;

        route = routes.Find(url2);
        if (!route) {
            while (url2.len > 1) {
                SplitStrReverse(url2, '/', &url2);

                Route *walk_route = routes.Find(url2);
                if (walk_route && walk_route->matching == Route::Matching::Walk) {
                    route = walk_route;
                    break;
                }
            }
            if (!route) {
                MHD_Response *response = CreateErrorPage(404).response;
                DEFER { MHD_destroy_response(response); };
                return MHD_queue_response(conn, 404, response);
            }
        }
    }

    // Execute route
    Response response = {};
    switch (route->type) {
        case Route::Type::Static: {
            response = ProduceStaticAsset(*route, compression_type);
        } break;

        case Route::Type::Function: {
            response = route->u.func(conn, url, compression_type);
        } break;
    }
    DebugAssert(response.response);
    DEFER { MHD_destroy_response(response.response); };

    // Add caching information
#ifdef NDEBUG
    MHD_add_response_header(response.response, "Cache-Control", "max-age=3600");
#else
    MHD_add_response_header(response.response, "Cache-Control", "max-age=0");
#endif
    if (etag[0]) {
        MHD_add_response_header(response.response, "ETag", etag);
    }

    return MHD_queue_response(conn, (unsigned int)response.code, response.response);
}

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdw [options]

Options:
    -p, --port <port>            Web server port
                                 (default: 8888)
        --concept_dir <dir>      Add concepts directory
                                 (default: <resource_dir>%/concepts)

    -s, --stays <path>           Add stays to casemix
)");
        PrintLn(fp, mco_options_usage);
    };

    LinkedAllocator temp_alloc;

    // Add default resource directory
    {
        const char *app_dir = GetApplicationDirectory();
        if (app_dir) {
            const char *default_resource_dir = Fmt(&temp_alloc, "%1%/resources", app_dir).ptr;
            mco_resource_directories.Append(default_resource_dir);
        }
    }

    HeapArray<const char *> desc_directories;
    uint16_t port = 8888;
    HeapArray<const char *> stays_filenames;
    {
        OptionParser opt_parser(argc, argv);

        const char *opt;
        while ((opt = opt_parser.Next())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (TestOption(opt, "-p", "--port")) {
                if (!opt_parser.RequireValue(PrintUsage))
                    return 1;

                char *end_ptr;
                long new_port = strtol(opt_parser.current_value, &end_ptr, 10);
                if (end_ptr == opt_parser.current_value || end_ptr[0] ||
                        new_port < 0 || new_port >= 65536) {
                    LogError("Option '--port' requires a value between 0 and 65535");
                    return 1;
                }
                port = (uint16_t)new_port;
            } else if (TestOption(opt, "--desc_dir")) {
                if (!opt_parser.RequireValue(PrintUsage))
                    return 1;

                desc_directories.Append(opt_parser.current_value);
            } else if (TestOption(opt, "-s", "--stays")) {
                if (!opt_parser.RequireValue(PrintUsage))
                    return 1;

                stays_filenames.Append(opt_parser.current_value);
            } else if (!mco_HandleMainOption(opt_parser, PrintUsage)) {
                return 1;
            }
        }
    }

    drdw_table_set = mco_GetMainTableSet();
    if (!drdw_table_set || !drdw_table_set->indexes.len)
        return 1;
    if (stays_filenames.len) {
        drdw_authorization_set = mco_GetMainAuthorizationSet();
        if (!drdw_authorization_set)
            return 1;
    }
    if (!InitDescSet(mco_resource_directories, desc_directories, &desc_set))
        return 1;

    if (stays_filenames.len) {
        LogInfo("Loading stays");

        mco_StaySetBuilder stay_set_builder;
        if (!stay_set_builder.LoadFiles(stays_filenames))
            return 1;
        if (!stay_set_builder.Finish(&drdw_stay_set))
            return 1;
    }

    LogInfo("Computing constraints");
    Async async;
    drdw_constraints_set.Reserve(drdw_table_set->indexes.len);
    for (Size i = 0; i < drdw_table_set->indexes.len; i++) {
        if (drdw_table_set->indexes[i].valid) {
            // Extend or remove this check when constraints go beyond the tree info (diagnoses, etc.)
            if (drdw_table_set->indexes[i].changed_tables & MaskEnum(mco_TableType::GhmDecisionTree) ||
                    !drdw_index_to_constraints[drdw_index_to_constraints.len - 1]) {
                HashTable<mco_GhmCode, mco_GhmConstraint> *constraints = drdw_constraints_set.AppendDefault();
                async.AddTask([=]() {
                    return mco_ComputeGhmConstraints(drdw_table_set->indexes[i], constraints);
                });
            }
            drdw_index_to_constraints.Append(&drdw_constraints_set[drdw_constraints_set.len - 1]);
        } else {
            drdw_index_to_constraints.Append(nullptr);
        }
    }
    if (!async.Sync())
        return 1;

#ifndef NDEBUG
    if (!UpdateStaticAssets())
        return 1;
#else
    InitRoutes();
#endif

    MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_AUTO_INTERNAL_THREAD | MHD_USE_ERROR_LOG, port,
        nullptr, nullptr, HandleHttpConnection, nullptr, MHD_OPTION_END);
    if (!daemon)
        return 1;
    DEFER { MHD_stop_daemon(daemon); };

    LogInfo("Listening on port %1", MHD_get_daemon_info(daemon, MHD_DAEMON_INFO_BIND_PORT)->port);
#ifdef _WIN32
    (void)getchar();
#else
    {
        static volatile bool run = true;

        signal(SIGINT, [](int) { run = false; });
        signal(SIGTERM, [](int) { run = false; });

        while (run) {
            pause();
        }
    }
#endif

    return 0;
}
