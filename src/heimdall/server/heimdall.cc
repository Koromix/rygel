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
#include "heimdall.hh"
#include "config.hh"
#include "database.hh"
#include "src/core/sandbox/sandbox.hh"

namespace K {

struct StaticRoute {
    const char *url;
    const AssetInfo *asset;
    const char *sourcemap;

    K_HASHTABLE_HANDLER(StaticRoute, url);
};

Config config;

static HashTable<const char *, StaticRoute> assets_map;
static AssetInfo assets_index;
static BlockAllocator assets_alloc;
static char shared_etag[17];

static int RunMigrate(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *filename = nullptr;

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 migrate [option...] filename%!0)"), FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        filename = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!filename) {
        LogError("Missing database filename");
        return 1;
    }

    sq_Database db;
    if (!db.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return 1;
    if (!db.SetWAL(true))
        return 1;
    if (!MigrateDatabase(&db))
        return 1;
    if (!db.Close())
        return 1;

    return 0;
}

static bool ApplySandbox(Span<const char *const> reveal_paths, Span<const char *const> mask_files)
{
    if (!sb_IsSandboxSupported()) {
        LogError("Sandbox mode is not supported on this platform");
        return false;
    }

    sb_SandboxBuilder sb;

    sb.RevealPaths(reveal_paths, false);
    sb.MaskFiles(mask_files);

#if defined(__linux__)
    sb.FilterSyscalls({
        { "exit", sb_FilterAction::Allow },
        { "exit_group", sb_FilterAction::Allow },
        { "brk", sb_FilterAction::Allow },
        { "mmap/anon", sb_FilterAction::Allow },
        { "mmap/shared", sb_FilterAction::Allow },
        { "munmap", sb_FilterAction::Allow },
        { "mremap", sb_FilterAction::Allow },
        { "mprotect/noexec", sb_FilterAction::Allow },
        { "mlock", sb_FilterAction::Allow },
        { "mlock2", sb_FilterAction::Allow },
        { "mlockall", sb_FilterAction::Allow },
        { "madvise", sb_FilterAction::Allow },
        { "pipe", sb_FilterAction::Allow },
        { "pipe2", sb_FilterAction::Allow },
        { "open", sb_FilterAction::Allow },
        { "openat", sb_FilterAction::Allow },
        { "openat2", sb_FilterAction::Allow },
        { "close", sb_FilterAction::Allow },
        { "fcntl", sb_FilterAction::Allow },
        { "read", sb_FilterAction::Allow },
        { "readv", sb_FilterAction::Allow },
        { "write", sb_FilterAction::Allow },
        { "writev", sb_FilterAction::Allow },
        { "pread64", sb_FilterAction::Allow },
        { "pwrite64", sb_FilterAction::Allow },
        { "lseek", sb_FilterAction::Allow },
        { "ftruncate", sb_FilterAction::Allow },
        { "fsync", sb_FilterAction::Allow },
        { "fdatasync", sb_FilterAction::Allow },
        { "fstat", sb_FilterAction::Allow },
        { "stat", sb_FilterAction::Allow },
        { "lstat", sb_FilterAction::Allow },
        { "lstat64", sb_FilterAction::Allow },
        { "fstatat64", sb_FilterAction::Allow },
        { "newfstatat", sb_FilterAction::Allow },
        { "statx", sb_FilterAction::Allow },
        { "access", sb_FilterAction::Allow },
        { "faccessat", sb_FilterAction::Allow },
        { "faccessat2", sb_FilterAction::Allow },
        { "ioctl/tty", sb_FilterAction::Allow },
        { "getrandom", sb_FilterAction::Allow },
        { "getpid", sb_FilterAction::Allow },
        { "gettid", sb_FilterAction::Allow },
        { "getuid", sb_FilterAction::Allow },
        { "getgid", sb_FilterAction::Allow },
        { "geteuid", sb_FilterAction::Allow },
        { "getegid", sb_FilterAction::Allow },
        { "getcwd", sb_FilterAction::Allow },
        { "rt_sigaction", sb_FilterAction::Allow },
        { "rt_sigpending", sb_FilterAction::Allow },
        { "rt_sigprocmask", sb_FilterAction::Allow },
        { "rt_sigqueueinfo", sb_FilterAction::Allow },
        { "rt_sigreturn", sb_FilterAction::Allow },
        { "rt_sigsuspend", sb_FilterAction::Allow },
        { "rt_sigtimedwait", sb_FilterAction::Allow },
        { "rt_sigtimedwait_time64", sb_FilterAction::Allow },
        { "waitpid", sb_FilterAction::Allow },
        { "waitid", sb_FilterAction::Allow },
        { "wait3", sb_FilterAction::Allow },
        { "wait4", sb_FilterAction::Allow },
        { "kill", sb_FilterAction::Allow },
        { "tgkill", sb_FilterAction::Allow },
        { "mkdir", sb_FilterAction::Allow },
        { "mkdirat", sb_FilterAction::Allow },
        { "unlink", sb_FilterAction::Allow },
        { "unlinkat", sb_FilterAction::Allow },
        { "rename", sb_FilterAction::Allow },
        { "renameat", sb_FilterAction::Allow },
        { "renameat2", sb_FilterAction::Allow },
        { "rmdir", sb_FilterAction::Allow },
        { "chown", sb_FilterAction::Allow },
        { "fchown", sb_FilterAction::Allow },
        { "fchownat", sb_FilterAction::Allow },
        { "chmod", sb_FilterAction::Allow },
        { "fchmod", sb_FilterAction::Allow },
        { "fchmodat", sb_FilterAction::Allow },
        { "fchmodat2", sb_FilterAction::Allow },
        { "clone", sb_FilterAction::Allow },
        { "clone3", sb_FilterAction::Allow },
        { "futex", sb_FilterAction::Allow },
        { "futex_time64", sb_FilterAction::Allow },
        { "rseq", sb_FilterAction::Allow },
        { "set_robust_list", sb_FilterAction::Allow },
        { "socket", sb_FilterAction::Allow },
        { "socketpair", sb_FilterAction::Allow },
        { "getsockopt", sb_FilterAction::Allow },
        { "setsockopt", sb_FilterAction::Allow },
        { "getsockname", sb_FilterAction::Allow },
        { "getpeername", sb_FilterAction::Allow },
        { "connect", sb_FilterAction::Allow },
        { "bind", sb_FilterAction::Allow },
        { "listen", sb_FilterAction::Allow },
        { "accept", sb_FilterAction::Allow },
        { "accept4", sb_FilterAction::Allow },
        { "eventfd", sb_FilterAction::Allow },
        { "eventfd2", sb_FilterAction::Allow },
        { "getdents", sb_FilterAction::Allow },
        { "getdents64", sb_FilterAction::Allow },
        { "prctl", sb_FilterAction::Allow },
        { "epoll_create", sb_FilterAction::Allow },
        { "epoll_create1", sb_FilterAction::Allow },
        { "epoll_ctl", sb_FilterAction::Allow },
        { "epoll_pwait", sb_FilterAction::Allow },
        { "epoll_wait", sb_FilterAction::Allow },
        { "poll", sb_FilterAction::Allow },
        { "ppoll", sb_FilterAction::Allow },
        { "select", sb_FilterAction::Allow },
        { "pselect6", sb_FilterAction::Allow },
        { "clock_nanosleep", sb_FilterAction::Allow },
        { "clock_gettime", sb_FilterAction::Allow },
        { "clock_gettime64", sb_FilterAction::Allow },
        { "clock_nanosleep", sb_FilterAction::Allow },
        { "clock_nanosleep_time64", sb_FilterAction::Allow },
        { "nanosleep", sb_FilterAction::Allow },
        { "sched_yield", sb_FilterAction::Allow },
        { "sched_getaffinity", sb_FilterAction::Allow },
        { "recv", sb_FilterAction::Allow },
        { "recvfrom", sb_FilterAction::Allow },
        { "recvmmsg", sb_FilterAction::Allow },
        { "recvmmsg_time64", sb_FilterAction::Allow },
        { "recvmsg", sb_FilterAction::Allow },
        { "sendmsg", sb_FilterAction::Allow },
        { "sendmmsg", sb_FilterAction::Allow },
        { "sendfile", sb_FilterAction::Allow },
        { "sendfile64", sb_FilterAction::Allow },
        { "sendto", sb_FilterAction::Allow },
        { "shutdown", sb_FilterAction::Allow },
        { "uname", sb_FilterAction::Allow },
        { "utime", sb_FilterAction::Allow },
        { "utimensat", sb_FilterAction::Allow },
        { "getrusage", sb_FilterAction::Allow }
    });
#endif

    return sb.Apply();
}

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
        FillRandomSafe(&buf, K_SIZE(buf));
        Fmt(shared_etag, "%1", FmtHex(buf).Pad0(-16));
    }

    HeapArray<const char *> bundles;
    const char *js = nullptr;
    const char *css = nullptr;

    for (const AssetInfo &asset: GetEmbedAssets()) {
        if (TestStr(asset.name, "src/heimdall/client/index.html")) {
            assets_index = asset;
        } else {
            Span<const char> name = SplitStrReverseAny(asset.name, K_PATH_SEPARATORS);

            if (NameContainsHash(name)) {
                const char *url = Fmt(&assets_alloc, "/static/%1", name).ptr;
                StaticRoute route = { url, &asset, nullptr };

                assets_map.Set(route);
            } else {
                const char *url = Fmt(&assets_alloc, "/static/%1/%2", shared_etag, name).ptr;
                StaticRoute route = { url, &asset, nullptr };

                if (EndsWith(name, ".js") || EndsWith(name, ".css")) {
                    const char *sourcemap = Fmt(&assets_alloc, "%1.map", asset.name).ptr;

                    if (FindEmbedAsset(sourcemap)) {
                        route.sourcemap = Fmt(&assets_alloc, "%1.map", name).ptr;
                    }
                }

                assets_map.Set(route);

                if (name == "heimdall.js") {
                    js = url;
                } else if (name == "heimdall.css") {
                    css = url;
                } else if (EndsWith(name, ".js")) {
                    bundles.Append(url);
                }
            }
        }
    }

    K_ASSERT(js);
    K_ASSERT(css);

    assets_index.data = PatchFile(assets_index, &assets_alloc, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "VERSION") {
            writer->Write(FelixVersion);
        } else if (key == "COMPILER") {
            writer->Write(FelixCompiler);
        } else if (key == "JS") {
            writer->Write(js);
        } else if (key == "CSS") {
            writer->Write(css);
        } else if (key == "CSS") {
            writer->Write(css);
        }  else if (key == "BUNDLES") {
            json_Writer json(writer);

            json.StartObject();
            for (const char *bundle: bundles) {
                const char *name = SplitStrReverseAny(bundle, K_PATH_SEPARATORS).ptr;
                json.Key(name); json.String(bundle);
            }
            json.EndObject();
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });
}

