// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "rokkerd.hh"
#include "alert.hh"
#include "config.hh"
#include "database.hh"
#include "mail.hh"
#include "plan.hh"
#include "repository.hh"
#include "user.hh"
#include "lib/native/sandbox/sandbox.hh"
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
    // More DNS resolving crap, the list was determined through an elaborate
    // process of trial and error.
    sb.RevealPaths({
        "/etc/resolv.conf",
        "/etc/hosts"
    }, true);

    sb.FilterSyscalls({
        { "restart_syscall", sb_FilterAction::Allow },
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
        Fmt(shared_etag, "%1", FmtHex(buf, 16));
    }

    HeapArray<const char *> bundles;
    const char *js = nullptr;
    const char *css = nullptr;

    for (const AssetInfo &asset: GetEmbedAssets()) {
        if (TestStr(asset.name, "src/rekkord/web/client/index.html")) {
            assets_index = asset;
            assets_map.Set("/", &assets_index);
        } else if (TestStr(asset.name, "src/rekkord/web/assets/main/rekkord.webp")) {
            assets_map.Set("/favicon.webp", &asset);
        } else {
            Span<const char> name = SplitStrReverseAny(asset.name, K_PATH_SEPARATORS);

            if (NameContainsHash(name)) {
                const char *url = Fmt(&assets_alloc, "/static/%1", name).ptr;
                assets_map.Set(url, &asset);
            } else {
                const char *url = Fmt(&assets_alloc, "/static/%1/%2", shared_etag, name).ptr;
                assets_map.Set(url, &asset);

                if (name == "main.js") {
                    js = url;
                } else if (name == "main.css") {
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
        } else if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "ENV") {
            json_Writer json(writer);

            json.StartObject();

            json.Key("title"); json.String(config.title);
            json.Key("url"); json.String(config.url);

            json.Key("sso"); json.StartArray();
            for (const oidc_Provider &provider: config.oidc_providers) {
                json.StartObject();

                json.Key("issuer"); json.String(provider.issuer);
                json.Key("title"); json.String(provider.title);

                json.EndObject();
            }
            json.EndArray();

            json.EndObject();
        } else if (key == "JS") {
            writer->Write(js);
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

    Span<const char> url = request.path;
    http_RequestMethod method = request.method;

    // CSRF protection
    if (method != http_RequestMethod::Get && !http_PreventCSRF(io))
        return;

    // Translate server-side errors
    {
        const char *lang = request.GetCookieValue("lang");
        ChangeThreadLocale(lang);
    }

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");
    io->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    io->AddHeader("Cross-Origin-Embedder-Policy", "require-corp");
    io->AddHeader("X-Robots-Tag", "noindex");
    io->AddHeader("Permissions-Policy", "interest-cohort=()");

    // API endpoint?
    if (StartsWith(url, "/api/")) {
        if (url == "/api/user/session" && method == http_RequestMethod::Get) {
            HandleUserSession(io);
        } else if (url == "/api/user/ping" && method == http_RequestMethod::Get) {
            HandleUserPing(io);
        } else if (url == "/api/user/register" && method == http_RequestMethod::Post) {
            HandleUserRegister(io);
        } else if (url == "/api/user/login" && method == http_RequestMethod::Post) {
            HandleUserLogin(io);
        } else if (url == "/api/user/logout" && method == http_RequestMethod::Post) {
            HandleUserLogout(io);
        } else if (url == "/api/user/recover" && method == http_RequestMethod::Post) {
            HandleUserRecover(io);
        } else if (url == "/api/user/reset" && method == http_RequestMethod::Post) {
            HandleUserReset(io);
        } else if (url == "/api/user/password" && method == http_RequestMethod::Post) {
            HandleUserPassword(io);
        } else if (url == "/api/user/security" && method == http_RequestMethod::Get) {
            HandleUserSecurity(io);
        } else if (url == "/api/sso/login" && method == http_RequestMethod::Post) {
            HandleSsoLogin(io);
        } else if (url == "/api/sso/oidc" && method == http_RequestMethod::Post) {
            HandleSsoOidc(io);
        } else if (url == "/api/sso/link" && method == http_RequestMethod::Post) {
            HandleSsoLink(io);
        } else if (url == "/api/sso/unlink" && method == http_RequestMethod::Post) {
            HandleSsoUnlink(io);
        } else if (url == "/api/totp/confirm" && method == http_RequestMethod::Post) {
            HandleTotpConfirm(io);
        } else if (url == "/api/totp/secret" && method == http_RequestMethod::Post) {
            HandleTotpSecret(io);
        } else if (url == "/api/totp/change" && method == http_RequestMethod::Post) {
            HandleTotpChange(io);
        } else if (url == "/api/totp/disable" && method == http_RequestMethod::Post) {
            HandleTotpDisable(io);
        } else if (url == "/api/picture/get" && method == http_RequestMethod::Get) {
            HandlePictureGet(io);
        } else if (url == "/api/picture/save" && method == http_RequestMethod::Post) {
            HandlePictureSave(io);
        } else if (url == "/api/picture/delete" && method == http_RequestMethod::Post) {
            HandlePictureDelete(io);
        } else if (url == "/api/repository/list" && method == http_RequestMethod::Get) {
            HandleRepositoryList(io);
        } else if (url == "/api/repository/get" && method == http_RequestMethod::Get) {
            HandleRepositoryGet(io);
        } else if (url == "/api/repository/save" && method == http_RequestMethod::Post) {
            HandleRepositorySave(io);
        } else if (url == "/api/repository/delete" && method == http_RequestMethod::Post) {
            HandleRepositoryDelete(io);
        } else if (url == "/api/repository/snapshots" && method == http_RequestMethod::Get) {
            HandleRepositorySnapshots(io);
        } else if (url == "/api/plan/list" && method == http_RequestMethod::Get) {
            HandlePlanList(io);
        } else if (url == "/api/plan/get" && method == http_RequestMethod::Get) {
            HandlePlanGet(io);
        } else if (url == "/api/plan/save" && method == http_RequestMethod::Post) {
            HandlePlanSave(io);
        } else if (url == "/api/plan/delete" && method == http_RequestMethod::Post) {
            HandlePlanDelete(io);
        } else if (url == "/api/plan/key" && method == http_RequestMethod::Post) {
            HandlePlanKey(io);
        } else if (url == "/api/plan/fetch" && method == http_RequestMethod::Get) {
            HandlePlanFetch(io);
        } else if (url == "/api/plan/report" && method == http_RequestMethod::Post) {
            HandlePlanReport(io);
        } else {
            io->SendError(404);
        }

        return;
    }

    // User picture?
    if (StartsWith(url, "/pictures/") && method == http_RequestMethod::Get) {
        HandlePictureGet(io);
        return;
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
            int64_t max_age = StartsWith(url, "/static/") ? (28ll * 86400000) : 0;
            AttachStatic(io, *asset, max_age, shared_etag);

            return;
        }
    }

    io->SendError(404);
}

int Main(int argc, char **argv)
{
    InitLocales(TranslationTables);

    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "rokkerd.ini";
    bool sandbox = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: %2)%!0

    %!..+-p, --port port%!0                Change web server port
                                   %!D..(default: %3)%!0
        %!..+--bind IP%!0                  Bind to specific IP

        %!..+--sandbox%!0                  Run sandboxed (on supported platforms))"),
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
                    config_filename = Fmt(&temp_alloc, "%1%/rokkerd.ini", TrimStrRight(opt.current_value, K_PATH_SEPARATORS)).ptr;
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
    if (!MigrateDatabase(&db))
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

        const char *database_directory = DuplicateString(GetPathDirectory(config.database_filename), &temp_alloc).ptr;

        const char *const reveals[] = {
#if defined(FELIX_HOT_ASSETS)
            // Needed for asset module
            GetApplicationDirectory(),
#endif
            database_directory,
            config.tmp_directory,
        };

        if (!ApplySandbox(reveals))
            return 1;
    }

    // Run!
    if (!daemon.Start(HandleRequest))
        return 1;

    // From here on, don't quit abruptly
    WaitEvents(0);

    // Run periodic tasks until exit
    int status = 0;
    {
        bool run = true;
        int timeout = 180 * 1000;

        // Randomize the delay a bit to reduce situations where all goupile
        // services perform cleanups at the same time and cause a load spike.
        timeout += randombytes_uniform(timeout / 4 + 1);
        LogInfo("Periodic timer set to %1 s", FmtDouble((double)timeout / 1000.0, 1));

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

            LogDebug("Prune tokens");
            PruneTokens();

            LogDebug("Prune sessions");
            PruneSessions();

            LogDebug("Detect alerts");
            DetectAlerts();

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
