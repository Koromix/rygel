// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../lib/libsodium/src/libsodium/include/sodium.h"

#ifndef _WIN32
    #include <dlfcn.h>
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

#include "thop.hh"
#include "thop_mco.hh"
#include "user.hh"
#include "../../packer/packer.hh"

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
    enum class Flag {
        NoCache = 1 << 0
    };

    Span<const char> url;
    const char *method;
    Matching matching;

    Type type;
    union {
        struct {
            PackerAsset asset;
            const char *mime_type;
        } st;

        int (*func)(const ConnectionInfo *conn, const char *url, Response *out_response);
    } u;

    Route() = default;
    Route(const char *url, const char *method, Matching matching,
          const PackerAsset &asset, const char *mime_type)
        : url(url), method(method), matching(matching), type(Type::Static)
    {
        u.st.asset = asset;
        u.st.mime_type = mime_type;
    }
    Route(const char *url, const char *method, Matching matching,
          int (*func)(const ConnectionInfo *conn, const char *url, Response *out_response))
        : url(url), method(method), matching(matching), type(Type::Function)
    {
        u.func = func;
    }

    HASH_TABLE_HANDLER(Route, url);
};

const mco_TableSet *thop_table_set;
HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint>> thop_constraints_set;
HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint> *> thop_index_to_constraints;

const mco_AuthorizationSet *thop_authorization_set;
UserSet thop_user_set;
StructureSet thop_structure_set;
mco_StaySet thop_stay_set;
Date thop_stay_set_dates[2];
HeapArray<mco_Result> thop_results;
HeapArray<mco_Result> thop_mono_results;

static DescSet desc_set;
#ifndef NDEBUG
static HeapArray<PackerAsset> packer_assets;
static LinkedAllocator packer_alloc;
#else
extern const Span<const PackerAsset> packer_assets;
#endif

static HashTable<Span<const char>, Route> routes;
static BlockAllocator routes_alloc(Kibibytes(16));
static char etag[64];

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
    } else if (extension == ".svg") {
        return "image/svg+xml";
    } else {
        LogError("Unknown MIME type for path '%1'", path);
        return "application/octet-stream";
    }
}

