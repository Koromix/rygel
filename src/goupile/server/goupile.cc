// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "admin.hh"
#include "domain.hh"
#include "file.hh"
#include "goupile.hh"
#include "instance.hh"
#include "message.hh"
#include "record.hh"
#include "user.hh"
#include "vm.hh"
#include "../legacy/records.hh"
#include "src/core/http/http.hh"
#include "src/core/request/curl.hh"
#include "src/core/sandbox/sandbox.hh"
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

namespace RG {

struct RenderInfo {
    const char *key;
    AssetInfo asset;
    int64_t time;

    RG_HASHTABLE_HANDLER(RenderInfo, key);
};

DomainHolder gp_domain;

static HashMap<const char *, const AssetInfo *> assets_map;
static const AssetInfo *assets_root;
static HeapArray<const char *> assets_for_cache;
static BlockAllocator assets_alloc;
static char shared_etag[17];

static std::shared_mutex render_mutex;
static BucketArray<RenderInfo, 8> render_cache;
static HashTable<const char *, RenderInfo *> render_map;

static const int64_t MaxRenderDelay = 20 * 60000;

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
        { "readlink", sb_FilterAction::Allow }
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
        FillRandomSafe(&buf, RG_SIZE(buf));
        Fmt(shared_etag, "%1", FmtHex(buf).Pad0(-16));
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
        } else if (TestStr(asset.name, "src/goupile/client/images/favicon.png")) {
            assets_map.Set("/favicon.png", &asset);
            assets_for_cache.Append("/favicon.png");
        } else if (TestStr(asset.name, "src/goupile/client/images/admin.png")) {
            assets_map.Set("/admin/favicon.png", &asset);
        } else if (StartsWith(asset.name, "src/goupile/client/") ||
                   StartsWith(asset.name, "vendor/opensans/")) {
            const char *name = SplitStrReverseAny(asset.name, RG_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&assets_alloc, "/static/%1/%2", shared_etag, name).ptr;

            assets_map.Set(url, &asset);
            assets_for_cache.Append(url);
        } else if (StartsWith(asset.name, "vendor/")) {
            Span<const char> library = SplitStr(asset.name + 7, '/');

            const char *name = SplitStrReverseAny(asset.name, RG_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&assets_alloc, "/static/%1/%2/%3", shared_etag, library, name).ptr;

            assets_map.Set(url, &asset);
            assets_for_cache.Append(url);
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

static void HandlePing(http_IO *io, InstanceHolder *instance)
{
    // Do this to renew session and clear invalid session cookies
    GetNormalSession(io, instance);

    io->AddCachingHeaders(0, nullptr);
    io->SendText(200, "{}", "application/json");
}

static void HandleFileStatic(http_IO *io, InstanceHolder *instance)
{
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    const char *base_url = Fmt(io->Allocator(), "/%1/", instance->key).ptr;

    json.StartArray();
    json.String(base_url);
    for (const InstanceHolder *slave: instance->slaves) {
        const char *url = Fmt(io->Allocator(), "/%1/", slave->key).ptr;
        json.String(url);
    }
    for (const char *path: assets_for_cache) {
        const char *url = Fmt(io->Allocator(), "/%1%2", instance->key, path).ptr;
        json.String(url);
    }
    json.EndArray();

    json.Finish();
}

static const AssetInfo *RenderTemplate(const char *key, const AssetInfo &asset,
                                       FunctionRef<void(Span<const char>, StreamWriter *)> func)
{
    RenderInfo *render;
    {
        std::shared_lock<std::shared_mutex> lock_shr(render_mutex);
        render = render_map.FindValue(key, nullptr);
    }

    if (!render) {
        std::lock_guard<std::shared_mutex> lock_excl(render_mutex);
        render = render_map.FindValue(key, nullptr);

        if (!render) {
            Allocator *alloc;
            render = render_cache.AppendDefault(&alloc);

            render->key = DuplicateString(key, alloc).ptr;
            render->asset = asset;
            render->asset.data = PatchFile(asset, alloc, func);
            render->time = GetMonotonicTime();

            render_map.Set(render);

            LogDebug("Rendered '%1' with '%2'", key, asset.name);
        }
    }

    return &render->asset;
}

static void PruneRenders()
{
    std::lock_guard lock_excl(render_mutex);

    int64_t now = GetMonotonicTime();

    Size expired = 0;
    for (const RenderInfo &render: render_cache) {
        if (now - render.time < MaxRenderDelay)
            break;

        render_map.Remove(render.key);
        expired++;
    }

    render_cache.RemoveFirst(expired);

    render_cache.Trim();
    render_map.Trim();
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
    RG_ASSERT(StartsWith(request.path, "/admin/") || TestStr(request.path, "/admin"));

    const char *admin_url = request.path + 6;

    // Missing trailing slash, redirect
    if (!admin_url[0]) {
        const char *redirect = Fmt(io->Allocator(), "%1/", request.path).ptr;

        io->AddHeader("Location", redirect);
        io->SendEmpty(302);

        return;
    }

    // Try static assets
    {
        if (TestStr(admin_url, "/")) {
            const AssetInfo *asset = assets_map.FindValue(admin_url, nullptr);
            RG_ASSERT(asset);

            const AssetInfo *render = RenderTemplate(request.path, *asset,
                                                     [&](Span<const char> expr, StreamWriter *writer) {
                Span<const char> key = TrimStr(expr);

                if (key == "VERSION") {
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
                    json_Writer json(writer);
                    char buf[128];

                    json.StartObject();
                    json.Key("key"); json.String("admin");
                    json.Key("urls"); json.StartObject();
                        json.Key("base"); json.String("/admin/");
                        json.Key("instance"); json.String("/admin/");
                        json.Key("static"); json.String(Fmt(buf, "/admin/static/%1/", shared_etag).ptr);
                    json.EndObject();
                    json.Key("title"); json.String("Admin");
                    json.Key("permissions"); json.StartObject();
                    for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
                        bool legacy = LegacyPermissionMask & (1 << i);

                        Span<const char> str = json_ConvertToJsonName(UserPermissionNames[i], buf);
                        json.Key(str.ptr, (size_t)str.len);
                        json.Bool(legacy);
                    }
                    json.EndObject();
                    json.Key("retention"); json.Int(gp_domain.config.archive_retention);
                    json.EndObject();
                } else if (key == "HEAD_TAGS") {
                    // Nothing to add
                } else {
                    Print(writer, "{{%1}}", expr);
                }
            });
            AttachStatic(io, *render, 0, shared_etag);

            return;
        } else if (TestStr(admin_url, "/favicon.png")) {
            const AssetInfo *asset = assets_map.FindValue("/admin/favicon.png", nullptr);
            RG_ASSERT(asset);

            AttachStatic(io, *asset, 0, shared_etag);

            return;
        } else {
            const AssetInfo *asset = assets_map.FindValue(admin_url, nullptr);

            if (asset) {
                int64_t max_age = StartsWith(admin_url, "/static/") ? (365ll * 86400000) : 0;
                AttachStatic(io, *asset, max_age, shared_etag);

                return;
            }
        }
    }

    // CSRF protection
    if (request.method != http_RequestMethod::Get && !http_PreventCSRF(io))
        return;

    // And now, API endpoints
    if (TestStr(admin_url, "/api/session/ping") && request.method == http_RequestMethod::Get) {
        HandlePing(io, nullptr);
    } else if (TestStr(admin_url, "/api/session/profile") && request.method == http_RequestMethod::Get) {
        HandleSessionProfile(io, nullptr);
    } else if (TestStr(admin_url, "/api/session/login") && request.method == http_RequestMethod::Post) {
        HandleSessionLogin(io, nullptr);
    } else if (TestStr(admin_url, "/api/session/confirm") && request.method == http_RequestMethod::Post) {
        HandleSessionConfirm(io, nullptr);
    } else if (TestStr(admin_url, "/api/session/logout") && request.method == http_RequestMethod::Post) {
        HandleSessionLogout(io);
    } else if (TestStr(admin_url, "/api/change/password") && request.method == http_RequestMethod::Post) {
        HandleChangePassword(io, nullptr);
    } else if (TestStr(admin_url, "/api/change/qrcode") && request.method == http_RequestMethod::Get) {
        HandleChangeQRcode(io);
    } else if (TestStr(admin_url, "/api/change/totp") && request.method == http_RequestMethod::Post) {
        HandleChangeTOTP(io);
    } else if (TestStr(admin_url, "/api/demo/create") && request.method == http_RequestMethod::Post) {
        HandleDemoCreate(io);
    } else if (TestStr(admin_url, "/api/instances/create") && request.method == http_RequestMethod::Post) {
        HandleInstanceCreate(io);
    } else if (TestStr(admin_url, "/api/instances/delete") && request.method == http_RequestMethod::Post) {
        HandleInstanceDelete(io);
    } else if (TestStr(admin_url, "/api/instances/migrate") && request.method == http_RequestMethod::Post) {
        HandleInstanceMigrate(io);
    } else if (TestStr(admin_url, "/api/instances/configure") && request.method == http_RequestMethod::Post) {
        HandleInstanceConfigure(io);
    } else if (TestStr(admin_url, "/api/instances/list") && request.method == http_RequestMethod::Get) {
        HandleInstanceList(io);
    } else if (TestStr(admin_url, "/api/instances/assign") && request.method == http_RequestMethod::Post) {
        HandleInstanceAssign(io);
    } else if (TestStr(admin_url, "/api/instances/permissions") && request.method == http_RequestMethod::Get) {
        HandleInstancePermissions(io);
    } else if (TestStr(admin_url, "/api/archives/create") && request.method == http_RequestMethod::Post) {
        HandleArchiveCreate(io);
    } else if (TestStr(admin_url, "/api/archives/delete") && request.method == http_RequestMethod::Post) {
        HandleArchiveDelete(io);
    } else if (TestStr(admin_url, "/api/archives/list") && request.method == http_RequestMethod::Get) {
        HandleArchiveList(io);
    } else if (StartsWith(admin_url, "/api/archives/files") && request.method == http_RequestMethod::Get) {
        HandleArchiveDownload(io);
    } else if (StartsWith(admin_url, "/api/archives/files") && request.method == http_RequestMethod::Put) {
        HandleArchiveUpload(io);
    } else if (TestStr(admin_url, "/api/archives/restore") && request.method == http_RequestMethod::Post) {
        HandleArchiveRestore(io);
    } else if (TestStr(admin_url, "/api/users/create") && request.method == http_RequestMethod::Post) {
        HandleUserCreate(io);
    } else if (TestStr(admin_url, "/api/users/edit") && request.method == http_RequestMethod::Post) {
        HandleUserEdit(io);
    } else if (TestStr(admin_url, "/api/users/delete") && request.method == http_RequestMethod::Post) {
        HandleUserDelete(io);
    } else if (TestStr(admin_url, "/api/users/list") && request.method == http_RequestMethod::Get) {
        HandleUserList(io);
    } else if (TestStr(admin_url, "/api/send/mail") && request.method == http_RequestMethod::Post) {
        HandleSendMail(io, nullptr);
    } else if (TestStr(admin_url, "/api/send/sms") && request.method == http_RequestMethod::Post) {
        HandleSendSMS(io, nullptr);
#if !defined(_WIN32)
    } else if (TestStr(admin_url, "/api/process/exit") && request.method == http_RequestMethod::Post) {
        HandleProcessSignal(io, SIGTERM);
    } else if (TestStr(admin_url, "/api/process/interrupt") && request.method == http_RequestMethod::Post) {
        HandleProcessSignal(io, SIGINT);
#endif
    } else {
        io->SendError(404);
    }

    // Send internal error details to root users for debug
    if (!io->HasResponded()) {
        RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

        if (session && session->IsRoot()) {
            const char *msg = io->LastError();
            io->SendError(500, msg);
        }
    }
}

