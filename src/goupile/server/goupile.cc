// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "admin.hh"
#include "config.hh"
#include "domain.hh"
#include "file.hh"
#include "goupile.hh"
#include "instance.hh"
#include "message.hh"
#include "record.hh"
#include "user.hh"
#include "vm.hh"
#include "../legacy/records.hh"
#include "lib/native/http/http.hh"
#include "lib/native/request/curl.hh"
#include "lib/native/sandbox/sandbox.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#if !defined(_WIN32)
    #include <signal.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
#endif
#if defined(__GLIBC__)
    #include <malloc.h>
#endif

namespace K {

struct RenderInfo {
    const char *key;
    AssetInfo asset;
    int64_t time;

    K_HASHTABLE_HANDLER(RenderInfo, key);
};

Config gp_config;
sq_Database gp_db;

const int64_t FullSnapshotDelay = 86400 * 1000;

static HashMap<const char *, const AssetInfo *> assets_map;
static const AssetInfo *assets_root;
static HeapArray<const char *> assets_for_cache;
static BlockAllocator assets_alloc;
static char shared_etag[17];

static const int DemoPruneDelay = 7 * 86400 * 1000;

static bool ApplySandbox(Span<const char *const> reveals)
{
    sb_SandboxBuilder sb;

    // We need the network and to send signals to the zygote
    unsigned int flags = UINT_MAX & ~((int)sb_IsolationFlag::Network | ~(int)sb_IsolationFlag::Signals);

    if (!sb.Init(flags))
        return false;

    sb.RevealPaths(reveals, false);

#if defined(__linux__)
    // Force glibc to load all the NSS crap beforehand, so we don't need to
    // expose it in the sandbox...
    // What a bunch of crap. Why does all this need to use shared libraries??
    {
        struct addrinfo *result = nullptr;
        int err = getaddrinfo("www.example.com", nullptr, nullptr, &result);

        if (err != 0) {
            LogError("Failed to init DNS resolver: '%1'", gai_strerror(err));
            return false;
        }

        freeaddrinfo(result);
    }

    // More DNS resolving crap, the list was determined through an elaborate
    // process of trial and error.
    sb.RevealPaths({
        "/etc/resolv.conf",
        "/etc/hosts",
        "/etc/ld.so.cache"
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
        { "getrusage", sb_FilterAction::Allow },
        { "readlink", sb_FilterAction::Allow },
        { "readlinkat", sb_FilterAction::Allow },
        { "link", sb_FilterAction::Allow },
        { "linkat", sb_FilterAction::Allow }
    });
#endif

    return sb.Apply();
}

static void InitAssets()
{
    assets_map.Clear();
    assets_for_cache.Clear();
    assets_alloc.ReleaseAll();

    // Update ETag
    {
        uint64_t buf;
        FillRandomSafe(&buf, K_SIZE(buf));
        Fmt(shared_etag, "%1", FmtHex(buf, 16));
    }

    for (const AssetInfo &asset: GetEmbedAssets()) {
        if (TestStr(asset.name, "src/goupile/client/goupile.html")) {
            assets_map.Set("/", &asset);
        } else if (TestStr(asset.name, "src/goupile/client/root.html")) {
            assets_root = &asset;
        } else if (TestStr(asset.name, "src/goupile/client/sw.js")) {
            assets_map.Set("/sw.js", &asset);
            assets_map.Set("/sw.pk.js", &asset);
        } else if (TestStr(asset.name, "src/goupile/client/manifest.json")) {
            assets_map.Set("/manifest.json", &asset);
            assets_for_cache.Append("/manifest.json");
        } else if (TestStr(asset.name, "src/goupile/client/images/favicon.png")) {
            assets_map.Set("/favicon.png", &asset);
            assets_for_cache.Append("/favicon.png");
        } else if (TestStr(asset.name, "src/goupile/client/images/admin.png")) {
            assets_map.Set("/admin/favicon.png", &asset);
        } else if (StartsWith(asset.name, "src/goupile/client/") ||
                   StartsWith(asset.name, "vendor/opensans/")) {
            const char *name = SplitStrReverseAny(asset.name, K_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&assets_alloc, "/static/%1/%2", shared_etag, name).ptr;

            assets_map.Set(url, &asset);
            assets_for_cache.Append(url);
        } else if (StartsWith(asset.name, "vendor/")) {
            Span<const char> library = SplitStr(asset.name + 7, '/');

            const char *name = SplitStrReverseAny(asset.name, K_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&assets_alloc, "/static/%1/%2/%3", shared_etag, library, name).ptr;
            Span<const char> ext = GetPathExtension(name);

            assets_map.Set(url, &asset);

            if (ext != ".wasm") {
                assets_for_cache.Append(url);
            }
        }
    }

    std::sort(assets_for_cache.begin(), assets_for_cache.end(), [](const char *url1, const char *url2) {
        return CmpStr(url1, url2) < 0;
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

static void AttachTemplate(http_IO *io, const AssetInfo &asset, FunctionRef<void(Span<const char>, StreamWriter *)> func)
{
    Span<const uint8_t> render = PatchFile(asset, io->Allocator(), func);
    const char *mimetype = GetMimeType(GetPathExtension(asset.name));

    io->AddCachingHeaders(0, nullptr);
    io->SendAsset(200, render, mimetype, asset.compression_type);
}

static void HandlePing(http_IO *io, InstanceHolder *instance)
{
    // Do this to renew session and clear invalid session cookies
    GetNormalSession(io, instance);

    io->AddCachingHeaders(0, nullptr);
    io->SendText(200, "{}", "application/json");
}

static void HandleFileStatic(http_IO *io, InstanceHolder *instance)
{
    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        const char *base_url = Fmt(io->Allocator(), "/%1/", instance->key).ptr;
        json->String(base_url);

        for (const InstanceHolder *slave: instance->slaves) {
            const char *url = Fmt(io->Allocator(), "/%1/", slave->key).ptr;
            json->String(url);
        }

        for (const char *path: assets_for_cache) {
            const char *url = Fmt(io->Allocator(), "/%1%2", instance->key, path).ptr;
            json->String(url);
        }

        json->EndArray();
    });
}

#if !defined(_WIN32)

static void HandleProcessSignal(http_IO *io, int signal)
{
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsRoot()) {
        LogError("Non-root users are not allowed to signal process");
        io->SendError(403);
        return;
    }

    pid_t pid = getpid();
    kill(pid, signal);

    io->SendText(200, "{}", "application/json");
}

#endif

static void HandleAdminRequest(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    K_ASSERT(StartsWith(request.path, "/admin/") || TestStr(request.path, "/admin"));

    Span<const char> url = request.path + 6;
    http_RequestMethod method = request.method;

    // Missing trailing slash, redirect
    if (!url.len) {
        const char *redirect = Fmt(io->Allocator(), "%1/", request.path).ptr;

        io->AddHeader("Location", redirect);
        io->SendEmpty(302);

        return;
    }

    DomainHolder *domain = RefDomain(false);
    K_DEFER { UnrefDomain(domain); };

    // Try static assets
    {
        if (TestStr(url, "/")) {
            const AssetInfo *asset = assets_map.FindValue(url, nullptr);
            K_ASSERT(asset);

            const char *nonce = Fmt(io->Allocator(), "%1", FmtRandom(16)).ptr;

            Span<const char> csp = Fmt(io->Allocator(), "script-src 'self' 'nonce-%1'; "
                                                        "style-src 'self' 'unsafe-inline'; "
                                                        "style-src-elem 'self' 'nonce-%1'; "
                                                        "style-src-attr 'self' 'unsafe-inline'; "
                                                        "frame-ancestors 'none'", nonce);
            io->AddHeader("Content-Security-Policy", csp);
            io->AddHeader("X-Content-Type-Options", "nosniff");
            io->AddHeader("X-Frame-Options", "DENY");

            AttachTemplate(io, *asset, [&](Span<const char> expr, StreamWriter *writer) {
                Span<const char> key = TrimStr(expr);

                if (key == "NONCE") {
                    writer->Write(nonce);
                } else if (key == "VERSION") {
                    writer->Write(FelixVersion);
                } else if (key == "COMPILER") {
                    writer->Write(FelixCompiler);
                } else if (key == "TITLE") {
                    writer->Write("Goupile Admin");
                } else if (key == "BASE_URL") {
                    writer->Write("/admin/");
                } else if (key == "STATIC_URL") {
                    Print(writer, "/admin/static/%1/", shared_etag);
                } else if (key == "ENV_JSON") {
                    json_CompactWriter json(writer);
                    char buf[128];

                    int64_t now = GetUnixTime();
                    TimeSpec spec = DecomposeTimeLocal(now);

                    json.StartObject();
                    if (!domain->IsInstalled()) {
                        json.Key("upgrade"); json.Int(domain->GetUpgrade());
                    }
                    json.Key("key"); json.String("admin");
                    json.Key("urls"); json.StartObject();
                        json.Key("base"); json.String("/admin/");
                        json.Key("instance"); json.String("/admin/");
                        json.Key("static"); json.String(Fmt(buf, "/admin/static/%1/", shared_etag).ptr);
                    json.EndObject();
                    json.Key("title"); json.String("Admin");
                    json.Key("timezone"); json.Int(spec.offset);
                    json.Key("permissions"); json.StartObject();
                    for (Size i = 0; i < K_LEN(UserPermissionNames); i++) {
                        bool legacy = LegacyPermissionMask & (1 << i);

                        Span<const char> str = json_ConvertToJsonName(UserPermissionNames[i], buf);
                        json.Key(str.ptr, (size_t)str.len);
                        json.Bool(legacy);
                    }
                    json.EndObject();
                    json.EndObject();
                } else if (key == "HEAD_TAGS") {
                    // Nothing to add
                } else {
                    Print(writer, "{{%1}}", expr);
                }
            });

            return;
        } else if (url == "/favicon.png") {
            const AssetInfo *asset = assets_map.FindValue("/admin/favicon.png", nullptr);
            K_ASSERT(asset);

            AttachStatic(io, *asset, 0, shared_etag);

            return;
        } else {
            const AssetInfo *asset = assets_map.FindValue(url, nullptr);

            if (asset) {
                int64_t max_age = StartsWith(url, "/static/") ? (365ll * 86400000) : 0;
                AttachStatic(io, *asset, max_age, shared_etag);

                return;
            }
        }
    }

    // CSRF protection
    if (method != http_RequestMethod::Get && !http_PreventCSRF(io))
        return;

    // Minimal endpoints available even before initial configuration
    if (url == "/api/session/ping" && method == http_RequestMethod::Get) {
        HandlePing(io, nullptr);
        return;
    } else if (url == "/api/session/profile" && method == http_RequestMethod::Get) {
        HandleSessionProfile(io, nullptr);
        return;
    } else if (url == "/api/domain/configure" && method == http_RequestMethod::Post) {
        HandleDomainConfigure(io);
        return;
    } else if (!domain->IsInstalled()) [[unlikely]] {
        io->SendError(404);
        return;
    }

    // And now, admin endpoints
    if (url == "/api/session/login" && method == http_RequestMethod::Post) {
        HandleSessionLogin(io, nullptr);
    } else if (url == "/api/session/confirm" && method == http_RequestMethod::Post) {
        HandleSessionConfirm(io, nullptr);
    } else if (url == "/api/session/logout" && method == http_RequestMethod::Post) {
        HandleSessionLogout(io);
    } else if (url == "/api/change/password" && method == http_RequestMethod::Post) {
        HandleChangePassword(io, nullptr);
    } else if (url == "/api/change/secret" && method == http_RequestMethod::Post) {
        HandleChangeSecret(io, domain->settings.title);
    } else if (url == "/api/change/totp" && method == http_RequestMethod::Post) {
        HandleChangeTOTP(io);
    } else if (url == "/api/domain/info" && method == http_RequestMethod::Get) {
        HandleDomainInfo(io);
    } else if (url == "/api/domain/demo" && method == http_RequestMethod::Post) {
        HandleDomainDemo(io);
    } else if (url == "/api/domain/restore" && method == http_RequestMethod::Post) {
        HandleDomainRestore(io);
    } else if (url == "/api/instances/create" && method == http_RequestMethod::Post) {
        HandleInstanceCreate(io);
    } else if (url == "/api/instances/delete" && method == http_RequestMethod::Post) {
        HandleInstanceDelete(io);
    } else if (url == "/api/instances/migrate" && method == http_RequestMethod::Post) {
        HandleInstanceMigrate(io);
    } else if (url == "/api/instances/clear" && method == http_RequestMethod::Post) {
        HandleInstanceClear(io);
    } else if (url == "/api/instances/configure" && method == http_RequestMethod::Post) {
        HandleInstanceConfigure(io);
    } else if (url == "/api/instances/list" && method == http_RequestMethod::Get) {
        HandleInstanceList(io);
    } else if (url == "/api/instances/assign" && method == http_RequestMethod::Post) {
        HandleInstanceAssign(io);
    } else if (url == "/api/instances/permissions" && method == http_RequestMethod::Get) {
        HandleInstancePermissions(io);
    } else if (url == "/api/archives/create" && method == http_RequestMethod::Post) {
        HandleArchiveCreate(io);
    } else if (url == "/api/archives/delete" && method == http_RequestMethod::Post) {
        HandleArchiveDelete(io);
    } else if (url == "/api/archives/list" && method == http_RequestMethod::Get) {
        HandleArchiveList(io);
    } else if (StartsWith(url, "/api/archives/files/") && method == http_RequestMethod::Get) {
        HandleArchiveDownload(io);
    } else if (StartsWith(url, "/api/archives/files/") && method == http_RequestMethod::Put) {
        HandleArchiveUpload(io);
    } else if (url == "/api/users/create" && method == http_RequestMethod::Post) {
        HandleUserCreate(io);
    } else if (url == "/api/users/edit" && method == http_RequestMethod::Post) {
        HandleUserEdit(io);
    } else if (url == "/api/users/delete" && method == http_RequestMethod::Post) {
        HandleUserDelete(io);
    } else if (url == "/api/users/list" && method == http_RequestMethod::Get) {
        HandleUserList(io);
    } else if (url == "/api/send/mail" && method == http_RequestMethod::Post) {
        HandleSendMail(io, nullptr);
    } else if (url == "/api/send/sms" && method == http_RequestMethod::Post) {
        HandleSendSMS(io, nullptr);
#if !defined(_WIN32)
    } else if (url == "/api/process/exit" && method == http_RequestMethod::Post) {
        HandleProcessSignal(io, SIGTERM);
    } else if (url == "/api/process/interrupt" && method == http_RequestMethod::Post) {
        HandleProcessSignal(io, SIGINT);
#endif
    } else {
        io->SendError(404);
    }
}

static void HandleInstanceRequest(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    DomainHolder *domain = RefDomain();
    K_DEFER { UnrefDomain(domain); };

    if (!domain) [[unlikely]] {
        io->SendError(404);
        return;
    }

    Span<const char> url = request.path;
    http_RequestMethod method = request.method;
    InstanceHolder *instance = nullptr;

    // Find relevant instance
    for (int i = 0; i < 2 && url.len; i++) {
        Size offset = SplitStr(url.Take(1, url.len - 1), '/').len + 1;

        Span<const char> new_url = url.Take(offset, url.len - offset);
        Span<const char> new_key = MakeSpan(request.path + 1, new_url.ptr - request.path - 1);

        InstanceHolder *ref = domain->Ref(new_key);
        if (!ref)
            break;

        if (instance) {
            instance->Unref();
        }
        instance = ref;
        url = new_url;

        // No need to look further
        if (!instance->slaves.len)
            break;
    }
    if (!instance) {
        io->SendError(404);
        return;
    }
    K_DEFER { instance->Unref(); };

    // Enforce trailing slash on base URLs. Use 302 instead of 301 to avoid
    // problems with query strings being erased without question.
    if (!url.len) {
        HeapArray<char> buf(io->Allocator());

        Fmt(&buf, "%1/?", request.path);
        for (const http_KeyValue &value: request.values) {
            Fmt(&buf, "%1=%2&", FmtUrlSafe(value.key, "-._~"), FmtUrlSafe(value.value, "-._~"));
        }
        buf.ptr[buf.len - 1] = 0;

        io->AddHeader("Location", buf.ptr);
        io->SendEmpty(302);

        return;
    }

    // Enable COEP for offlines instances to get SharedArrayBuffer
    if (instance->settings.use_offline) {
        io->AddHeader("Cross-Origin-Embedder-Policy", "require-corp");
    }

    // Try application files
    if (method == http_RequestMethod::Get && HandleFileGet(io, instance))
        return;

    // Try static assets
    if (method == http_RequestMethod::Get && !StartsWith(url, "/api/") &&
                                             !StartsWith(url, "/blobs/")) {
        if (!GetPathExtension(url).len) {
            url = "/";
        }

        const AssetInfo *asset = assets_map.FindValue(url, nullptr);

        if (url == "/" || url == "/sw.js" || url == "/sw.pk.js" || url == "/manifest.json") {
            K_ASSERT(asset);

            const InstanceHolder *master = instance->master;
            int64_t fs_version = master->fs_version.load(std::memory_order_relaxed);

            const char *etag = Fmt(io->Allocator(), "%1_%2_%3_%4", shared_etag, (const void *)asset, instance->unique, fs_version).ptr;
            const char *nonce = (url == "/") ? Fmt(io->Allocator(), "%1", FmtRandom(16)).ptr : nullptr;

            if (nonce) {
                // We will make this more secure progressively!
                Span<const char> csp = Fmt(io->Allocator(), "script-src 'self' 'nonce-%1' 'unsafe-eval' blob:;"
                                                            "style-src 'self' 'unsafe-inline'; "
                                                            "style-src-elem 'self' 'nonce-%1'; "
                                                            "style-src-attr 'self' 'unsafe-inline'; "
                                                            "frame-ancestors 'none'", nonce);
                io->AddHeader("Content-Security-Policy", csp);
                io->AddHeader("X-Content-Type-Options", "nosniff");
                io->AddHeader("X-Frame-Options", "DENY");
            }

            AttachTemplate(io, *asset, [&](Span<const char> expr, StreamWriter *writer) {
                Span<const char> key = TrimStr(expr);

                if (key == "NONCE") {
                    K_ASSERT(nonce);
                    writer->Write(nonce);
                } else if (key == "VERSION") {
                    writer->Write(FelixVersion);
                } else if (key == "COMPILER") {
                    writer->Write(FelixCompiler);
                } else if (key == "TITLE") {
                    writer->Write(master->title);
                } else if (key == "BASE_URL") {
                    Print(writer, "/%1/", master->key);
                } else if (key == "STATIC_URL") {
                    Print(writer, "/%1/static/%2/", master->key, shared_etag);
                } else if (key == "ENV_JSON") {
                    json_CompactWriter json(writer);
                    char buf[512];

                    json.StartObject();
                    json.Key("key"); json.String(master->key.ptr);
                    json.Key("urls"); json.StartObject();
                        json.Key("base"); json.String(Fmt(buf, "/%1/", master->key).ptr);
                        json.Key("instance"); json.String(Fmt(buf, "/%1/", instance->key).ptr);
                        json.Key("static"); json.String(Fmt(buf, "/%1/static/%2/", master->key, shared_etag).ptr);
                        json.Key("files"); json.String(Fmt(buf, "/%1/files/%2/", master->key, fs_version).ptr);
                    json.EndObject();
                    json.Key("title"); json.String(master->title);
                    json.Key("lang"); json.String(master->settings.lang);
                    json.Key("legacy"); json.Bool(master->legacy);
                    json.Key("demo"); json.Bool(instance->demo);
                    json.Key("version"); json.Int64(fs_version);
                    json.Key("buster"); json.String(etag);
                    json.Key("use_offline"); json.Bool(master->settings.use_offline);
                    json.Key("data_remote"); json.Bool(master->settings.data_remote);
                    json.EndObject();
                } else if (key == "HEAD_TAGS") {
                    if (master->settings.use_offline) {
                        Print(writer, "<link rel=\"manifest\" href=\"/%1/manifest.json\"/>", master->key);
                    }
                } else {
                    Print(writer, "{{%1}}", expr);
                }
            });

            return;
        } else if (asset) {
            int64_t max_age = StartsWith(url, "/static/") ? (365ll * 86400000) : 0;
            AttachStatic(io, *asset, max_age, shared_etag);

            return;
        }
    }

    // CSRF protection
    if (method != http_RequestMethod::Get && !http_PreventCSRF(io))
        return;

    // And now, API endpoints
    if (url == "/api/session/ping" && method == http_RequestMethod::Get) {
        HandlePing(io, instance);
    } else if (url == "/api/session/profile" && method == http_RequestMethod::Get) {
        HandleSessionProfile(io, instance);
    } else if (url == "/api/session/login" && method == http_RequestMethod::Post) {
        HandleSessionLogin(io, instance);
    } else if (url == "/api/session/token" && method == http_RequestMethod::Post) {
        HandleSessionToken(io, instance);
    } else if (url == "/api/session/confirm" && method == http_RequestMethod::Post) {
        HandleSessionConfirm(io, instance);
    } else if (url == "/api/session/logout" && method == http_RequestMethod::Post) {
        HandleSessionLogout(io);
    } else if (url == "/api/change/password" && method == http_RequestMethod::Post) {
        HandleChangePassword(io, instance);
    } else if (url == "/api/change/secret" && method == http_RequestMethod::Post) {
        HandleChangeSecret(io, domain->settings.title);
    } else if (url == "/api/change/totp" && method == http_RequestMethod::Post) {
        HandleChangeTOTP(io);
    } else if (url == "/api/change/mode" && method == http_RequestMethod::Post) {
        HandleChangeMode(io, instance);
    } else if (url == "/api/change/key" && method == http_RequestMethod::Post) {
        HandleChangeApiKey(io, instance);
    } else if (url == "/api/files/static" && method == http_RequestMethod::Get) {
        HandleFileStatic(io, instance);
    } else if (url == "/api/files/list" && method == http_RequestMethod::Get) {
        HandleFileList(io, instance);
    } else if (StartsWith(url, "/files/") && method == http_RequestMethod::Put) {
        HandleFilePut(io, instance);
    } else if (StartsWith(url, "/files/") && method == http_RequestMethod::Delete) {
        HandleFileDelete(io, instance);
    } else if (url == "/api/files/history" && method == http_RequestMethod::Get) {
        HandleFileHistory(io, instance);
    } else if (url == "/api/files/restore" && method == http_RequestMethod::Post) {
        HandleFileRestore(io, instance);
    } else if (url == "/api/files/delta" && method == http_RequestMethod::Get) {
        HandleFileDelta(io, instance);
    } else if (url == "/api/files/publish" && method == http_RequestMethod::Post) {
        HandleFilePublish(io, instance);
    } else if (url == "/api/records/list" && method == http_RequestMethod::Get) {
        HandleRecordList(io, instance);
    } else if (url == "/api/records/get" && method == http_RequestMethod::Get) {
        HandleRecordGet(io, instance);
    } else if (!instance->legacy && url == "/api/records/audit" && method == http_RequestMethod::Get) {
        HandleRecordAudit(io, instance);
    } else if (!instance->legacy && url == "/api/records/reserve" && method == http_RequestMethod::Post) {
        HandleRecordReserve(io, instance);
    } else if (!instance->legacy && url == "/api/records/save" && method == http_RequestMethod::Post) {
        HandleRecordSave(io, instance);
    } else if (!instance->legacy && url == "/api/records/delete" && method == http_RequestMethod::Post) {
        HandleRecordDelete(io, instance);
    } else if (!instance->legacy && url == "/api/records/lock" && method == http_RequestMethod::Post) {
        HandleRecordLock(io, instance);
    } else if (!instance->legacy && url == "/api/records/unlock" && method == http_RequestMethod::Post) {
        HandleRecordUnlock(io, instance);
    } else if (!instance->legacy && url == "/api/records/public" && method == http_RequestMethod::Get) {
        HandleRecordPublic(io, instance);
    } else if (!instance->legacy && StartsWith(url, "/blobs/") && method == http_RequestMethod::Get) {
        HandleBlobGet(io, instance);
    } else if (!instance->legacy && url == "/api/records/blob" && method == http_RequestMethod::Post) {
        HandleBlobPost(io, instance);
    } else if (!instance->legacy && url == "/api/bulk/export" && method == http_RequestMethod::Post) {
        HandleBulkExport(io, instance);
    } else if (!instance->legacy && url == "/api/bulk/list" && method == http_RequestMethod::Get) {
        HandleBulkList(io, instance);
    } else if (!instance->legacy && url == "/api/bulk/download" && method == http_RequestMethod::Get) {
        HandleBulkDownload(io, instance);
    } else if (!instance->legacy && url == "/api/bulk/import" && method == http_RequestMethod::Post) {
        HandleBulkImport(io, instance);
    } else if (instance->legacy && url == "/api/records/load" && method == http_RequestMethod::Get) {
        HandleLegacyLoad(io, instance);
    } else if (instance->legacy && url == "/api/records/save" && method == http_RequestMethod::Post) {
        HandleLegacySave(io, instance);
    } else if (instance->legacy && url == "/api/records/export" && method == http_RequestMethod::Get) {
        HandleLegacyExport(io, instance);
    } else if (url == "/api/send/mail" && method == http_RequestMethod::Post) {
        HandleSendMail(io, instance);
    } else if (url == "/api/send/sms" && method == http_RequestMethod::Post) {
        HandleSendSMS(io, instance);
    } else if (url == "/api/send/tokenize" && method == http_RequestMethod::Post) {
        HandleSendTokenize(io, instance);
    } else {
        io->SendError(404);
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

    // Translate server-side errors
    {
        const char *lang = request.GetCookieValue("lang");
        ChangeThreadLocale(lang);
    }

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");
    io->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    io->AddHeader("X-Robots-Tag", "noindex");
    io->AddHeader("Permissions-Policy", "interest-cohort=()");

    // If new base URLs are added besides "/admin", RunCreateInstance() must be modified
    // to forbid the instance key.
    if (TestStr(request.path, "/")) {
        const char *nonce = Fmt(io->Allocator(), "%1", FmtRandom(16)).ptr;

        Span<const char> csp = Fmt(io->Allocator(), "script-src 'self' 'nonce-%1'; "
                                                    "style-src 'self' 'unsafe-inline'; "
                                                    "style-src-elem 'self' 'nonce-%1'; "
                                                    "style-src-attr 'self' 'unsafe-inline'; "
                                                    "frame-ancestors 'none'", nonce);
        io->AddHeader("Content-Security-Policy", csp);
        io->AddHeader("X-Content-Type-Options", "nosniff");
        io->AddHeader("X-Frame-Options", "DENY");

        AttachTemplate(io, *assets_root, [&](Span<const char> expr, StreamWriter *writer) {
            Span<const char> key = TrimStr(expr);

            if (key == "NONCE") {
                writer->Write(nonce);
            } else if (key == "STATIC_URL") {
                Print(writer, "/admin/static/%1/", shared_etag);
            } else if (key == "VERSION") {
                writer->Write(FelixVersion);
            } else if (key == "COMPILER") {
                writer->Write(FelixCompiler);
            } else if (key == "DEMO") {
                writer->Write(gp_config.demo_mode ? "true" : "false");
            } else {
                Print(writer, "{{%1}}", expr);
            }
        });
    } else if (TestStr(request.path, "/favicon.png")) {
        const AssetInfo *asset = assets_map.FindValue("/favicon.png", nullptr);
        K_ASSERT(asset);

        AttachStatic(io, *asset, 0, shared_etag);
    } else if (StartsWith(request.path, "/admin/") || TestStr(request.path, "/admin")) {
        HandleAdminRequest(io);
    } else {
        HandleInstanceRequest(io);
    }
}

static bool PruneOldFiles(const char *dirname, const char *filter, bool recursive, int64_t max_age,
                          int64_t *out_max_mtime = nullptr)
{
    BlockAllocator temp_alloc;

    int64_t threshold = GetUnixTime() - max_age;
    int64_t max_mtime = 0;
    bool complete = true;

    EnumerateDirectory(dirname, nullptr, -1, [&](const char *basename, FileType) {
        const char *filename = Fmt(&temp_alloc, "%1%/%2", dirname, basename).ptr;

        FileInfo file_info;
        if (StatFile(filename, &file_info) != StatResult::Success) {
            complete = false;
            return true;
        }

        switch (file_info.type) {
            case FileType::Directory: {
                if (recursive) {
                    if (PruneOldFiles(filename, filter, true, max_age, &max_mtime)) {
                        LogInfo("Prune old directory '%1'", filename);
                        complete &= UnlinkDirectory(filename);
                    } else {
                        complete = false;
                    }
                } else {
                    complete = false;
                }
            } break;
            case FileType::File: {
                if (!filter || MatchPathName(basename, filter)) {
                    if (file_info.mtime < threshold) {
                        LogInfo("Prune old file '%1'", filename);
                        complete &= UnlinkFile(filename);
                    } else {
                        max_mtime = std::max(max_mtime, file_info.mtime);
                        complete = false;
                    }
                } else {
                    complete = false;
                }
            } break;

            case FileType::Device:
            case FileType::Link:
            case FileType::Pipe:
            case FileType::Socket: {
                // Should not happen, don't touch this crap
                LogDebug("Unexpected non-regular file '%1'", filename);
                complete = false;
            } break;
        }

        return true;
    });

    if (out_max_mtime) {
        *out_max_mtime = max_mtime;
    }
    return complete;
}

void PruneDemos()
{
    // Best effort
    gp_db.Transaction([&] {
        int64_t treshold = GetUnixTime() - DemoPruneDelay;

        if (!gp_db.Run("DELETE FROM dom_instances WHERE demo IS NOT NULL AND demo < ?1", treshold))
            return false;
        if (!sqlite3_changes(gp_db))
            return true;

        SyncDomain(false);
        return true;
    });
}

static bool OpenMainDatabase()
{
    K_DEFER_N(db_guard) { gp_db.Close(); };

    // Open the database
    if (!gp_db.Open(gp_config.database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return false;
    if (!gp_db.SetWAL(true))
        return false;

    // Check schema version
    {
        int version;
        if (!gp_db.GetUserVersion(&version))
            return false;

        if (version > DomainVersion) {
            LogError("Domain schema is too recent (%1, expected %2)", version, DomainVersion);
            return false;
        } else if (version < DomainVersion) {
            if (!MigrateDomain(&gp_db, gp_config.instances_directory))
                return false;
        }
    }

    if (gp_config.use_snapshots && !gp_db.SetSnapshotDirectory(gp_config.snapshot_directory, FullSnapshotDelay))
        return 1;

    db_guard.Disable();
    return true;
}

static bool PerformDuties(bool first)
{
    DomainHolder *domain = RefDomain();
    K_DEFER { UnrefDomain(domain); };

    if (!domain)
        return true;

    if (gp_config.demo_mode) {
        LogDebug("Prune demos");
        PruneDemos();
    }

    // In theory, all temporary files are deleted. But if any remain behind (crash, etc.)
    // we need to make sure they get deleted eventually.
    LogDebug("Prune temporary files");
    PruneOldFiles(gp_config.database_directory, "*.tmp", false, first ? 0 : 7200 * 1000);
    PruneOldFiles(gp_config.tmp_directory, nullptr, true, first ? 0 : 7200 * 1000);
    PruneOldFiles(gp_config.snapshot_directory, "*.tmp", false, first ? 0 : 7200 * 1000);
    PruneOldFiles(gp_config.archive_directory, "*.tmp", false, first ? 0 : 7200 * 1000);
    PruneOldFiles(gp_config.export_directory, "*.tmp", false, first ? 0 : 7200 * 1000);

    LogDebug("Prune old snapshot files");
    PruneOldFiles(gp_config.snapshot_directory, nullptr, true, 3 * 86400 * 1000);

    LogDebug("Prune old archives");
    if (domain) {
        int64_t time = GetUnixTime();
        int64_t snapshot = 0;

        if (domain->settings.archive_retention > 0) {
            PruneOldFiles(gp_config.archive_directory, "*.goupilearchive", false,
                          domain->settings.archive_retention * 86400 * 1000);
            PruneOldFiles(gp_config.archive_directory, "*.goarch", false,
                          domain->settings.archive_retention * 86400 * 1000, &snapshot);
        }

        if (domain->settings.archive_hour >= 0) {
            TimeSpec spec = DecomposeTimeLocal(time);

            if (spec.hour == domain->settings.archive_hour && time - snapshot > 2 * 3600 * 1000) {
                LogInfo("Creating daily snapshot");
                if (!ArchiveDomain())
                    return false;
            } else if (time - snapshot > 25 * 3600 * 1000) {
                LogInfo("Creating forced snapshot (previous one is old)");
                if (!ArchiveDomain())
                    return false;
            }
        }
    }

    // Make sure data loss (if it happens) is very limited in time.
    // If it fails, exit; something is really wrong and we don't fake to it.
    LogDebug("Checkpoint databases");
    if (!gp_db.Checkpoint())
        return false;
    if (domain && !domain->Checkpoint())
        return false;

    LogDebug("Prune domain");
    PruneDomain();

    LogDebug("Prune sessions");
    PruneSessions();

    LogDebug("Check zygote");
    if (!CheckZygote())
        return false;

    return true;
}

static int RunServe(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    const char *config_filename = "goupile.ini";
    bool sandbox = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 [serve] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: %2)%!0

    %!..+-p, --port port%!0                Change web server port
                                   %!D..(default: %3)%!0
        %!..+--bind IP%!0                  Bind to specific IP

        %!..+--sandbox%!0                  Run sandboxed (on supported platforms)

Other commands:

    %!..+init%!0                           Create empty new domain
    %!..+keys%!0                           Generate archive key pairs
    %!..+unseal%!0                         Unseal domain archive

For help about those commands, type: %!..+%1 command --help%!0)"),
                FelixTarget, config_filename, gp_config.http.port);
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
                    config_filename = Fmt(&temp_alloc, "%1%/goupile.ini", TrimStrRight(opt.current_value, K_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

#if !defined(_WIN32)
    // Increase maximum number of open file descriptors
    RaiseMaximumOpenFiles(4096);
#endif

    LogInfo("Init config");
    if (!LoadConfig(config_filename, &gp_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-p", "--port", OptionType::Value)) {
                if (!gp_config.http.SetPortOrPath(opt.current_value))
                    return 1;
            } else if (opt.Test("--bind", OptionType::Value)) {
                gp_config.http.bind_addr = opt.current_value;
            } else if (opt.Test("--sandbox")) {
                sandbox = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();

        // We may have changed some stuff (such as HTTP port), so revalidate
        if (!gp_config.Validate())
            return 1;
    }

    LogInfo("Init assets");
    InitAssets();

    LogInfo("Init directories");
    if (!MakeDirectory(gp_config.instances_directory, false))
        return false;
    if (!MakeDirectory(gp_config.tmp_directory, false))
        return false;
    if (!MakeDirectory(gp_config.archive_directory, false))
        return false;
    if (!MakeDirectory(gp_config.snapshot_directory, false))
        return false;
    if (!MakeDirectory(gp_config.view_directory, false))
        return false;
    if (!MakeDirectory(gp_config.export_directory, false))
        return false;

    // Make sure tmp and instances live on the same volume, because we need to
    // perform atomic renames in some cases.
    {
        const char *tmp_filename1 = CreateUniqueFile(gp_config.tmp_directory, nullptr, ".tmp", &temp_alloc);
        if (!tmp_filename1)
            return 1;
        K_DEFER { UnlinkFile(tmp_filename1); };

        const char *tmp_filename2 = CreateUniqueFile(gp_config.instances_directory, "", ".tmp", &temp_alloc);
        if (!tmp_filename2)
            return 1;
        K_DEFER { UnlinkFile(tmp_filename2); };

        if (RenameFile(tmp_filename1, tmp_filename2, (int)RenameFlag::Overwrite) != RenameResult::Success)
            return 1;
    }

    // We need to bind the socket before sandboxing
    LogInfo("Init HTTP server");
    http_Daemon daemon;
    if (!daemon.Bind(gp_config.http))
        return 1;

#if defined(__linux__)
    if (!NotifySystemd())
        return 1;
#endif

    LogInfo("Init zygote");
    {
        ZygoteResult ret = RunZygote(sandbox, gp_config.view_directory);

        switch (ret) {
            case ZygoteResult::Parent: {} break;

            case ZygoteResult::Child: return 0;
            case ZygoteResult::Error: return 1;
        }
    }

    // Apply sandbox
    if (sandbox) {
        LogInfo("Init sandbox");

        // We use temp_store = MEMORY but, just in case...
        sqlite3_temp_directory = sqlite3_mprintf("%s", gp_config.tmp_directory);

        const char *const reveals[] = {
#if defined(FELIX_HOT_ASSETS)
            // Needed for asset module
            GetApplicationDirectory(),
#endif
            gp_config.database_directory,
            gp_config.archive_directory,
            gp_config.snapshot_directory,
            gp_config.tmp_directory,
            gp_config.view_directory
        };

        if (!ApplySandbox(reveals))
            return 1;
    }

    LogInfo("Init main database");
    if (!OpenMainDatabase())
        return 1;

    LogInfo("Init domain");
    if (!InitDomain())
        return 1;
    K_DEFER { CloseDomain(); };

    // From here on, don't quit abruptly
    // Trigger a check when something happens to the zygote process
    WaitEvents(0);
    SetSignalHandler(SIGCHLD, [](int) { PostWaitMessage(); });

    // Run!
    if (!daemon.Start(HandleRequest))
        return 1;

    // Run periodic tasks until exit
    int status = 0;
    {
        bool run = true;
        bool first = true;
        int timeout = 180 * 1000;

        // Randomize the delay a bit to reduce situations where all goupile
        // services perform cleanups at the same time and cause a load spike.
        timeout += randombytes_uniform(timeout / 4 + 1);
        LogInfo("Periodic timer set to %1 s", FmtDouble((double)timeout / 1000.0, 1));

        while (run) {
            if (!PerformDuties(first))
                return 1;

            WaitResult ret = WaitEvents(timeout);

            if (ret == WaitResult::Exit) {
                LogInfo("Exit requested");
                run = false;
            } else if (ret == WaitResult::Interrupt) {
                LogInfo("Process interrupted");
                status = 1;
                run = false;
            } else if (ret == WaitResult::Message) {
                LogDebug("Reload domain");
                InitDomain();
            }

            first = false;
        }
    }

    LogInfo("Stop zygote");
    StopZygote();

    LogInfo("Stop HTTP server");
    daemon.Stop();

    return status;
}

int Main(int argc, char **argv)
{
    InitLocales(TranslationTables, "en");

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

    if (TestStr(cmd, "serve")) {
        return RunServe(arguments);
    } else if (TestStr(cmd, "init")) {
        return RunInit(arguments);
    } else if (TestStr(cmd, "keys")) {
        return RunKeys(arguments);
    } else if (TestStr(cmd, "unseal")) {
        return RunUnseal(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