static bool InitDescSet(Span<const char *const> resource_directories,
                        Span<const char *const> desc_directories,
                        DescSet *out_set)
{
    BlockAllocator temp_alloc(Kibibytes(8));

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

static bool InitUserSet(Span<const char *const> resource_directories,
                        const char *user_filename, UserSet *out_set)
{
    LogInfo("Loading users");

    BlockAllocator temp_alloc(Kibibytes(8));

    const char *filename = nullptr;
    {
        if (user_filename) {
            filename = user_filename;
        } else {
            for (Size i = resource_directories.len; i-- > 0;) {
                const char *test_filename = Fmt(&temp_alloc, "%1%/config/users.ini",
                                                resource_directories[i]).ptr;
                if (TestPath(test_filename, FileType::File)) {
                    filename = test_filename;
                    break;
                }
            }
        }
    }

    if (filename && filename[0]) {
        UserSetBuilder user_set_builder;
        if (!user_set_builder.LoadFiles(filename))
            return false;
        user_set_builder.Finish(out_set);
    } else {
        LogError("No users file specified or found");
    }

    return true;
}

static bool InitStructureSet(Span<const char *const> resource_directories,
                             const char *structure_filename, StructureSet *out_set)
{
    LogInfo("Loading structures");

    BlockAllocator temp_alloc(Kibibytes(8));

    const char *filename = nullptr;
    {
        if (structure_filename) {
            filename = structure_filename;
        } else {
            for (Size i = resource_directories.len; i-- > 0;) {
                const char *test_filename = Fmt(&temp_alloc, "%1%/config/structures.ini",
                                                resource_directories[i]).ptr;
                if (TestPath(test_filename, FileType::File)) {
                    filename = test_filename;
                    break;
                }
            }
        }
    }

    if (filename && filename[0]) {
        StructureSetBuilder structure_set_builder;
        if (!structure_set_builder.LoadFiles(filename))
            return false;
        structure_set_builder.Finish(out_set);
    } else {
        LogError("No structures file specified or found");
    }

    return true;
}

void AddContentEncodingHeader(MHD_Response *response, CompressionType compression_type)
{
    switch (compression_type) {
        case CompressionType::None: {} break;
        case CompressionType::Zlib:
            { MHD_add_response_header(response, "Content-Encoding", "deflate"); } break;
        case CompressionType::Gzip:
            { MHD_add_response_header(response, "Content-Encoding", "gzip"); } break;
    }
}

void AddCookieHeader(MHD_Response *response, const char *name, const char *value, bool http_only)
{
    char cookie_buf[512];
    if (value) {
        Fmt(cookie_buf, "%1=%2; Path=" THOP_BASE_URL ";%3", name, value,
            http_only ? " HttpOnly;" : "");
    } else {
        Fmt(cookie_buf, "%1=; Path=" THOP_BASE_URL "; Max-Age=0;", name);
    }

    MHD_add_response_header(response, "Set-Cookie", cookie_buf);
}

static void ReleaseCallback(void *ptr)
{
    Allocator::Release(nullptr, ptr, -1);
}

int CreateErrorPage(int code, Response *out_response)
{
    Span<char> page = Fmt((Allocator *)nullptr, "Error %1: %2", code,
                          MHD_get_reason_phrase_for((unsigned int)code));

    MHD_Response *response = MHD_create_response_from_heap((size_t)page.len, page.ptr,
                                                           ReleaseCallback);
    out_response->response.reset(response);

    MHD_add_response_header(response, "Content-Type", "text/plain");

    return code;
}

int BuildJson(std::function<bool(rapidjson::Writer<JsonStreamWriter> &)> func,
              CompressionType compression_type, Response *out_response)
{
    HeapArray<uint8_t> buf;
    {
        StreamWriter st(&buf, nullptr, compression_type);
        JsonStreamWriter json_stream(&st);
        rapidjson::Writer<JsonStreamWriter> writer(json_stream);

        if (!func(writer))
            return CreateErrorPage(500, out_response);
    }

    MHD_Response *response = MHD_create_response_from_heap((size_t)buf.len, buf.ptr,
                                                           ReleaseCallback);
    buf.Leak();
    out_response->response.reset(response);

    AddContentEncodingHeader(response, compression_type);
    MHD_add_response_header(response, "Content-Type", "application/json");

    return 200;
}

static int ProduceStaticAsset(const Route &route, CompressionType compression_type,
                              Response *out_response)
{
    DebugAssert(route.type == Route::Type::Static);

    MHD_Response *response;
    if (compression_type != route.u.st.asset.compression_type) {
        HeapArray<uint8_t> buf;
        {
            StreamReader reader(route.u.st.asset.data, nullptr, route.u.st.asset.compression_type);
            StreamWriter writer(&buf, nullptr, compression_type);
            if (!SpliceStream(&reader, Megabytes(8), &writer))
                return CreateErrorPage(500, out_response);
            if (!writer.Close())
                return CreateErrorPage(500, out_response);
        }

        response = MHD_create_response_from_heap((size_t)buf.len, (void *)buf.ptr, ReleaseCallback);
        buf.Leak();
    } else {
        response = MHD_create_response_from_buffer((size_t)route.u.st.asset.data.len,
                                                   (void *)route.u.st.asset.data.ptr,
                                                   MHD_RESPMEM_PERSISTENT);
    }
    out_response->response.reset(response);

    AddContentEncodingHeader(response, compression_type);
    if (route.u.st.mime_type) {
        MHD_add_response_header(response, "Content-Type", route.u.st.mime_type);
    }

    return 200;
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
        routes.Set({url, "GET", Route::Matching::Exact, asset, GetMimeType(asset.name)});
    }

    // Concepts
    for (const PackerAsset &desc: desc_set.descs) {
        const char *url = Fmt(&routes_alloc, "/concepts/%1", desc.name).ptr;
        routes.Set({url, "GET", Route::Matching::Exact, desc, GetMimeType(url)});
    }

    // Root
    Route html = *routes.Find("/static/thop.html");
    routes.Set({"/", "GET", Route::Matching::Exact, html.u.st.asset, html.u.st.mime_type});

    // User
    routes.Set({"/login", "GET", Route::Matching::Walk, html.u.st.asset, html.u.st.mime_type});
    routes.Set({"/api/connect.json", "POST", Route::Matching::Exact, HandleConnect});
    routes.Set({"/api/disconnect.json", "POST", Route::Matching::Exact, HandleDisconnect});

    // MCO
    routes.Set({"/mco_casemix", "GET", Route::Matching::Walk, html.u.st.asset, html.u.st.mime_type});
    routes.Set({"/mco_list", "GET", Route::Matching::Walk, html.u.st.asset, html.u.st.mime_type});
    routes.Set({"/mco_pricing", "GET", Route::Matching::Walk, html.u.st.asset, html.u.st.mime_type});
    routes.Set({"/mco_tree", "GET", Route::Matching::Walk, html.u.st.asset, html.u.st.mime_type});
    routes.Set({"/api/mco_settings.json", "GET", Route::Matching::Exact, ProduceMcoSettings});
    routes.Set({"/api/mco_casemix.json", "GET", Route::Matching::Exact, ProduceMcoCasemix});
    routes.Set({"/api/mco_indexes.json", "GET", Route::Matching::Exact, ProduceMcoIndexes});
    routes.Set({"/api/mco_diagnoses.json", "GET", Route::Matching::Exact, ProduceMcoDiagnoses});
    routes.Set({"/api/mco_procedures.json", "GET", Route::Matching::Exact, ProduceMcoProcedures});
    routes.Set({"/api/mco_ghm_ghs.json", "GET", Route::Matching::Exact, ProduceMcoGhmGhs});
    routes.Set({"/api/mco_tree.json", "GET", Route::Matching::Exact, ProduceMcoTree});

    // Special cases
    if (Route *favicon = routes.Find("/static/favicon.ico"); favicon) {
        routes.Set({"/favicon.ico", "GET", Route::Matching::Exact,
                    favicon->u.st.asset, favicon->u.st.mime_type});
    }
    routes.Remove("/static/thop.html");

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
    BlockAllocator temp_alloc(Kibibytes(8));

    const char *filename = nullptr;
    const Span<const PackerAsset> *lib_assets = nullptr;
#ifdef _WIN32
    Assert(GetApplicationDirectory());
    filename = Fmt(&temp_alloc, "%1%/thop_assets.dll", GetApplicationDirectory()).ptr;
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
    filename = Fmt(&temp_alloc, "%1%/thop_assets.so", GetApplicationDirectory()).ptr;
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

    LogInfo("Loaded assets from '%1'", SplitStrReverseAny(filename, PATH_SEPARATORS));
    return true;
}
#endif