static void EncodeUrlSafe(Span<const char> str, const char *passthrough, HeapArray<char> *out_buf)
{
    for (char c: str) {
        if (IsAsciiAlphaOrDigit(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            out_buf->Append((char)c);
        } else if (passthrough && strchr(passthrough, c)) {
            out_buf->Append((char)c);
        } else {
            Fmt(out_buf, "%%%1", FmtHex((uint8_t)c).Pad0(-2));
        }
    }

    out_buf->Grow(1);
    out_buf->ptr[out_buf->len] = 0;
}

static void HandleInstanceRequest(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    InstanceHolder *instance = nullptr;
    const char *instance_url = request.path;

    // Find relevant instance
    for (int i = 0; i < 2 && instance_url[0]; i++) {
        Size offset = SplitStr(instance_url + 1, '/').len + 1;

        const char *new_url = instance_url + offset;
        Span<const char> new_key = MakeSpan(request.path + 1, new_url - request.path - 1);

        InstanceHolder *ref = gp_domain.Ref(new_key);
        if (!ref)
            break;

        if (instance) {
            instance->Unref();
        }
        instance = ref;
        instance_url = new_url;

        // No need to look further
        if (!instance->slaves.len)
            break;
    }
    if (!instance) {
        io->SendError(404);
        return;
    }
    RG_DEFER { instance->Unref(); };

    // Enforce trailing slash on base URLs. Use 302 instead of 301 to avoid
    // problems with query strings being erased without question.
    if (!instance_url[0]) {
        HeapArray<char> buf(io->Allocator());

        Fmt(&buf, "%1/?", request.path);
        for (const http_KeyValue &value: request.values) {
            EncodeUrlSafe(value.key, nullptr, &buf);
            buf.Append('=');
            EncodeUrlSafe(value.value, nullptr, &buf);
            buf.Append('&');
        }
        buf.ptr[buf.len - 1] = 0;

        io->AddHeader("Location", buf.ptr);
        io->SendEmpty(302);

        return;
    }

    // Enable COEP for offlines instances to get SharedArrayBuffer
    if (instance->config.use_offline) {
        io->AddHeader("Cross-Origin-Embedder-Policy", "require-corp");
    }

    // Try application files
    if (request.method == http_RequestMethod::Get && HandleFileGet(io, instance))
        return;

    // Try static assets
    if (request.method == http_RequestMethod::Get && !StartsWith(instance_url, "/api/")) {
        if (!GetPathExtension(instance_url).len) {
            instance_url = "/";
        }

        const AssetInfo *asset = assets_map.FindValue(instance_url, nullptr);

        if (TestStr(instance_url, "/") || TestStr(instance_url, "/sw.js") ||
                                          TestStr(instance_url, "/sw.pk.js") ||
                                          TestStr(instance_url, "/manifest.json")) {
            RG_ASSERT(asset);

            const InstanceHolder *master = instance->master;
            int64_t fs_version = master->fs_version.load(std::memory_order_relaxed);

            char instance_etag[64];
            Fmt(instance_etag, "%1_%2_%3_%4", shared_etag, (const void *)asset, instance->unique, fs_version);

            const AssetInfo *render = RenderTemplate(instance_etag, *asset,
                                                     [&](Span<const char> expr, StreamWriter *writer) {
                Span<const char> key = TrimStr(expr);

                if (key == "VERSION") {
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
                    json_Writer json(writer);
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
                    json.Key("legacy"); json.Bool(master->legacy);
                    json.Key("demo"); json.Bool(instance->demo);
                    json.Key("version"); json.Int64(fs_version);
                    json.Key("buster"); json.String(instance_etag);
                    json.Key("use_offline"); json.Bool(master->config.use_offline);
                    json.Key("data_remote"); json.Bool(master->config.data_remote);
                    if (master->config.auto_key) {
                        json.Key("auto_key"); json.String(master->config.auto_key);
                    }
                    json.EndObject();
                } else if (key == "HEAD_TAGS") {
                    if (master->config.use_offline) {
                        Print(writer, "<link rel=\"manifest\" href=\"/%1/manifest.json\"/>", master->key);
                    }
                } else {
                    Print(writer, "{{%1}}", expr);
                }
            });
            AttachStatic(io, *render, 0, instance_etag);

            return;
        } else if (asset) {
            int64_t max_age = StartsWith(instance_url, "/static/") ? (365ll * 86400000) : 0;
            AttachStatic(io, *asset, max_age, shared_etag);

            return;
        }
    }

    // CSRF protection
    if (request.method != http_RequestMethod::Get && !http_PreventCSRF(io))
        return;

    // And now, API endpoints
    if (TestStr(instance_url, "/api/session/ping") && request.method == http_RequestMethod::Get) {
        HandlePing(io, instance);
    } else if (TestStr(instance_url, "/api/session/profile") && request.method == http_RequestMethod::Get) {
        HandleSessionProfile(io, instance);
    } else if (TestStr(instance_url, "/api/session/login") && request.method == http_RequestMethod::Post) {
        HandleSessionLogin(io, instance);
    } else if (TestStr(instance_url, "/api/session/token") && request.method == http_RequestMethod::Post) {
        HandleSessionToken(io, instance);
    } else if (TestStr(instance_url, "/api/session/key") && request.method == http_RequestMethod::Post) {
        HandleSessionKey(io, instance);
    } else if (TestStr(instance_url, "/api/session/confirm") && request.method == http_RequestMethod::Post) {
        HandleSessionConfirm(io, instance);
    } else if (TestStr(instance_url, "/api/session/logout") && request.method == http_RequestMethod::Post) {
        HandleSessionLogout(io);
    } else if (TestStr(instance_url, "/api/change/password") && request.method == http_RequestMethod::Post) {
        HandleChangePassword(io, instance);
    } else if (TestStr(instance_url, "/api/change/qrcode") && request.method == http_RequestMethod::Get) {
        HandleChangeQRcode(io);
    } else if (TestStr(instance_url, "/api/change/totp") && request.method == http_RequestMethod::Post) {
        HandleChangeTOTP(io);
    } else if (TestStr(instance_url, "/api/change/mode") && request.method == http_RequestMethod::Post) {
        HandleChangeMode(io, instance);
    } else if (TestStr(instance_url, "/api/change/export_key") && request.method == http_RequestMethod::Post) {
        HandleChangeExportKey(io, instance);
    } else if (TestStr(instance_url, "/api/files/static") && request.method == http_RequestMethod::Get) {
        HandleFileStatic(io, instance);
    } else if (TestStr(instance_url, "/api/files/list") && request.method == http_RequestMethod::Get) {
        HandleFileList(io, instance);
    } else if (StartsWith(instance_url, "/files/") && request.method == http_RequestMethod::Put) {
        HandleFilePut(io, instance);
    } else if (StartsWith(instance_url, "/files/") && request.method == http_RequestMethod::Delete) {
        HandleFileDelete(io, instance);
    } else if (StartsWith(instance_url, "/api/files/history") && request.method == http_RequestMethod::Get) {
        HandleFileHistory(io, instance);
    } else if (StartsWith(instance_url, "/api/files/restore") && request.method == http_RequestMethod::Post) {
        HandleFileRestore(io, instance);
    } else if (StartsWith(instance_url, "/api/files/delta") && request.method == http_RequestMethod::Get) {
        HandleFileDelta(io, instance);
    } else if (StartsWith(instance_url, "/api/files/publish") && request.method == http_RequestMethod::Post) {
        HandleFilePublish(io, instance);
    } else if (TestStr(instance_url, "/api/records/list") && request.method == http_RequestMethod::Get) {
        HandleRecordList(io, instance);
    } else if (TestStr(instance_url, "/api/records/get") && request.method == http_RequestMethod::Get) {
        HandleRecordGet(io, instance);
    } else if (!instance->legacy && TestStr(instance_url, "/api/records/audit") && request.method == http_RequestMethod::Get) {
        HandleRecordAudit(io, instance);
    } else if (!instance->legacy && TestStr(instance_url, "/api/records/save") && request.method == http_RequestMethod::Post) {
        HandleRecordSave(io, instance);
    } else if (!instance->legacy && TestStr(instance_url, "/api/records/delete") && request.method == http_RequestMethod::Post) {
        HandleRecordDelete(io, instance);
    } else if (!instance->legacy && TestStr(instance_url, "/api/records/lock") && request.method == http_RequestMethod::Post) {
        HandleRecordLock(io, instance);
    } else if (!instance->legacy && TestStr(instance_url, "/api/records/unlock") && request.method == http_RequestMethod::Post) {
        HandleRecordUnlock(io, instance);
    } else if (!instance->legacy && StartsWith(instance_url, "/blobs/") && request.method == http_RequestMethod::Get) {
        HandleBlobGet(io, instance);
    } else if (!instance->legacy && TestStr(instance_url, "/api/records/blob") && request.method == http_RequestMethod::Post) {
        HandleBlobPost(io, instance);
    } else if (!instance->legacy && TestStr(instance_url, "/api/export/create") && request.method == http_RequestMethod::Post) {
        HandleExportCreate(io, instance);
    } else if (!instance->legacy && TestStr(instance_url, "/api/export/list") && request.method == http_RequestMethod::Get) {
        HandleExportList(io, instance);
    } else if (!instance->legacy && TestStr(instance_url, "/api/export/download") && request.method == http_RequestMethod::Get) {
        HandleExportDownload(io, instance);
    } else if (instance->legacy && TestStr(instance_url, "/api/records/load") && request.method == http_RequestMethod::Get) {
        HandleLegacyLoad(io, instance);
    } else if (instance->legacy && TestStr(instance_url, "/api/records/save") && request.method == http_RequestMethod::Post) {
        HandleLegacySave(io, instance);
    } else if (instance->legacy && TestStr(instance_url, "/api/records/export") && request.method == http_RequestMethod::Get) {
        HandleLegacyExport(io, instance);
    } else if (TestStr(instance_url, "/api/send/mail") && request.method == http_RequestMethod::Post) {
        HandleSendMail(io, instance);
    } else if (TestStr(instance_url, "/api/send/sms") && request.method == http_RequestMethod::Post) {
        HandleSendSMS(io, instance);
    } else if (TestStr(instance_url, "/api/send/tokenize") && request.method == http_RequestMethod::Post) {
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

            render_cache.Clear();
            render_map.Clear();
        }
    }
#endif

    if (gp_domain.config.require_host) {
        const char *host = request.GetHeaderValue("Host");

        if (!host) {
            LogError("Request is missing required Host header");
            io->SendError(400);
            return;
        }
        if (!TestStr(host, gp_domain.config.require_host)) {
            LogError("Unexpected Host header '%1'", host);
            io->SendError(403);
            return;
        }
    }

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");
    io->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    io->AddHeader("X-Robots-Tag", "noindex");
    io->AddHeader("Permissions-Policy", "interest-cohort=()");

    // If new base URLs are added besides "/admin", RunCreateInstance() must be modified
    // to forbid the instance key.
    if (TestStr(request.path, "/")) {
        const AssetInfo *render = RenderTemplate("/", *assets_root,
                                                 [&](Span<const char> expr, StreamWriter *writer) {
            Span<const char> key = TrimStr(expr);

            if (key == "STATIC_URL") {
                Print(writer, "/admin/static/%1/", shared_etag);
            } else if (key == "VERSION") {
                writer->Write(FelixVersion);
            } else if (key == "COMPILER") {
                writer->Write(FelixCompiler);
            } else if (key == "DEMO") {
                writer->Write(gp_domain.config.demo_mode ? "true" : "false");
            } else {
                Print(writer, "{{%1}}", expr);
            }
        });
        AttachStatic(io, *render, 0, shared_etag);
    } else if (TestStr(request.path, "/favicon.png")) {
        const AssetInfo *asset = assets_map.FindValue("/favicon.png", nullptr);
        RG_ASSERT(asset);

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

static int RunServe(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    const char *config_filename = "goupile.ini";
    bool sandbox = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [serve] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: %2)%!0

    %!..+-p, --port port%!0                Change web server port
                                   %!D..(default: %3)%!0
        %!..+--sandbox%!0                  Run sandboxed (on supported platforms)

Other commands:

    %!..+init%!0                           Create new domain
    %!..+migrate%!0                        Migrate existing domain
    %!..+keys%!0                           Generate archive key pairs
    %!..+unseal%!0                         Unseal domain archive

For help about those commands, type: %!..+%1 command --help%!0)",
                FelixTarget, config_filename, gp_domain.config.http.port);
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
                    config_filename = Fmt(&temp_alloc, "%1%/goupile.ini", TrimStrRight(opt.current_value, RG_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else if (opt.Test("--sandbox")) {
                sandbox = true;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

#if !defined(_WIN32)
    // Increase maximum number of open file descriptors
    RaiseMaximumOpenFiles(4096);
#endif

    LogInfo("Init assets");
    InitAssets();

    LogInfo("Init domain");
    if (!gp_domain.Open(config_filename))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-p", "--port", OptionType::Value)) {
                if (!gp_domain.config.http.SetPortOrPath(opt.current_value))
                    return 1;
            } else if (opt.Test("--sandbox")) {
                // Already handled
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();

        // We may have changed some stuff (such as HTTP port), so revalidate
        if (!gp_domain.config.Validate())
            return 1;
    }

    LogInfo("Init messaging");
    if (gp_domain.config.sms.provider != sms_Provider::None && !InitSMS(gp_domain.config.sms))
        return 1;
    if (gp_domain.config.smtp.url && !InitSMTP(gp_domain.config.smtp))
        return 1;

    // We need to bind the socket before sandboxing
    LogInfo("Init HTTP server");
    http_Daemon daemon;
    if (!daemon.Bind(gp_domain.config.http))
        return 1;

#if defined(__linux__)
    if (!NotifySystemd())
        return 1;
#endif

    LogInfo("Init zygote");
    {
        ZygoteResult ret = RunZygote(sandbox, gp_domain.config.view_directory);

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
        sqlite3_temp_directory = sqlite3_mprintf("%s", gp_domain.config.tmp_directory);

        const char *const reveal_paths[] = {
#if defined(FELIX_HOT_ASSETS)
            // Needed for asset module
            GetApplicationDirectory(),
#endif
            gp_domain.config.database_directory,
            gp_domain.config.archive_directory,
            gp_domain.config.snapshot_directory,
            gp_domain.config.tmp_directory,
            gp_domain.config.view_directory
        };
        const char *const mask_files[] = {
            gp_domain.config.config_filename
        };

        if (!ApplySandbox(reveal_paths, mask_files))
            return 1;
    }

    LogInfo("Init instances");
    if (!gp_domain.SyncAll())
        return 1;

    // From here on, don't quit abruptly
    // Trigger a check when something happens to the zygote process
    WaitForInterrupt(0);
    SetSignalHandler(SIGCHLD, [](int) { SignalWaitFor(); });

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
            if (gp_domain.config.demo_mode) {
                LogDebug("Prune demos");
                PruneDemos();
            }

            // In theory, all temporary files are deleted. But if any remain behind (crash, etc.)
            // we need to make sure they get deleted eventually.
            LogDebug("Prune temporary files");
            PruneOldFiles(gp_domain.config.database_directory, "*.tmp", false, first ? 0 : 7200 * 1000);
            PruneOldFiles(gp_domain.config.tmp_directory, nullptr, true, first ? 0 : 7200 * 1000);
            PruneOldFiles(gp_domain.config.snapshot_directory, "*.tmp", false, first ? 0 : 7200 * 1000);
            PruneOldFiles(gp_domain.config.archive_directory, "*.tmp", false, first ? 0 : 7200 * 1000);

            LogDebug("Prune old snapshot files");
            PruneOldFiles(gp_domain.config.snapshot_directory, nullptr, true, 3 * 86400 * 1000);

            LogDebug("Prune old archives");
            {
                int64_t time = GetUnixTime();
                int64_t snapshot = 0;

                if (gp_domain.config.archive_retention > 0) {
                    PruneOldFiles(gp_domain.config.archive_directory, "*.goupilearchive", false,
                                  gp_domain.config.archive_retention * 86400 * 1000);
                    PruneOldFiles(gp_domain.config.archive_directory, "*.goarch", false,
                                  gp_domain.config.archive_retention * 86400 * 1000, &snapshot);
                }

                if (gp_domain.config.archive_hour >= 0) {
                    TimeSpec spec = gp_domain.config.archive_utc ? DecomposeTimeUTC(time) : DecomposeTimeLocal(time);

                    if (spec.hour == gp_domain.config.archive_hour && time - snapshot > 2 * 3600 * 1000) {
                        LogInfo("Creating daily snapshot");
                        if (!ArchiveDomain())
                            return 1;
                    } else if (time - snapshot > 25 * 3600 * 1000) {
                        LogInfo("Creating forced snapshot (previous one is old)");
                        if (!ArchiveDomain())
                            return 1;
                    }
                }
            }

            // Make sure data loss (if it happens) is very limited in time.
            // If it fails, exit; something is really wrong and we don't fake to it.
            LogDebug("Checkpoint databases");
            if (!gp_domain.Checkpoint())
                return 1;

            WaitForResult ret = WaitForInterrupt(timeout);

            if (ret == WaitForResult::Exit) {
                LogInfo("Exit requested");
                run = false;
            } else if (ret == WaitForResult::Interrupt) {
                LogInfo("Process interrupted");
                status = 1;
                run = false;
            } else if (ret == WaitForResult::Message) {
                LogDebug("Syncing instances");
                gp_domain.SyncAll(true);
            }

            LogDebug("Prune sessions");
            PruneSessions();

            LogDebug("Prune template renders");
            PruneRenders();

            LogDebug("Check zygote");
            if (!CheckZygote())
                return 1;

            first = false;
        }
    }

    LogDebug("Stop zygote");
    StopZygote();

    LogDebug("Stop HTTP server");
    daemon.Stop();

    return status;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

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
            PrintLn("Compiler: %1", FelixCompiler);
            return 0;
        }
    }

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }
    if (curl_global_init(CURL_GLOBAL_ALL)) {
        LogError("Failed to initialize libcurl");
        return 1;
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

    if (TestStr(cmd, "init")) {
        return RunInit(arguments);
    } else if (TestStr(cmd, "migrate")) {
        return RunMigrate(arguments);
    } else if (TestStr(cmd, "keys")) {
        return RunKeys(arguments);
    } else if (TestStr(cmd, "unseal")) {
        return RunUnseal(arguments);
    } else if (TestStr(cmd, "serve")) {
        return RunServe(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
