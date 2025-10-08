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
#include "api.hh"
#include "config.hh"
#include "database.hh"
#include "ludivine.hh"
#include "mail.hh"
#include "src/core/sandbox/sandbox.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#if !defined(_WIN32)
    #include <signal.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
#endif

namespace K {

Config config;
sq_Database db;

static HashMap<const char *, const AssetInfo *> assets_map;
static AssetInfo assets_index;
static BlockAllocator assets_alloc;
static char shared_etag[17];

static bool ApplySandbox(Span<const char *const> reveals)
{
    sb_SandboxBuilder sb;

    if (!sb.Init())
        return false;

    sb.RevealPaths(reveals, false);

#if defined(__linux__)
    sb.RevealPaths({
        "/etc/resolv.conf",
        "/etc/hosts"
    }, true);

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
        if (TestStr(asset.name, "src/ludivine/client/index.html")) {
            assets_index = asset;
            assets_map.Set("/", &assets_index);
        } else if (TestStr(asset.name, "src/ludivine/assets/main/ldv.webp")) {
            assets_map.Set("/favicon.webp", &asset);
        } else {
            Span<const char> name = SplitStrReverseAny(asset.name, K_PATH_SEPARATORS);

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

    K_ASSERT(js);
    K_ASSERT(css);

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
            json.Key("contact"); json.String(config.contact);
            json.Key("url"); json.String(config.url);
            json.Key("pages"); json.StartArray();
            for (const Config::PageInfo &page: config.pages) {
                json.StartObject();
                json.Key("title"); json.String(page.title);
                json.Key("url"); json.String(page.url);
                json.EndObject();
            }
            json.EndArray();
            json.Key("test"); json.Bool(config.test_mode);
            json.EndObject();
        } else if (key == "JS") {
            writer->Write(js);
        } else if (key == "CSS") {
            writer->Write(css);
        } else if (key == "BUNDLES") {
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
        } else if (TestStr(request.path, "/api/token") && request.method == http_RequestMethod::Post) {
            HandleToken(io);
        } else if (TestStr(request.path, "/api/protect") && request.method == http_RequestMethod::Post) {
            HandleProtect(io);
        } else if (TestStr(request.path, "/api/password") && request.method == http_RequestMethod::Post) {
            HandlePassword(io);
        } else if (TestStr(request.path, "/api/download") && request.method == http_RequestMethod::Get) {
            HandleDownload(io);
        } else if (TestStr(request.path, "/api/upload") && request.method == http_RequestMethod::Put) {
            HandleUpload(io);
        } else if (TestStr(request.path, "/api/remind") && request.method == http_RequestMethod::Post) {
            HandleRemind(io);
        } else if (TestStr(request.path, "/api/publish") && request.method == http_RequestMethod::Post) {
            HandlePublish(io);
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
            int64_t max_age = StartsWith(request.path, "/static/") ? (28ll * 86400000) : 0;
            AttachStatic(io, *asset, max_age, shared_etag);

            return;
        }
    }

    io->SendError(404);
}

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "ludivine.ini";
    bool sandbox = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: %2)%!0

    %!..+-p, --port port%!0                Change web server port
                                   %!D..(default: %3)%!0
        %!..+--bind IP%!0                  Bind to specific IP

        %!..+--sandbox%!0                  Run sandboxed (on supported platforms))",
                FelixTarget, config_filename, config.http.port);
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
                    config_filename = Fmt(&temp_alloc, "%1%/ludivine.ini", TrimStrRight(opt.current_value, K_PATH_SEPARATORS)).ptr;
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
            } else if (opt.Test("--bind", OptionType::Value)) {
                config.http.bind_addr = opt.current_value;
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
    if (!db.Open(config.database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return 1;
    if (!db.SetWAL(true))
        return 1;
    if (!MigrateDatabase(&db, config.vault_directory))
        return 1;
    if (!MakeDirectory(config.vault_directory, false))
        return 1;
    if (!MakeDirectory(config.tmp_directory, false))
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

#if defined(__linux__)
    if (!NotifySystemd())
        return 1;
#endif

    // Apply sandbox
    if (sandbox) {
        LogInfo("Init sandbox");

        // We use temp_store = MEMORY but, just in case...
        sqlite3_temp_directory = sqlite3_mprintf("%s", config.tmp_directory);

        HeapArray<const char *> reveals;

#if defined(FELIX_HOT_ASSETS)
        // Needed for asset module
        reveals.Append(GetApplicationDirectory());
#endif

        const char *database_directory = DuplicateString(GetPathDirectory(config.database_filename), &temp_alloc).ptr;

        reveals.Append(database_directory);
        reveals.Append(config.vault_directory);
        reveals.Append(config.tmp_directory);
        if (config.static_directory) {
            reveals.Append(config.static_directory);
        }

        if (!ApplySandbox(reveals))
            return 1;
    }

    // Run!
    if (!daemon.Start(HandleRequest))
        return 1;

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
            }

            LogDebug("Send mails");
            SendMails();
        }
    }

    LogInfo("Stop HTTP server");
    daemon.Stop();

    return status;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