static int HandleHttpConnection(void *, MHD_Connection *conn2, const char *url, const char *method,
                                const char *, const char *upload_data, size_t *upload_data_size,
                                void **con_cls)
{
#ifndef NDEBUG
    UpdateStaticAssets();
#endif

    ConnectionInfo *conn = (ConnectionInfo *)*con_cls;
    if (!conn) {
        conn = new ConnectionInfo;
        conn->conn = conn2;
        conn->user = CheckSessionUser(conn2);
        *con_cls = conn;
    }

    // Process POST data if any
    if (TestStr(method, "POST")) {
        if (!conn->pp) {
            conn->pp = MHD_create_post_processor(conn->conn, Kibibytes(32),
                                                 [](void *cls, enum MHD_ValueKind, const char *key,
                                                    const char *, const char *, const char *,
                                                    const char *data, uint64_t, size_t) {
                ConnectionInfo *conn = (ConnectionInfo *)cls;

                key = DuplicateString(&conn->temp_alloc, key).ptr;
                data = DuplicateString(&conn->temp_alloc, data).ptr;
                conn->post.Append(key, data);

                return MHD_YES;
            }, conn);
            if (!conn->pp) {
                Response response;
                CreateErrorPage(422, &response);
                return MHD_queue_response(conn->conn, 422, response.response.get());
            }

            return MHD_YES;
        } else if (*upload_data_size) {
            if (MHD_post_process(conn->pp, upload_data, *upload_data_size) != MHD_YES) {
                Response response;
                CreateErrorPage(422, &response);
                return MHD_queue_response(conn->conn, 422, response.response.get());
            }

            *upload_data_size = 0;
            return MHD_YES;
        }
    }

    // Negociate content encoding
    {
        uint32_t acceptable_encodings =
            ParseAcceptableEncodings(MHD_lookup_connection_value(conn->conn, MHD_HEADER_KIND, "Accept-Encoding"));

        if (acceptable_encodings & (1 << (int)CompressionType::Gzip)) {
            conn->compression_type = CompressionType::Gzip;
        } else if (acceptable_encodings) {
            conn->compression_type = (CompressionType)CountTrailingZeros(acceptable_encodings);
        } else {
            Response response;
            CreateErrorPage(406, &response);
            return MHD_queue_response(conn->conn, 406, response.response.get());
        }
    }

    // Find appropriate route
    Route *route;
    bool try_cache;
    {
        Span<const char> url2 = url;

        route = routes.Find(url2);
        if (!route || !TestStr(route->method, method)) {
            while (url2.len > 1) {
                SplitStrReverse(url2, '/', &url2);

                Route *walk_route = routes.Find(url2);
                if (walk_route && walk_route->matching == Route::Matching::Walk &&
                        TestStr(walk_route->method, method)) {
                    route = walk_route;
                    break;
                }
            }

            if (!route) {
                Response response;
                CreateErrorPage(404, &response);
                return MHD_queue_response(conn->conn, 404, response.response.get());
            }
        }

        try_cache = TestStr(method, "GET");
    }

    // Handle server-side cache validation (ETag)
    if (try_cache) {
        const char *client_etag = MHD_lookup_connection_value(conn->conn, MHD_HEADER_KIND, "If-None-Match");
        if (client_etag && TestStr(client_etag, etag)) {
            MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            DEFER { MHD_destroy_response(response); };
            return MHD_queue_response(conn->conn, 304, response);
        }
    }

    // Execute route
    int code = 0;
    Response response;
    switch (route->type) {
        case Route::Type::Static: {
            code = ProduceStaticAsset(*route, conn->compression_type, &response);
        } break;

        case Route::Type::Function: {
            code = route->u.func(conn, url, &response);
        } break;
    }
    DebugAssert(response.response);

    AddSessionHeaders(conn->conn, conn->user, response.response.get());

    // Add caching information
    if (try_cache) {
#ifndef NDEBUG
        response.flags |= (int)Response::Flag::DisableCacheControl;
#endif

        if (!(response.flags & (int)Response::Flag::DisableCacheControl)) {
            MHD_add_response_header(response.response.get(), "Cache-Control", "max-age=3600");
        } else {
            MHD_add_response_header(response.response.get(), "Cache-Control", "max-age=0");
        }

        if (etag[0] && !(response.flags & (int)Response::Flag::DisableETag)) {
            MHD_add_response_header(response.response.get(), "ETag", etag);
        }
    }

    return MHD_queue_response(conn->conn, (unsigned int)code, response.response.get());
}

