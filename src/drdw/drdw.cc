// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"
#include "../common/json.hh"
#ifndef _WIN32
    #include <dlfcn.h>
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif
#include "resources.hh"

GCC_PUSH_IGNORE(-Wconversion)
GCC_PUSH_IGNORE(-Wsign-conversion)
#include "../../lib/libmicrohttpd/src/include/microhttpd.h"
GCC_POP_IGNORE()
GCC_POP_IGNORE()

static const char *const pages[] = {
    "/pricing/table",
    "/pricing/chart",
    "/lists/ghm_tree",
    "/lists/ghm_roots",
    "/lists/ghs",
    "/lists/diagnoses",
    "/lists/exclusions",
    "/lists/procedures"
};

static const mco_TableSet *table_set;
static HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint>> constraints_set;
static HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint> *> index_to_constraints;
static const mco_CatalogSet *catalog_set;
static const mco_AuthorizationSet *authorization_set;
static mco_StaySet stay_set;

#ifndef NDEBUG
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
    for (const char *page: pages) {
        routes.Set(page, static_resources[0].data);
    }

    for (const Resource &res: static_resources) {
        routes.Set(res.url, res.data);
    }

    // Special cases
    Span<const uint8_t> *favicon = routes.Find("/static/favicon.ico");
    if (favicon) {
        routes.Set("/favicon.ico", *favicon);
    }
}