static void AttachStatic(http_IO *io, const AssetInfo &asset, const char *sourcemap, int64_t max_age)
{
    const http_RequestInfo &request = io->Request();
    const char *client_etag = request.GetHeaderValue("If-None-Match");

    if (client_etag && TestStr(client_etag, shared_etag)) {
        io->SendEmpty(304);
    } else {
        const char *mimetype = GetMimeType(GetPathExtension(asset.name));

        if (sourcemap) {
            io->AddHeader("SourceMap", sourcemap);
        }
        io->AddCachingHeaders(max_age, shared_etag);

        io->SendAsset(200, asset.data, mimetype, asset.compression_type);
    }
}

static bool IsProjectNameSafe(const char *project)
{
    return !PathIsAbsolute(project) && !PathContainsDotDot(project);
}

static void HandleData(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    const char *project = request.GetQueryValue("project");

    if (!project) {
        LogError("Missing project parameter");
        io->SendError(422);
        return;
    }
    if (!IsProjectNameSafe(project)) {
        LogError("Unsafe project name");
        io->SendError(403);
        return;
    }

    const char *filename = Fmt(io->Allocator(), "%1%/%2.db", config.project_directory, project).ptr;

    if (!TestFile(filename)) {
        LogError("Unknown project '%1'", project);
        io->SendError(404);
        return;
    }

    sq_Database db;
    if (!db.Open(filename, SQLITE_OPEN_READONLY))
        return;
    if (!db.SetWAL(true))
        return;

    // Make sure we can read this database
    {
        int version = GetDatabaseVersion(&db);
        if (version < 0)
            return;
        if (version != DatabaseVersion) {
            LogError("Cannot read from database schema version %1 (expected %2)", version, DatabaseVersion);
            io->SendError(403);
            return;
        }
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        // Dump views
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(SELECT v.view, v.name, i.path, c.domain || '::' || c.name
                               FROM views v
                               INNER JOIN items i ON (i.view = v.view)
                               INNER JOIN concepts c ON (c.concept = i.concept)
                               ORDER BY v.view)", &stmt))
                return;

            json->Key("views"); json->StartArray();
            if (stmt.Step()) {
                do {
                    int64_t view = sqlite3_column_int64(stmt, 0);
                    const char *name = (const char *)sqlite3_column_text(stmt, 1);

                    json->StartObject();

                    json->Key("name"); json->String(name);
                    json->Key("items"); json->StartObject();
                    do {
                        const char *path = (const char *)sqlite3_column_text(stmt, 2);
                        const char *name = (const char *)sqlite3_column_text(stmt, 3);

                        json->Key(path); json->String(name);
                    } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == view);
                    json->EndObject();

                    json->EndObject();
                } while (stmt.IsRow());
            }
            if (!stmt.IsValid())
                return;
            json->EndArray();
        }

        // Dump entities
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(SELECT en.entity, en.name,
                                      ev.event, ce.domain || '::' || ce.name, ev.timestamp, ev.warning,
                                      p.period, cp.domain || '::' || cp.name, p.timestamp, p.duration, p.warning,
                                      m.measure, cm.domain || '::' || cm.name, m.timestamp, m.value, m.warning
                               FROM entities en
                               LEFT JOIN events ev ON (ev.entity = en.entity)
                               LEFT JOIN concepts ce ON (ce.concept = ev.concept)
                               LEFT JOIN periods p ON (p.entity = en.entity)
                               LEFT JOIN concepts cp ON (cp.concept = p.concept)
                               LEFT JOIN measures m ON (m.entity = en.entity)
                               LEFT JOIN concepts cm ON (cm.concept = m.concept)
                               ORDER BY en.name)", &stmt))
                return;

            // Reuse for performance
            HeapArray<char> events;
            HeapArray<char> periods;
            HeapArray<char> values;

            json->Key("entities"); json->StartArray();
            if (stmt.Step()) {
                do {
                    events.RemoveFrom(0);
                    periods.RemoveFrom(0);
                    values.RemoveFrom(0);

                    int64_t entity = sqlite3_column_int64(stmt, 0);
                    const char *name = (const char *)sqlite3_column_text(stmt, 1);

                    json->StartObject();

                    json->Key("name"); json->String(name);

                    int64_t start = INT64_MAX;
                    int64_t end = INT64_MIN;

                    // Walk elements
                    {
                        StreamWriter st1(&events, "<json>");
                        StreamWriter st2(&periods, "<json>");
                        StreamWriter st3(&values, "<json>");
                        json_Writer json1(&st1);
                        json_Writer json2(&st2);
                        json_Writer json3(&st3);

                        int64_t prev_event = 0;
                        int64_t prev_period = 0;
                        int64_t prev_measure = 0;

                        json1.StartArray();
                        json2.StartArray();
                        json3.StartArray();
                        do {
                            if (int64_t event = sqlite3_column_int64(stmt, 2); event > prev_event) {
                                prev_event = event;

                                const char *name = (const char *)sqlite3_column_text(stmt, 3);
                                int64_t time = sqlite3_column_int64(stmt, 4);
                                bool warning = sqlite3_column_int(stmt, 5);

                                json1.StartObject();
                                json1.Key("concept"); json1.String(name);
                                json1.Key("time"); json1.Int64(time);
                                json1.Key("warning"); json1.Bool(warning);
                                json1.EndObject();

                                start = std::min(start, time);
                                end = std::max(end, time);
                            }

                            if (int64_t period = sqlite3_column_int64(stmt, 6); period > prev_period) {
                                prev_period = period;

                                const char *name = (const char *)sqlite3_column_text(stmt, 7);
                                int64_t time = sqlite3_column_int64(stmt, 8);
                                int64_t duration = sqlite3_column_int64(stmt, 9);
                                bool warning = sqlite3_column_int(stmt, 10);

                                json2.StartObject();
                                json2.Key("concept"); json2.String(name);
                                json2.Key("time"); json2.Int64(time);
                                json2.Key("duration"); json2.Int64(duration);
                                json2.Key("warning"); json2.Bool(warning);
                                json2.EndObject();

                                start = std::min(start, time);
                                end = std::max(end, time + duration);
                            }

                            if (int64_t measure = sqlite3_column_int64(stmt, 11); measure > prev_measure) {
                                prev_measure = measure;

                                const char *name = (const char *)sqlite3_column_text(stmt, 12);
                                int64_t time = sqlite3_column_int64(stmt, 13);
                                double value = sqlite3_column_double(stmt, 14);
                                bool warning = sqlite3_column_int(stmt, 15);

                                json3.StartObject();
                                json3.Key("concept"); json3.String(name);
                                json3.Key("time"); json3.Int64(time);
                                json3.Key("value"); json3.Double(value);
                                json3.Key("warning"); json3.Bool(warning);
                                json3.EndObject();

                                start = std::min(start, time);
                                end = std::max(end, time);
                            }
                        } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == entity);
                        if (!stmt.IsValid())
                            return;
                        json1.EndArray();
                        json2.EndArray();
                        json3.EndArray();
                    }

                    json->Key("events"); json->Raw(events);
                    json->Key("periods"); json->Raw(periods);
                    json->Key("values"); json->Raw(values);

                    if (start < INT64_MAX) {
                        json->Key("start"); json->Int64(start);
                        json->Key("end"); json->Int64(end);
                    } else {
                        json->Key("start"); json->Null();
                        json->Key("end"); json->Null();
                    }

                    json->EndObject();
                } while (stmt.IsRow());
            }
            if (!stmt.IsValid())
                return;
            json->EndArray();
        }

        json->EndObject();
    });
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

    // Translate server-side errors
    {
        const char *lang = request.GetCookieValue("language");
        ChangeThreadLocale(lang);
    }

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");
    io->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    io->AddHeader("Cross-Origin-Embedder-Policy", "require-corp");
    io->AddHeader("X-Robots-Tag", "noindex");
    io->AddHeader("Permissions-Policy", "interest-cohort=()");

    // Help user
    if (TestStr(request.path, "/")) {
        LogError("Missing project name");
        io->SendError(404);
        return;
    }

    // API endpoint?
    if (StartsWith(request.path, "/api/")) {
        if (TestStr(request.path, "/api/data") && request.method == http_RequestMethod::Get) {
            HandleData(io);
        } else {
            io->SendError(404);
        }

        return;
    }

    // Embedded static asset?
    {
        Span<const char> path = request.path;
        const char *ext = GetPathExtension(path).ptr;

        if (!ext[0]) {
            K_ASSERT(path.len && path[0] == '/');

            if (path[path.len - 1] == '/') {
                Span<const char> redirect = TrimStrRight(path, '/');
                io->AddHeader("Location", redirect);
                io->SendEmpty(302);
                return;
            }

            const char *project = path.ptr + 1;

            if (!IsProjectNameSafe(project)) {
                LogError("Unsafe project name");
                io->SendError(403);
                return;
            }

            const char *filename = Fmt(io->Allocator(), "%1%/%2.db", config.project_directory, project).ptr;

            if (!TestFile(filename)) {
                LogError("Unknown project '%1'", project);
                io->SendError(404);
                return;
            }

            AttachStatic(io, assets_index, nullptr, 0);
            return;
        } else {
            const StaticRoute *route = assets_map.Find(request.path);

            if (route) {
                int64_t max_age = StartsWith(request.path, "/static/") ? (28ll * 86400000) : 0;
                AttachStatic(io, *route->asset, route->sourcemap, max_age);

                return;
            }
        }
    }

    io->SendError(404);
}