static void ReleaseConnectionData(void *, struct MHD_Connection *,
                                  void **con_cls, enum MHD_RequestTerminationCode)
{
    ConnectionInfo *conn = (ConnectionInfo *)*con_cls;

    if (conn) {
        if (conn->pp) {
            MHD_destroy_post_processor(conn->pp);
        }
        delete conn;
    }
}

static bool InitTables(Span<const char *const> desc_directories,
                       Span<const char *const> stays_filenames)
{
    thop_table_set = mco_GetMainTableSet();
    if (!thop_table_set || !thop_table_set->indexes.len)
        return false;

    if (stays_filenames.len) {
        thop_authorization_set = mco_GetMainAuthorizationSet();
        if (!thop_authorization_set)
            return false;
        if (!InitUserSet(mco_resource_directories, nullptr, &thop_user_set))
            return false;
        if (!InitStructureSet(mco_resource_directories, nullptr, &thop_structure_set))
            return false;
    }

    if (!InitDescSet(mco_resource_directories, desc_directories, &desc_set))
        return false;

    return true;
}

static bool InitStays(Span<const char *const> stays_filenames)
{
    LogInfo("Stays");

    mco_StaySetBuilder stay_set_builder;
    if (!stay_set_builder.LoadFiles(stays_filenames))
        return false;
    if (!stay_set_builder.Finish(&thop_stay_set))
        return false;

    if (thop_stay_set.stays.len) {
        // Limit dates
        {
            Span<const mco_Stay> mono_stays = thop_stay_set.stays;

            Span<const mco_Stay> sub_stays = mco_Split(mono_stays, 1, &mono_stays);
            thop_stay_set_dates[0] = sub_stays[sub_stays.len - 1].exit.date;
            thop_stay_set_dates[1] = sub_stays[sub_stays.len - 1].exit.date;

            while (mono_stays.len) {
                sub_stays = mco_Split(mono_stays, 1, &mono_stays);
                thop_stay_set_dates[0] = std::min(thop_stay_set_dates[0], sub_stays[sub_stays.len - 1].exit.date);
                thop_stay_set_dates[1] = std::max(thop_stay_set_dates[1], sub_stays[sub_stays.len - 1].exit.date);
            }

            thop_stay_set_dates[1]++;
        }

        LogInfo("Classify");

        mco_Classify(*thop_table_set, *thop_authorization_set, thop_stay_set.stays,
                     (int)mco_ClassifyFlag::Mono, &thop_results, &thop_mono_results);
        thop_results.Trim();
        thop_mono_results.Trim();
    }

    return true;
}

