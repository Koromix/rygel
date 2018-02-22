// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"
#include "../common/json.hh"
#ifndef _WIN32
    #include <signal.h>
#endif
#include "resources.hh"

GCC_PUSH_IGNORE(-Wconversion)
GCC_PUSH_IGNORE(-Wsign-conversion)
#include "../../lib/libmicrohttpd/src/include/microhttpd.h"
GCC_POP_IGNORE()
GCC_POP_IGNORE()

struct Page {
    const char *const category;
    const char *const url;
    const char *const name;
};

static const Page pages[] = {
    {"Tarifs",   "/pricing/table",     "Table"},
    {"Tarifs",   "/pricing/chart",     "Graphique"},
    {"Listes",   "/lists/ghm_tree",    "Arbre de groupage"},
    {"Listes",   "/lists/ghm_roots",   "Racines de GHM"},
    {"Listes",   "/lists/ghs",         "GHS"},
    {"Listes",   "/lists/diagnoses",   "Diagnostics"},
    {"Listes",   "/lists/exclusions",  "Exclusions"},
    {"Listes",   "/lists/procedures",  "Actes"}
};

static const TableSet *table_set;
static HeapArray<HashTable<GhmCode, GhmConstraint>> constraints_set;
static HeapArray<HashTable<GhmCode, GhmConstraint> *> index_to_constraints;
static const CatalogSet *catalog_set;

#if !defined(NDEBUG) && defined(_WIN32)
static HeapArray<Resource> static_resources;
static LinkedAllocator static_alloc;
#else
extern const Span<const Resource> static_resources;
#endif

static HashMap<const char *, Span<const uint8_t>> routes;

static void InitRoutes()
{
    Assert(static_resources.len > 0);
    routes.Set("/", static_resources[0].data);
    for (const Page &page: pages) {
        routes.Set(page.url, static_resources[0].data);
    }

    for (const Resource &res: static_resources) {
        routes.Set(res.url, res.data);
    }
}

#if !defined(NDEBUG) && defined(_WIN32)
static bool UpdateStaticResources()
{
    LinkedAllocator temp_alloc;

    Assert(GetApplicationDirectory());
    const char *filename = Fmt(&temp_alloc, "%1%/drdw_res.dll", GetApplicationDirectory()).ptr;
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

    const Span<const Resource> *dll_resources =
        (const Span<const Resource> *)GetProcAddress(h, "static_resources");
    if (!dll_resources) {
        LogError("Cannot find symbol 'static_resources' in library '%1'", filename);
        return false;
    }

    static_resources.Clear();
    static_alloc.ReleaseAll();
    for (const Resource &res: *dll_resources) {
        Resource new_res;
        new_res.url = DuplicateString(&static_alloc, res.url).ptr;
        uint8_t *data_ptr = (uint8_t *)Allocator::Allocate(&static_alloc, res.data.len);
        memcpy(data_ptr, res.data.ptr, (size_t)res.data.len);
        new_res.data = {data_ptr, res.data.len};
        static_resources.Append(new_res);
    }

    routes.Clear();
    InitRoutes();

    LogInfo("Loaded resources from '%1'", filename);
    return true;
}
#endif

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

struct Response {
    int code;
    MHD_Response *response;
};

static Response CreateErrorPage(int code)
{
    Span<char> page = Fmt(nullptr, "Error %1", code);
    MHD_Response *response = MHD_create_response_from_heap((size_t)page.len, page.ptr,
                                                           ReleaseCallback);
    return {code, response};
}

static MHD_Response *BuildJson(CompressionType compression_type,
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

static Response ProduceIndexes(MHD_Connection *, const char *, CompressionType compression_type)
{
    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        writer.StartArray();
        for (const TableIndex &index: table_set->indexes) {
            char buf[32];

            writer.StartObject();
            writer.Key("begin_date"); writer.String(Fmt(buf, "%1", index.limit_dates[0]).ptr);
            writer.Key("end_date"); writer.String(Fmt(buf, "%1", index.limit_dates[1]).ptr);
            if (index.changed_tables & ~MaskEnum(TableType::PriceTable)) {
                writer.Key("changed_tables"); writer.Bool(true);
            }
            if (index.changed_tables & MaskEnum(TableType::PriceTable)) {
                writer.Key("changed_prices"); writer.Bool(true);
            }
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    });

    return {200, response};
}