static int RunServe(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "heimdall.ini";
    bool sandbox = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 [serve] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: %2)%!0

    %!..+-p, --port port%!0                Change web server port
                                   %!D..(default: %3)%!0
        %!..+--sandbox%!0                  Run sandboxed (on supported platforms)

Other commands:

    %!..+migrate%!0                        Create or migrate project database)"),
                FelixTarget, config_filename, config.http.port);
    };

    // Find config filename
    {
        OptionParser opt(arguments, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                if (IsDirectory(opt.current_value)) {
                    config_filename = Fmt(&temp_alloc, "%1%/heimdall.ini", TrimStrRight(opt.current_value, K_PATH_SEPARATORS)).ptr;
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
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-p", "--port", OptionType::Value)) {
                if (!config.http.SetPortOrPath(opt.current_value))
                    return 1;
            } else if (opt.Test("--sandbox")) {
                sandbox = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        if (!config.Validate())
            return 1;
    }

    LogInfo("Init data");
    if (!MakeDirectory(config.project_directory, false))
        return 1;
    if (!MakeDirectory(config.tmp_directory, false))
        return 1;

    LogInfo("Init assets");
    InitAssets();

    // Run!
    LogInfo("Init HTTP server");
    http_Daemon daemon;
    if (!daemon.Bind(config.http))
        return 1;

#if defined(__linux__)
    if (!NotifySystemd())
        return 1;
#endif

    // Apply sandbox
    if (sandbox) {
        LogInfo("Init sandbox");

        // We use temp_store = MEMORY but, just in case...
        sqlite3_temp_directory = sqlite3_mprintf("%s", config.tmp_directory);

        const char *const reveal_paths[] = {
#if defined(FELIX_HOT_ASSETS)
            // Needed for asset module
            GetApplicationDirectory(),
#endif
            config.project_directory,
            config.tmp_directory,
        };
        const char *const mask_files[] = {
            config_filename
        };

        if (!ApplySandbox(reveal_paths, mask_files))
            return 1;
    }

    // Run!
    if (!daemon.Start(HandleRequest))
        return 1;

    // From here on, don't quit abruptly
    WaitForInterrupt(0);

    // Run periodic tasks until exit
    int status = 0;
    {
        bool run = true;
        int timeout = 180 * 1000;

        // Randomize the delay a bit to reduce situations where all goupile
        // services perform cleanups at the same time and cause a load spike.
        timeout += GetRandomInt(0, timeout / 4 + 1);
        LogInfo("Periodic timer set to %1 s", FmtDouble((double)timeout / 1000.0, 1));

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

int Main(int argc, char **argv)
{
    InitLocales(TranslationTables);

    // Handle help and version arguments
    if (argc >= 2) {
        if (TestStr(argv[1], "--help") || TestStr(argv[1], "help")) {
            if (argc >= 3 && argv[2][0] != '-') {
                argv[1] = argv[2];
                argv[2] = const_cast<char *>("--help");
            } else {
                const char *args[] = {"--help"};
                return RunServe(args);
            }
        } else if (TestStr(argv[1], "--version")) {
            PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
            PrintLn(T("Compiler: %1"), FelixCompiler);
            return 0;
        }
    }

    const char *cmd = nullptr;
    Span<const char *> arguments = {};

    if (argc >= 2) {
        cmd = argv[1];

        if (cmd[0] == '-') {
            cmd = "serve";
            arguments = MakeSpan((const char **)argv + 1, argc - 1);
        } else {
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        }
    } else {
        cmd = "serve";
    }

    if (TestStr(cmd, "migrate")) {
        return RunMigrate(arguments);
    } else if (TestStr(cmd, "serve")) {
        return RunServe(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