#ifndef NDEBUG
static bool UpdateStaticResources()
{
    LinkedAllocator temp_alloc;

    const char *filename = nullptr;
    const Span<const Resource> *lib_resources = nullptr;
#ifdef _WIN32
    Assert(GetApplicationDirectory());
    filename = Fmt(&temp_alloc, "%1%/drdw_res.dll", GetApplicationDirectory()).ptr;
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

    lib_resources = (const Span<const Resource> *)GetProcAddress(h, "static_resources");
#else
    Assert(GetApplicationDirectory());
    filename = Fmt(&temp_alloc, "%1%/drdw_res.so", GetApplicationDirectory()).ptr;
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

    lib_resources = (const Span<const Resource> *)dlsym(h, "static_resources");
#endif
    if (!lib_resources) {
        LogError("Cannot find symbol 'static_resources' in library '%1'", filename);
        return false;
    }

    static_resources.Clear();
    static_alloc.ReleaseAll();
    for (const Resource &res: *lib_resources) {
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
    Span<char> page = Fmt((Allocator *)nullptr, "Error %1", code);
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
        for (const mco_TableIndex &index: table_set->indexes) {
            char buf[32];

            writer.StartObject();
            writer.Key("begin_date"); writer.String(Fmt(buf, "%1", index.limit_dates[0]).ptr);
            writer.Key("end_date"); writer.String(Fmt(buf, "%1", index.limit_dates[1]).ptr);
            if (index.changed_tables & ~MaskEnum(mco_TableType::PriceTable)) {
                writer.Key("changed_tables"); writer.Bool(true);
            }
            if (index.changed_tables & MaskEnum(mco_TableType::PriceTable)) {
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

    const mco_TableIndex *index = table_set->FindIndex(date);
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
    const HashTable<mco_GhmCode, mco_GhmConstraint> &constraints =
        *index_to_constraints[index - table_set->indexes.ptr];

    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        char buf[512];

        writer.StartArray();
        for (const mco_GhmRootInfo &ghm_root_info: index->ghm_roots) {
            const mco_GhmRootDesc *ghm_root_desc = catalog_set->ghm_roots_map.Find(ghm_root_info.ghm_root);

            writer.StartObject();
            writer.Key("ghm_root"); writer.String(Fmt(buf, "%1", ghm_root_info.ghm_root).ptr);
            if (ghm_root_desc) {
                writer.Key("ghm_root_desc"); writer.String(ghm_root_desc->ghm_root_desc);
            }
            writer.Key("ghs"); writer.StartArray();

            Span<const mco_GhmToGhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
            for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
                const mco_GhsPriceInfo *ghs_price_info =
                    index->FindGhsPrice(ghm_to_ghs_info.Ghs(Sector::Public), Sector::Public);
                if (!ghs_price_info)
                    continue;

                const mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);
                if (!constraint)
                    continue;

                writer.StartObject();
                writer.Key("ghm"); writer.String(Fmt(buf, "%1", ghm_to_ghs_info.ghm).ptr);
                writer.Key("ghm_mode"); writer.String(&ghm_to_ghs_info.ghm.parts.mode, 1);
                {
                    uint32_t combined_duration_mask = constraint->duration_mask;
                    combined_duration_mask &= ~((1u << ghm_to_ghs_info.minimal_duration) - 1);
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
                if (ghm_to_ghs_info.bed_authorization) {
                    writer.String(Fmt(buf, "Autorisation Lit %1", ghm_to_ghs_info.bed_authorization).ptr);
                }
                if (ghm_to_ghs_info.unit_authorization) {
                    writer.String(Fmt(buf, "Autorisation Unité %1", ghm_to_ghs_info.unit_authorization).ptr);
                    if (ghm_to_ghs_info.minimal_duration) {
                        writer.String(Fmt(buf, "Durée Unitée Autorisée ≥ %1", ghm_to_ghs_info.minimal_duration).ptr);
                    }
                } else if (ghm_to_ghs_info.minimal_duration) {
                    writer.String(Fmt(buf, "Durée ≥ %1", ghm_to_ghs_info.minimal_duration).ptr);
                }
                if (ghm_to_ghs_info.minimal_age) {
                    writer.String(Fmt(buf, "Age ≥ %1", ghm_to_ghs_info.minimal_age).ptr);
                }
                if (ghm_to_ghs_info.main_diagnosis_mask.value) {
                    writer.String(Fmt(buf, "DP de la liste D$%1.%2",
                                      ghm_to_ghs_info.main_diagnosis_mask.offset,
                                      ghm_to_ghs_info.main_diagnosis_mask.value).ptr);
                }
                if (ghm_to_ghs_info.diagnosis_mask.value) {
                    writer.String(Fmt(buf, "Diagnostic de la liste D$%1.%2",
                                      ghm_to_ghs_info.diagnosis_mask.offset,
                                      ghm_to_ghs_info.diagnosis_mask.value).ptr);
                }
                for (const ListMask &mask: ghm_to_ghs_info.procedure_masks) {
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
                    if (ghs_price_info->flags & (int)mco_GhsPriceInfo::Flag::ExbOnce) {
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
        for (const mco_GhmRootDesc &desc: catalog_set->ghm_roots) {
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

static bool ParseDateRange(Span<const char> date_str, Date *out_start_date, Date *out_end_date)
{
    Date start_date = {};
    Date end_date = {};

    if (date_str.len) {
        Span<const char> str = date_str;
        start_date = Date::FromString(str, 0, &str);
        if (str.len < 2 || str[0] != '.' || str[1] != '.')
            goto invalid;
        end_date = Date::FromString(str.Take(2, str.len - 2), 0, &str);
        if (str.len)
            goto invalid;
        if (!start_date.IsValid() || !end_date.IsValid() || end_date <= start_date)
            goto invalid;
    }

    *out_start_date = start_date;
    *out_end_date = end_date;
    return true;

invalid:
    LogError("Invalid date range '%1'", date_str);
    return false;
}

static Response ProduceCaseMix(MHD_Connection *conn, const char *,
                               CompressionType compression_type)
{
    enum class DurationMode {
        Disabled,
        Partial,
        Full
    };

    struct CellSummary {
        mco_GhmCode ghm;
        int16_t ghs;
        int16_t duration;
        int count;
        int64_t ghs_price_cents;
    };

    union SummaryMapKey {
        struct {
            mco_GhmCode ghm;
            mco_GhsCode ghs;
            int16_t duration;
        } st;
        int64_t value;
        StaticAssert(SIZE(SummaryMapKey::value) == SIZE(SummaryMapKey::st));
    };

    Date dates[2] = {};
    Date diff_dates[2] = {};
    HashSet<UnitCode> units;
    DurationMode duration_mode = DurationMode::Disabled;
    {
        if (!ParseDateRange(MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "dates"),
                            &dates[0], &dates[1]))
            return CreateErrorPage(404);
        if (!ParseDateRange(MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "diff"),
                            &diff_dates[0], &diff_dates[1]))
            return CreateErrorPage(404);

        Span<const char> units_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "units");
        while (units_str.len) {
            Span<const char> unit_str = SplitStrAny(units_str, " ,+", &units_str);

            UnitCode unit;
            unit.number = ParseDec<int16_t>(unit_str).first;
            if (!unit.IsValid())
                return CreateErrorPage(404);

            units.Append(unit);
        }

        const char *durations_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "duration_mode");
        if (durations_str && durations_str[0]) {
            if (TestStr(durations_str, "partial")) {
                duration_mode = DurationMode::Partial;
            } else if (TestStr(durations_str, "full")) {
                duration_mode = DurationMode::Full;
            } else if (!TestStr(durations_str, "0")) {
                LogError("Invalid 'duration_mode' parameter value '%1'", durations_str);
                return CreateErrorPage(404);
            }
        }
    }

    if (diff_dates[0].value && !dates[0].value) {
        LogError("Parameter 'diff' specified but 'dates' is missing");
        return CreateErrorPage(404);
    }
    if (dates[0].value && diff_dates[0].value &&
               dates[0] < diff_dates[1] && dates[1] > diff_dates[0]) {
        LogError("Parameters 'dates' and 'diff' must not overlap");
        return CreateErrorPage(404);
    }

    HeapArray<mco_Result> results;
    mco_ClassifyParallel(*table_set, *authorization_set, stay_set.stays, 0, &results);

    HeapArray<CellSummary> summary;
    {
        HashMap<int64_t, Size> summary_map;
        for (const mco_Result &result: results) {
            if (result.ghm.IsError())
                continue;

            int multiplier;
            if (!dates[0].value ||
                    (result.stays[result.stays.len - 1].exit.date >= dates[0] &&
                     result.stays[result.stays.len - 1].exit.date < dates[1])) {
                multiplier = 1;
            } else if (diff_dates[0].value &&
                       result.stays[result.stays.len - 1].exit.date >= diff_dates[0] &&
                       result.stays[result.stays.len - 1].exit.date < diff_dates[1]) {
                multiplier = -1;
            } else {
                continue;
            }

            int full_duration = 0;
            for (const mco_Stay &stay: result.stays) {
                if (stay.exit.date != stay.entry.date) {
                    full_duration += stay.exit.date - stay.entry.date;
                } else {
                    full_duration++;
                }
            }

            int result_total = 0;
            for (Size i = 0; i < result.stays.len; i++) {
                const mco_Stay &stay = result.stays[i];

                if (units.table.count && !units.Find(stay.unit))
                    continue;

                int stay_duration = stay.exit.date - stay.entry.date;
                int stay_ghs_cents = result.ghs_price_cents * (stay_duration ? stay_duration : 1) / full_duration;
                result_total += stay_ghs_cents;
                // FIXME: Hack for rounding errors
                if (i == result.stays.len - 1) {
                    stay_ghs_cents += result.ghs_price_cents - result_total;
                }

                // TODO: Careful with duration overflow
                SummaryMapKey key;
                key.st.ghm = result.ghm;
                key.st.ghs = result.ghs;
                switch (duration_mode) {
                    case DurationMode::Disabled: { key.st.duration = 0; } break;
                    case DurationMode::Partial: { key.st.duration = (int16_t)stay_duration; } break;
                    case DurationMode::Full: { key.st.duration = (int16_t)result.duration; } break;
                }

                std::pair<int64_t *, bool> ret = summary_map.Append(key.value, summary.len);
                if (ret.second) {
                    CellSummary cell_summary = {};
                    cell_summary.ghm = result.ghm;
                    cell_summary.ghs = result.ghs.number;
                    cell_summary.duration = key.st.duration;
                    summary.Append(cell_summary);
                }
                summary[*ret.first].count += multiplier;
                summary[*ret.first].ghs_price_cents += multiplier * stay_ghs_cents;
            }
        }
    }

    std::sort(summary.begin(), summary.end(), [](const CellSummary &cs1, const CellSummary &cs2) {
        return MultiCmp(cs1.ghm.value - cs2.ghm.value,
                        cs1.ghs - cs2.ghs,
                        cs1.duration - cs2.duration) < 0;
    });

    MHD_Response *response = BuildJson(compression_type,
                                       [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        writer.StartArray();
        for (const CellSummary &cs: summary) {
            char buf[32];

            writer.StartObject();
            writer.Key("ghm"); writer.String(Fmt(buf, "%1", cs.ghm).ptr);
            writer.Key("ghs"); writer.Int(cs.ghs);
            if (duration_mode != DurationMode::Disabled) {
                writer.Key("duration"); writer.Int(cs.duration);
            }
            writer.Key("stays_count"); writer.Int(cs.count);
            writer.Key("ghs_price_cents"); writer.Int64(cs.ghs_price_cents);
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
#ifndef NDEBUG
    UpdateStaticResources();
#endif

    Span<const uint8_t> resource_data = routes.FindValue(url, {});
    if (!resource_data.IsValid())
        resource_data = routes.FindValue("/", {});
        // return CreateErrorPage(404);

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
    } else if (TestStr(url, "/api/casemix.json")) {
        response = ProduceCaseMix(conn, url, compression_type);
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

    -s, --stays <file>           Stays
)");
        PrintLn(fp, mco_options_usage);
    };

    LinkedAllocator temp_alloc;

    // Add default data directory
    {
        const char *app_dir = GetApplicationDirectory();
        if (app_dir) {
            const char *default_data_dir = Fmt(&temp_alloc, "%1%/data", app_dir).ptr;
            mco_data_directories.Append(default_data_dir);
        }
    }

    uint16_t port = 8888;
    HeapArray<const char *> stays_filenames;
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
            } else if (TestOption(opt, "-s", "--stays")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return 1;

                stays_filenames.Append(opt_parser.current_value);
            } else if (!mco_HandleMainOption(opt_parser, PrintUsage)) {
                return 1;
            }
        }
    }

    LogInfo("Loading tables");
    table_set = mco_GetMainTableSet();
    if (!table_set || !table_set->indexes.len)
        return 1;
    if (stays_filenames.len) {
        authorization_set = mco_GetMainAuthorizationSet();
        if (!authorization_set)
            return 1;
    }
    catalog_set = mco_GetMainCatalogSet();
    if (!catalog_set || !catalog_set->ghm_roots.len)
        return 1;

    if (stays_filenames.len) {
        LogInfo("Loading stays");

        mco_StaySetBuilder stay_set_builder;
        if (!stay_set_builder.LoadFiles(stays_filenames))
            return false;
        if (!stay_set_builder.Finish(&stay_set))
            return false;
    }

    LogInfo("Computing constraints");
    Async async;
    for (Size i = 0; i < table_set->indexes.len; i++) {

        // Extend or remove this check when constraints go beyond the tree info (diagnoses, etc.)
        if (table_set->indexes[i].changed_tables & MaskEnum(mco_TableType::GhmDecisionTree)) {
            HashTable<mco_GhmCode, mco_GhmConstraint> *constraints = constraints_set.AppendDefault();
            async.AddTask([=]() {
                return mco_ComputeGhmConstraints(table_set->indexes[i], constraints);
            });
        }

        index_to_constraints.Append(&constraints_set[constraints_set.len - 1]);
    }
    if (!async.Sync())
        return 1;

#ifndef NDEBUG
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

    LogInfo("Listening on port %1", MHD_get_daemon_info(daemon, MHD_DAEMON_INFO_BIND_PORT)->port);
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