static Response ProducePriceMap(MHD_Connection *conn, const char *,
                                CompressionType compression_type)
{
    Date date = {};
    {
        const char *date_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "date");
        if (date_str) {
            date = Date::FromString(date_str);
        }
        if (!date.value)
            return CreateErrorPage(404);
    }

    const TableIndex *index = table_set->FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        return CreateErrorPage(404);
    }

    // Redirect to the canonical URL for this version, to improve client-side caching
    if (date != index->limit_dates[0]) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);

        {
            char url_buf[64];
            Fmt(url_buf, "price_map.json?date=%1", index->limit_dates[0]);
            MHD_add_response_header(response, "Location", url_buf);
        }

        return {303, response};
    }
    const HashTable<GhmCode, GhmConstraint> &constraints =
        *index_to_constraints[index - table_set->indexes.ptr];

    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[512];

        writer.StartArray();
        for (const GhmRootInfo &ghm_root_info: index->ghm_roots) {
            const GhmRootDesc *ghm_root_desc = catalog_set->ghm_roots_map.Find(ghm_root_info.ghm_root);

            writer.StartObject();
            writer.Key("ghm_root"); writer.String(Fmt(buf, "%1", ghm_root_info.ghm_root).ptr);
            if (ghm_root_desc) {
                writer.Key("ghm_root_desc"); writer.String(ghm_root_desc->ghm_root_desc);
            }
            writer.Key("ghs"); writer.StartArray();

            Span<const GhsAccessInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
            for (const GhsAccessInfo &ghs_access_info: compatible_ghs) {
                const GhsPriceInfo *ghs_price_info =
                    index->FindGhsPrice(ghs_access_info.Ghs(Sector::Public), Sector::Public);
                if (!ghs_price_info)
                    continue;

                const GhmConstraint *constraint = constraints.Find(ghs_access_info.ghm);
                if (!constraint)
                    continue;

                writer.StartObject();
                writer.Key("ghm"); writer.String(Fmt(buf, "%1", ghs_access_info.ghm).ptr);
                writer.Key("ghm_mode"); writer.String(&ghs_access_info.ghm.parts.mode, 1);
                {
                    uint32_t combined_duration_mask = constraint->duration_mask;
                    combined_duration_mask &= ~((1u << ghs_access_info.minimal_duration) - 1);
                    writer.Key("duration_mask"); writer.Uint(combined_duration_mask);
                }
                if (ghm_root_info.young_severity_limit) {
                    writer.Key("young_age_treshold"); writer.Int(ghm_root_info.young_age_treshold);
                    writer.Key("young_severity_limit"); writer.Int(ghm_root_info.young_severity_limit);
                }
                if (ghm_root_info.old_severity_limit) {
                    writer.Key("old_age_treshold"); writer.Int(ghm_root_info.old_age_treshold);
                    writer.Key("old_severity_limit"); writer.Int(ghm_root_info.old_severity_limit);
                }
                writer.Key("ghs"); writer.Int(ghs_price_info->ghs.number);

                writer.Key("conditions"); writer.StartArray();
                if (ghs_access_info.bed_authorization) {
                    writer.String(Fmt(buf, "Autorisation Lit %1", ghs_access_info.bed_authorization).ptr);
                }
                if (ghs_access_info.unit_authorization) {
                    writer.String(Fmt(buf, "Autorisation Unité %1", ghs_access_info.unit_authorization).ptr);
                    if (ghs_access_info.minimal_duration) {
                        writer.String(Fmt(buf, "Durée Unitée Autorisée ≥ %1", ghs_access_info.minimal_duration).ptr);
                    }
                } else if (ghs_access_info.minimal_duration) {
                    writer.String(Fmt(buf, "Durée ≥ %1", ghs_access_info.minimal_duration).ptr);
                }
                if (ghs_access_info.minimal_age) {
                    writer.String(Fmt(buf, "Age ≥ %1", ghs_access_info.minimal_age).ptr);
                }
                if (ghs_access_info.main_diagnosis_mask.value) {
                    writer.String(Fmt(buf, "DP de la liste D$%1.%2",
                                      ghs_access_info.main_diagnosis_mask.offset, ghs_access_info.main_diagnosis_mask.value).ptr);
                }
                if (ghs_access_info.diagnosis_mask.value) {
                    writer.String(Fmt(buf, "Diagnostic de la liste D$%1.%2",
                                      ghs_access_info.diagnosis_mask.offset, ghs_access_info.diagnosis_mask.value).ptr);
                }
                for (const ListMask &mask: ghs_access_info.procedure_masks) {
                    writer.String(Fmt(buf, "Acte de la liste A$%1.%2",
                                      mask.offset, mask.value).ptr);
                }
                writer.EndArray();

                writer.Key("price_cents"); writer.Int(ghs_price_info->price_cents);
                if (ghs_price_info->exh_treshold) {
                    writer.Key("exh_treshold"); writer.Int(ghs_price_info->exh_treshold);
                    writer.Key("exh_cents"); writer.Int(ghs_price_info->exh_cents);
                }
                if (ghs_price_info->exb_treshold) {
                    writer.Key("exb_treshold"); writer.Int(ghs_price_info->exb_treshold);
                    writer.Key("exb_cents"); writer.Int(ghs_price_info->exb_cents);
                    if (ghs_price_info->flags & (int)GhsPriceInfo::Flag::ExbOnce) {
                        writer.Key("exb_once"); writer.Bool(true);
                    }
                }

                writer.EndObject();
            }
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    });

    return {200, response};
}