static bool ComputeConstraints()
{
    LogInfo("Constraints");

    Async async;

    thop_constraints_set.Reserve(thop_table_set->indexes.len);
    for (Size i = 0; i < thop_table_set->indexes.len; i++) {
        if (thop_table_set->indexes[i].valid) {
            // Extend or remove this check when constraints go beyond the tree info (diagnoses, etc.)
            if (thop_table_set->indexes[i].changed_tables & MaskEnum(mco_TableType::GhmDecisionTree) ||
                    !thop_index_to_constraints[thop_index_to_constraints.len - 1]) {
                HashTable<mco_GhmCode, mco_GhmConstraint> *constraints = thop_constraints_set.AppendDefault();
                async.AddTask([=]() {
                    return mco_ComputeGhmConstraints(thop_table_set->indexes[i], constraints);
                });
            }
            thop_index_to_constraints.Append(&thop_constraints_set[thop_constraints_set.len - 1]);
        } else {
            thop_index_to_constraints.Append(nullptr);
        }
    }

    return async.Sync();
}

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: thop [options] [stay_file ..]

Options:
    -p, --port <port>            Web server port
                                 (default: 8888)
        --concept_dir <dir>      Add concepts directory
                                 (default: <resource_dir>%/concepts)

    -c, --casemix                Load stays for casemix module
)");
        PrintLn(fp, mco_options_usage);
    };

    BlockAllocator temp_alloc(Kibibytes(8));

    // Add default resource directory
    if (const char *app_dir = GetApplicationDirectory(); app_dir) {
        const char *default_resource_dir = Fmt(&temp_alloc, "%1%/resources", app_dir).ptr;
        mco_resource_directories.Append(default_resource_dir);
    }

    HeapArray<const char *> desc_directories;
    uint16_t port = 8888;
    HeapArray<const char *> stays_filenames;
    {
        OptionParser opt_parser(argc, argv);

        bool casemix = false;
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
            } else if (TestOption(opt, "-c", "--casemix")) {
                casemix = true;
            } else if (!mco_HandleMainOption(opt_parser, PrintUsage)) {
                return 1;
            }
        }

        if (casemix) {
            opt_parser.ConsumeNonOptions(&stays_filenames);
            if (!stays_filenames.len) {
                LogError("No stay filenames specified despite '--casemix' option");
                return 1;
            }
        }
    }

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }

    if (!InitTables(desc_directories, stays_filenames))
        return 1;
    if (stays_filenames.len && !InitStays(stays_filenames))
        return 1;
    if (!ComputeConstraints())
        return 1;

#ifndef NDEBUG
    if (!UpdateStaticAssets())
        return 1;
#else
    InitRoutes();
#endif

    MHD_Daemon *daemon;
    {
        int flags = MHD_USE_AUTO_INTERNAL_THREAD | MHD_USE_ERROR_LOG;
#ifndef NDEBUG
        flags |= MHD_USE_DEBUG;
#endif

        daemon = MHD_start_daemon(flags, port, nullptr, nullptr, HandleHttpConnection, nullptr,
                                  MHD_OPTION_NOTIFY_COMPLETED, ReleaseConnectionData, nullptr,
                                  MHD_OPTION_END);
        if (!daemon)
            return 1;
    }
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