static Response ProduceGhmRoots(MHD_Connection *, const char *,
                                CompressionType compression_type)
{
    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[32];

        writer.StartArray();
        for (const GhmRootDesc &desc: catalog_set->ghm_roots) {
            writer.StartObject();
            writer.Key("ghm_root"); writer.String(Fmt(buf, "%1", desc.ghm_root).ptr);
            writer.Key("ghm_root_desc"); writer.String(desc.ghm_root_desc);
            writer.Key("da"); writer.String(desc.da);
            writer.Key("da_desc"); writer.String(desc.da_desc);
            writer.Key("ga"); writer.String(desc.ga);
            writer.Key("ga_desc"); writer.String(desc.ga_desc);
            writer.EndObject();
        }
        writer.EndArray();

        return true;
    });

    return {200, response};
}

static Response ProducePages(MHD_Connection *, const char *,
                             CompressionType compression_type)
{
    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
       writer.StartArray();
       for (Size i = 0; i < ARRAY_SIZE(pages);) {
           writer.StartObject();
           writer.Key("category"); writer.String(pages[i].category);
           writer.Key("pages"); writer.StartArray();
           Size j = i;
           for (; j < ARRAY_SIZE(pages) && pages[j].category == pages[i].category; j++) {
               writer.StartObject();
               writer.Key("url"); writer.String(pages[j].url + 1);
               writer.Key("name"); writer.Key(pages[j].name);
               writer.EndObject();
           }
           i = j;
           writer.EndArray();
           writer.EndObject();
       }
       writer.EndArray();

       return true;
    });

    return {200, response};
}

static Response ProduceStaticResource(MHD_Connection *, const char *url,
                                      CompressionType compression_type)
{
#if !defined(NDEBUG) && defined(_WIN32)
    UpdateStaticResources();
#endif

    Span<const uint8_t> resource_data = routes.FindValue(url, {});
    if (!resource_data.IsValid())
        return CreateErrorPage(404);

    MHD_Response *response;
    if (compression_type != CompressionType::None) {
        HeapArray<uint8_t> buffer;
        StreamWriter st(&buffer, nullptr, compression_type);
        st.Write(resource_data);
        if (!st.Close())
            return CreateErrorPage(500);

        response = MHD_create_response_from_heap((size_t)buffer.len, (void *)buffer.ptr,
                                                 ReleaseCallback);
        buffer.Leak();

        AddContentEncodingHeader(response, compression_type);
    } else {
        response = MHD_create_response_from_buffer((size_t)resource_data.len,
                                                   (void *)resource_data.ptr,
                                                   MHD_RESPMEM_PERSISTENT);
    }

    return {200, response};
}

// TODO: Deny if URL too long (MHD option?)
static int HandleHttpConnection(void *, MHD_Connection *conn,
                                const char *url, const char *,
                                const char *, const char *,
                                size_t *, void **)
{
    CompressionType compression_type = CompressionType::None;
    {
        Span<const char> encodings = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "Accept-Encoding");
        while (encodings.len) {
            Span<const char> encoding = TrimStr(SplitStr(encodings, ',', &encodings));
            if (encoding == "gzip") {
                compression_type = CompressionType::Gzip;
                break;
            } else if (encoding == "deflate") {
                compression_type = CompressionType::Zlib;
                break;
            }
        }
    }

    Response response;
    if (TestStr(url, "/api/indexes.json")) {
        response = ProduceIndexes(conn, url, compression_type);
    } else if (TestStr(url, "/api/price_map.json")) {
        response = ProducePriceMap(conn, url, compression_type);
    } else if (TestStr(url, "/api/ghm_roots.json")) {
        response = ProduceGhmRoots(conn, url, compression_type);
    } else if (TestStr(url, "/api/pages.json")) {
        response = ProducePages(conn, url, compression_type);
    } else {
        response = ProduceStaticResource(conn, url, compression_type);
    }
    DEFER { MHD_destroy_response(response.response); };

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

)");
        PrintLn(fp, main_options_usage);
    };

    LinkedAllocator temp_alloc;

    // Add default data directory
    {
        const char *app_dir = GetApplicationDirectory();
        if (app_dir) {
            const char *default_data_dir = Fmt(&temp_alloc, "%1%/data", app_dir).ptr;
            main_data_directories.Append(default_data_dir);
        }
    }

    uint16_t port = 8888;
    {
        OptionParser opt_parser(argc, argv);

        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (TestOption(opt, "-p", "--port")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return 1;

                char *end_ptr;
                long new_port = strtol(opt_parser.current_value, &end_ptr, 10);
                if (end_ptr == opt_parser.current_value || end_ptr[0] ||
                        new_port < 0 || new_port >= 65536) {
                    LogError("Option '--port' requires a value between 0 and 65535");
                    return 1;
                }
                port = (uint16_t)new_port;
            } else if (!HandleMainOption(opt_parser, PrintUsage)) {
                return 1;
            }
        }
    }

    table_set = GetMainTableSet();
    if (!table_set || !table_set->indexes.len)
        return 1;
    catalog_set = GetMainCatalogSet();
    if (!catalog_set || !catalog_set->ghm_roots.len)
        return 1;

    for (Size i = 0; i < table_set->indexes.len; i++) {
        LogInfo("Computing constraints %1 / %2", i + 1, table_set->indexes.len);

        // Extend or remove this check when constraints go beyond the tree info (diagnoses, etc.)
        if (table_set->indexes[i].changed_tables & MaskEnum(TableType::GhmDecisionTree)) {
            HashTable<GhmCode, GhmConstraint> *constraints = constraints_set.AppendDefault();
            if (!ComputeGhmConstraints(table_set->indexes[i], constraints))
                return 1;
        }

        index_to_constraints.Append(&constraints_set[constraints_set.len - 1]);
    }

#if !defined(NDEBUG) && defined(_WIN32)
    if (!UpdateStaticResources())
        return false;
#endif
    InitRoutes();

    MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_AUTO_INTERNAL_THREAD | MHD_USE_ERROR_LOG, port,
        nullptr, nullptr, HandleHttpConnection, nullptr,
        MHD_OPTION_CONNECTION_MEMORY_LIMIT, Megabytes(1), MHD_OPTION_END);
    if (!daemon)
        return 1;
    DEFER { MHD_stop_daemon(daemon); };

#ifdef _WIN32
    (void)getchar();
#else
    {
        static volatile bool run = true;
        const auto do_exit = [](int) {
            run = false;
        };
        signal(SIGINT, do_exit);
        signal(SIGTERM, do_exit);

        while (run) {
            pause();
        }
    }
#endif

    return 0;
}
