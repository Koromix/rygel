// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "admin.hh"
#include "domain.hh"
#include "files.hh"
#include "goupile.hh"
#include "instance.hh"
#include "messages.hh"
#include "records.hh"
#include "user.hh"
#include "src/core/libnet/libnet.hh"
#include "src/core/libsandbox/libsandbox.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#include "vendor/curl/include/curl/curl.h"
#ifndef _WIN32
    #include <sys/time.h>
    #include <sys/resource.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
#endif
#ifdef __GLIBC__
    #include <malloc.h>
#endif

namespace RG {

struct RenderInfo {
    const char *url;
    AssetInfo asset;
    int64_t time;

    RG_HASHTABLE_HANDLER(RenderInfo, url);
};

DomainHolder gp_domain;

static HashMap<const char *, const AssetInfo *> assets_map;
static const AssetInfo *assets_root;
static HeapArray<const char *> assets_for_cache;
static LinkedAllocator assets_alloc;
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

#ifdef __linux__
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

    sb.FilterSyscalls(sb_FilterAction::Kill, {
        {"exit", sb_FilterAction::Allow},
        {"exit_group", sb_FilterAction::Allow},
        {"brk", sb_FilterAction::Allow},
        {"mmap/anon", sb_FilterAction::Allow},
        {"mmap/shared", sb_FilterAction::Allow},
        {"munmap", sb_FilterAction::Allow},
        {"mremap", sb_FilterAction::Allow},
        {"mprotect/noexec", sb_FilterAction::Allow},
        {"mlock", sb_FilterAction::Allow},
        {"mlock2", sb_FilterAction::Allow},
        {"mlockall", sb_FilterAction::Allow},
        {"madvise", sb_FilterAction::Allow},
        {"pipe", sb_FilterAction::Allow},
        {"pipe2", sb_FilterAction::Allow},
        {"open", sb_FilterAction::Allow},
        {"openat", sb_FilterAction::Allow},
        {"openat2", sb_FilterAction::Allow},
        {"close", sb_FilterAction::Allow},
        {"fcntl", sb_FilterAction::Allow},
        {"read", sb_FilterAction::Allow},
        {"readv", sb_FilterAction::Allow},
        {"write", sb_FilterAction::Allow},
        {"writev", sb_FilterAction::Allow},
        {"pread64", sb_FilterAction::Allow},
        {"pwrite64", sb_FilterAction::Allow},
        {"lseek", sb_FilterAction::Allow},
        {"ftruncate", sb_FilterAction::Allow},
        {"fsync", sb_FilterAction::Allow},
        {"fdatasync", sb_FilterAction::Allow},
        {"fstat", sb_FilterAction::Allow},
        {"stat", sb_FilterAction::Allow},
        {"lstat", sb_FilterAction::Allow},
        {"lstat64", sb_FilterAction::Allow},
        {"fstatat64", sb_FilterAction::Allow},
        {"newfstatat", sb_FilterAction::Allow},
        {"ioctl/tty", sb_FilterAction::Allow},
        {"getrandom", sb_FilterAction::Allow},
        {"getpid", sb_FilterAction::Allow},
        {"gettid", sb_FilterAction::Allow},
        {"getuid", sb_FilterAction::Allow},
        {"getgid", sb_FilterAction::Allow},
        {"geteuid", sb_FilterAction::Allow},
        {"getegid", sb_FilterAction::Allow},
        {"getcwd", sb_FilterAction::Allow},
        {"rt_sigaction", sb_FilterAction::Allow},
        {"rt_sigpending", sb_FilterAction::Allow},
        {"rt_sigprocmask", sb_FilterAction::Allow},
        {"rt_sigqueueinfo", sb_FilterAction::Allow},
        {"rt_sigreturn", sb_FilterAction::Allow},
        {"rt_sigsuspend", sb_FilterAction::Allow},
        {"rt_sigtimedwait", sb_FilterAction::Allow},
        {"rt_sigtimedwait_time64", sb_FilterAction::Allow},
        {"kill", sb_FilterAction::Allow},
        {"tgkill", sb_FilterAction::Allow},
        {"mkdir", sb_FilterAction::Allow},
        {"mkdirat", sb_FilterAction::Allow},
        {"unlink", sb_FilterAction::Allow},
        {"unlinkat", sb_FilterAction::Allow},
        {"rename", sb_FilterAction::Allow},
        {"renameat", sb_FilterAction::Allow},
        {"renameat2", sb_FilterAction::Allow},
        {"rmdir", sb_FilterAction::Allow},
        {"chown", sb_FilterAction::Allow},
        {"chmod", sb_FilterAction::Allow},
        {"clone/thread", sb_FilterAction::Allow},
        {"futex", sb_FilterAction::Allow},
        {"set_robust_list", sb_FilterAction::Allow},
        {"socket", sb_FilterAction::Allow},
        {"socketpair", sb_FilterAction::Allow},
        {"getsockopt", sb_FilterAction::Allow},
        {"setsockopt", sb_FilterAction::Allow},
        {"getsockname", sb_FilterAction::Allow},
        {"getpeername", sb_FilterAction::Allow},
        {"connect", sb_FilterAction::Allow},
        {"bind", sb_FilterAction::Allow},
        {"listen", sb_FilterAction::Allow},
        {"accept", sb_FilterAction::Allow},
        {"accept4", sb_FilterAction::Allow},
        {"eventfd", sb_FilterAction::Allow},
        {"eventfd2", sb_FilterAction::Allow},
        {"getdents", sb_FilterAction::Allow},
        {"getdents64", sb_FilterAction::Allow},
        {"prctl", sb_FilterAction::Allow},
        {"epoll_create", sb_FilterAction::Allow},
        {"epoll_create1", sb_FilterAction::Allow},
        {"epoll_ctl", sb_FilterAction::Allow},
        {"epoll_pwait", sb_FilterAction::Allow},
        {"epoll_wait", sb_FilterAction::Allow},
        {"poll", sb_FilterAction::Allow},
        {"select", sb_FilterAction::Allow},
        {"clock_nanosleep", sb_FilterAction::Allow},
        {"clock_gettime", sb_FilterAction::Allow},
        {"clock_gettime64", sb_FilterAction::Allow},
        {"clock_nanosleep", sb_FilterAction::Allow},
        {"clock_nanosleep_time64", sb_FilterAction::Allow},
        {"nanosleep", sb_FilterAction::Allow},
        {"sched_yield", sb_FilterAction::Allow},
        {"recv", sb_FilterAction::Allow},
        {"recvfrom", sb_FilterAction::Allow},
        {"recvmmsg", sb_FilterAction::Allow},
        {"recvmmsg_time64", sb_FilterAction::Allow},
        {"recvmsg", sb_FilterAction::Allow},
        {"sendmsg", sb_FilterAction::Allow},
        {"sendmmsg", sb_FilterAction::Allow},
        {"sendfile", sb_FilterAction::Allow},
        {"sendfile64", sb_FilterAction::Allow},
        {"sendto", sb_FilterAction::Allow},
        {"shutdown", sb_FilterAction::Allow},
        {"uname", sb_FilterAction::Allow},
        {"utime", sb_FilterAction::Allow}
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

    for (const AssetInfo &asset: GetPackedAssets()) {
        if (TestStr(asset.name, "src/goupile/client/goupile.html")) {
            assets_map.Set("/", &asset);
            assets_for_cache.Append("/");
        } else if (TestStr(asset.name, "src/goupile/client/root.html")) {
            assets_root = &asset;
        } else if (TestStr(asset.name, "src/goupile/client/sw.pk.js")) {
            assets_map.Set("/sw.pk.js", &asset);
        } else if (TestStr(asset.name, "src/goupile/client/manifest.json")) {
            assets_map.Set("/manifest.json", &asset);
        } else if (TestStr(asset.name, "src/goupile/client/images/favicon.png")) {
            assets_map.Set("/favicon.png", &asset);
            assets_for_cache.Append("/favicon.png");
        } else if (TestStr(asset.name, "src/goupile/client/images/admin.png")) {
            assets_map.Set("/admin/favicon.png", &asset);
        } else if (StartsWith(asset.name, "src/goupile/client/") ||
                   StartsWith(asset.name, "vendor/")) {
            const char *name = SplitStrReverseAny(asset.name, RG_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&assets_alloc, "/static/%1/%2", shared_etag, name).ptr;

            assets_map.Set(url, &asset);
            assets_for_cache.Append(url);
        }
    }
}

static void AttachStatic(const AssetInfo &asset, int max_age, const char *etag,
                         const http_RequestInfo &request, http_IO *io)
{
    const char *client_etag = request.GetHeaderValue("If-None-Match");

    if (client_etag && TestStr(client_etag, etag)) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        io->AttachResponse(304, response);
    } else {
        const char *mimetype = http_GetMimeType(GetPathExtension(asset.name));

        io->AttachBinary(200, asset.data, mimetype, asset.compression_type);
        io->AddCachingHeaders(max_age, etag);

        if (asset.source_map) {
            io->AddHeader("SourceMap", asset.source_map);
        }
    }
}

static void HandlePing(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    // Do this to renew session and clear invalid session cookies
    GetCheckedSession(instance, request, io);

    io->AddCachingHeaders(0, nullptr);
    io->AttachText(200, "Pong!");
}

static void HandleFileStatic(const http_RequestInfo &, http_IO *io)
{
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    for (const char *url: assets_for_cache) {
        // Skip the leading slash
        json.String(url + 1);
    }
    json.EndArray();

    json.Finish();
}

static const AssetInfo *RenderTemplate(const char *url, const AssetInfo &asset,
                                       FunctionRef<void(const char *, StreamWriter *)> func)
{
    RenderInfo *render;
    {
        std::shared_lock<std::shared_mutex> lock_shr(render_mutex);
        render = render_map.FindValue(url, nullptr);
    }

    if (!render) {
        std::lock_guard<std::shared_mutex> lock_excl(render_mutex);
        render = render_map.FindValue(url, nullptr);

        if (!render) {
            Allocator *alloc;
            render = render_cache.AppendDefault(&alloc);

            render->url = DuplicateString(url, alloc).ptr;
            render->asset = asset;
            render->asset.data = PatchAsset(asset, alloc, func);
            render->time = GetMonotonicTime();

            render_map.Set(render);

            LogDebug("Rendered '%1' with '%2'", url, asset.name);
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

        render_map.Remove(render.url);
        expired++;
    }

    render_cache.RemoveFirst(expired);

    render_cache.Trim();
    render_map.Trim();
}

static void HandleAdminRequest(const http_RequestInfo &request, http_IO *io)
{
    RG_ASSERT(StartsWith(request.url, "/admin/") || TestStr(request.url, "/admin"));
    const char *admin_url = request.url + 6;

    // Missing trailing slash, redirect
    if (!admin_url[0]) {
        const char *redirect = Fmt(&io->allocator, "%1/", request.url).ptr;
        io->AddHeader("Location", redirect);
        io->AttachNothing(302);
        return;
    }

    // Try static assets
    {
        if (TestStr(admin_url, "/")) {
            const AssetInfo *asset = assets_map.FindValue(admin_url, nullptr);
            RG_ASSERT(asset);

            const AssetInfo *render = RenderTemplate(request.url, *asset, [&](const char *key, StreamWriter *writer) {
                if (TestStr(key, "VERSION")) {
                    writer->Write(FelixVersion);
                } else if (TestStr(key, "COMPILER")) {
                    writer->Write(FelixCompiler);
                } else if (TestStr(key, "TITLE")) {
                    writer->Write("Goupile Admin");
                } else if (TestStr(key, "BASE_URL")) {
                    writer->Write("/admin/");
                } else if (TestStr(key, "STATIC_URL")) {
                    Print(writer, "/admin/static/%1/", shared_etag);
                } else if (TestStr(key, "ENV_JSON")) {
                    json_Writer json(writer);
                    char buf[128];

                    json.StartObject();
                    json.Key("urls"); json.StartObject();
                        json.Key("base"); json.String("/admin/");
                        json.Key("instance"); json.String("/admin/");
                        json.Key("static"); json.String(Fmt(buf, "/admin/static/%1/", shared_etag).ptr);
                    json.EndObject();
                    json.Key("title"); json.String("Admin");
                    json.Key("permissions"); json.StartArray();
                    for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
                        Span<const char> str = ConvertToJsonName(UserPermissionNames[i], buf);
                        json.String(str.ptr, (size_t)str.len);
                    }
                    json.EndArray();
                    json.Key("retention"); json.Int(gp_domain.config.archive_retention);
                    json.EndObject();
                } else if (TestStr(key, "HEAD_TAGS")) {
                    // Nothing to add
                } else {
                    Print(writer, "{%1}", key);
                }
            });
            AttachStatic(*render, 0, shared_etag, request, io);

            return;
        } else if (TestStr(admin_url, "/favicon.png")) {
            const AssetInfo *asset = assets_map.FindValue("/admin/favicon.png", nullptr);
            RG_ASSERT(asset);

            AttachStatic(*asset, 0, shared_etag, request, io);

            return;
        } else {
            const AssetInfo *asset = assets_map.FindValue(admin_url, nullptr);

            if (asset) {
                int max_age = StartsWith(admin_url, "/static/") ? (365 * 86400) : 0;
                AttachStatic(*asset, max_age, shared_etag, request, io);

                return;
            }
        }
    }

    // CSRF protection
    if (request.method != http_RequestMethod::Get && !http_PreventCSRF(request, io))
        return;

    // And now, API endpoints
    if (TestStr(admin_url, "/api/session/ping") && request.method == http_RequestMethod::Get) {
        HandlePing(nullptr, request, io);
    } else if (TestStr(admin_url, "/api/session/profile") && request.method == http_RequestMethod::Get) {
        HandleSessionProfile(nullptr, request, io);
    } else if (TestStr(admin_url, "/api/session/login") && request.method == http_RequestMethod::Post) {
        HandleSessionLogin(nullptr, request, io);
    } else if (TestStr(admin_url, "/api/session/confirm") && request.method == http_RequestMethod::Post) {
        HandleSessionConfirm(nullptr, request, io);
    } else if (TestStr(admin_url, "/api/session/logout") && request.method == http_RequestMethod::Post) {
        HandleSessionLogout(request, io);
    } else if (TestStr(admin_url, "/api/change/password") && request.method == http_RequestMethod::Post) {
        HandleChangePassword(nullptr, request, io);
    } else if (TestStr(admin_url, "/api/change/qrcode") && request.method == http_RequestMethod::Get) {
        HandleChangeQRcode(request, io);
    } else if (TestStr(admin_url, "/api/change/totp") && request.method == http_RequestMethod::Post) {
        HandleChangeTOTP(request, io);
    } else if (TestStr(admin_url, "/api/instances/create") && request.method == http_RequestMethod::Post) {
        HandleInstanceCreate(request, io);
    } else if (TestStr(admin_url, "/api/instances/delete") && request.method == http_RequestMethod::Post) {
        HandleInstanceDelete(request, io);
    } else if (TestStr(admin_url, "/api/instances/configure") && request.method == http_RequestMethod::Post) {
        HandleInstanceConfigure(request, io);
    } else if (TestStr(admin_url, "/api/instances/list") && request.method == http_RequestMethod::Get) {
        HandleInstanceList(request, io);
    } else if (TestStr(admin_url, "/api/instances/assign") && request.method == http_RequestMethod::Post) {
        HandleInstanceAssign(request, io);
    } else if (TestStr(admin_url, "/api/instances/permissions") && request.method == http_RequestMethod::Get) {
        HandleInstancePermissions(request, io);
    } else if (TestStr(admin_url, "/api/archives/create") && request.method == http_RequestMethod::Post) {
        HandleArchiveCreate(request, io);
    } else if (TestStr(admin_url, "/api/archives/delete") && request.method == http_RequestMethod::Post) {
        HandleArchiveDelete(request, io);
    } else if (TestStr(admin_url, "/api/archives/list") && request.method == http_RequestMethod::Get) {
        HandleArchiveList(request, io);
    } else if (StartsWith(admin_url, "/api/archives/files") && request.method == http_RequestMethod::Get) {
        HandleArchiveDownload(request, io);
    } else if (StartsWith(admin_url, "/api/archives/files") && request.method == http_RequestMethod::Put) {
        HandleArchiveUpload(request, io);
    } else if (TestStr(admin_url, "/api/archives/restore") && request.method == http_RequestMethod::Post) {
        HandleArchiveRestore(request, io);
    } else if (TestStr(admin_url, "/api/users/create") && request.method == http_RequestMethod::Post) {
        HandleUserCreate(request, io);
    } else if (TestStr(admin_url, "/api/users/edit") && request.method == http_RequestMethod::Post) {
        HandleUserEdit(request, io);
    } else if (TestStr(admin_url, "/api/users/delete") && request.method == http_RequestMethod::Post) {
        HandleUserDelete(request, io);
    } else if (TestStr(admin_url, "/api/users/list") && request.method == http_RequestMethod::Get) {
        HandleUserList(request, io);
    } else if (TestStr(admin_url, "/api/send/mail") && request.method == http_RequestMethod::Post) {
        HandleSendMail(nullptr, request, io);
    } else if (TestStr(admin_url, "/api/send/sms") && request.method == http_RequestMethod::Post) {
        HandleSendSMS(nullptr, request, io);
    } else {
        io->AttachError(404);
    }
}

static void HandleInstanceRequest(const http_RequestInfo &request, http_IO *io)
{
    InstanceHolder *instance = nullptr;
    const char *instance_url = request.url;

    // Find relevant instance
    for (int i = 0; i < 2 && instance_url[0]; i++) {
        Size offset = SplitStr(instance_url + 1, '/').len + 1;

        const char *new_url = instance_url + offset;
        Span<const char> new_key = MakeSpan(request.url + 1, new_url - request.url - 1);

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
        io->AttachError(404);
        return;
    }
    io->AddFinalizer([=]() { instance->Unref(); });

    // Enforce trailing slash on base URLs. Use 302 instead of 301 to avoid
    // problems with query strings being erased without question.
    if (!instance_url[0]) {
        HeapArray<char> buf(&io->allocator);

        Fmt(&buf, "%1/?", request.url);
        MHD_get_connection_values(request.conn, MHD_GET_ARGUMENT_KIND, [](void *udata, enum MHD_ValueKind,
                                                                     const char *key, const char *value) {
            HeapArray<char> *buf = (HeapArray<char> *)udata;

            http_EncodeUrlSafe(key, buf);
            buf->Append('=');
            http_EncodeUrlSafe(value, buf);
            buf->Append('&');

            return MHD_YES;
        }, &buf);
        buf.ptr[buf.len - 1] = 0;

        io->AddHeader("Location", buf.ptr);
        io->AttachNothing(302);
        return;
    }

    // Handle sessions triggered by query parameters
    if (request.method == http_RequestMethod::Get) {
        if (instance->config.auto_key && !HandleSessionKey(instance, request, io))
            return;
        if (!HandleSessionToken(instance, request, io))
            return;
    }

    // Try application files
    if (request.method == http_RequestMethod::Get && HandleFileGet(instance, request, io))
        return;

    // Try static assets
    if (request.method == http_RequestMethod::Get) {
        if (StartsWith(instance_url, "/main/")) {
            instance_url = "/";
        }

        const AssetInfo *asset = assets_map.FindValue(instance_url, nullptr);

        if (TestStr(instance_url, "/") || TestStr(instance_url, "/sw.pk.js") ||
                                          TestStr(instance_url, "/manifest.json")) {
            RG_ASSERT(asset);

            const InstanceHolder *master = instance->master;
            int64_t fs_version = master->fs_version.load(std::memory_order_relaxed);

            char master_etag[64];
            Fmt(master_etag, "%1_%2_%3_%4", shared_etag, (const void *)asset, master->unique, fs_version);

            const AssetInfo *render = RenderTemplate(master_etag, *asset, [&](const char *key, StreamWriter *writer) {
                if (TestStr(key, "VERSION")) {
                    writer->Write(FelixVersion);
                } else if (TestStr(key, "COMPILER")) {
                    writer->Write(FelixCompiler);
                } else if (TestStr(key, "TITLE")) {
                    writer->Write(master->title);
                } else if (TestStr(key, "BASE_URL")) {
                    Print(writer, "/%1/", master->key);
                } else if (TestStr(key, "STATIC_URL")) {
                    Print(writer, "/%1/static/%2/", master->key, shared_etag);
                } else if (TestStr(key, "ENV_JSON")) {
                    json_Writer json(writer);
                    char buf[512];

                    json.StartObject();
                    json.Key("urls"); json.StartObject();
                        json.Key("base"); json.String(Fmt(buf, "/%1/", master->key).ptr);
                        json.Key("instance"); json.String(Fmt(buf, "/%1/", master->key).ptr);
                        json.Key("static"); json.String(Fmt(buf, "/%1/static/%2/", master->key, shared_etag).ptr);
                        json.Key("files"); json.String(Fmt(buf, "/%1/files/%2/", master->key, fs_version).ptr);
                    json.EndObject();
                    json.Key("title"); json.String(master->title);
                    json.Key("version"); json.Int64(fs_version);
                    json.Key("buster"); json.String(master_etag);
                    json.Key("cache_offline"); json.Bool(master->config.use_offline);
                    {
                        Span<const char> str = ConvertToJsonName(SyncModeNames[(int)master->config.sync_mode], buf);
                        json.Key("sync_mode"); json.String(str.ptr, (size_t)str.len);
                    }
                    if (master->config.backup_key) {
                        json.Key("backup_key"); json.String(master->config.backup_key);
                    }
                    if (master->slaves.len) {
                        json.Key("instances"); json.StartArray();
                        for (const InstanceHolder *slave: master->slaves) {
                            json.StartObject();
                            json.Key("title"); json.String(slave->title);
                            json.Key("name"); json.String(slave->config.name);
                            json.Key("url"); json.String(Fmt(buf, "/%1/", slave->key).ptr);
                            json.EndObject();
                        }
                        json.EndArray();
                    }
                    json.EndObject();
                } else if (TestStr(key, "HEAD_TAGS")) {
                    if (master->config.use_offline) {
                        Print(writer, "<link rel=\"manifest\" href=\"/%1/manifest.json\"/>", master->key);
                    }
                } else {
                    Print(writer, "{%1}", key);
                }
            });
            AttachStatic(*render, 0, master_etag, request, io);

            return;
        } else if (asset) {
            int max_age = StartsWith(instance_url, "/static/") ? (365 * 86400) : 0;
            AttachStatic(*asset, max_age, shared_etag, request, io);

            return;
        }
    }

    // CSRF protection
    if (request.method != http_RequestMethod::Get && !http_PreventCSRF(request, io))
        return;

    // And now, API endpoints
    if (TestStr(instance_url, "/api/session/ping") && request.method == http_RequestMethod::Get) {
        HandlePing(instance, request, io);
    } else if (TestStr(instance_url, "/api/session/profile") && request.method == http_RequestMethod::Get) {
        HandleSessionProfile(instance, request, io);
    } else if (TestStr(instance_url, "/api/session/login") && request.method == http_RequestMethod::Post) {
        HandleSessionLogin(instance, request, io);
    } else if (TestStr(instance_url, "/api/session/key") && request.method == http_RequestMethod::Post) {
        HandleSessionKey(instance, request, io);
    } else if (TestStr(instance_url, "/api/session/confirm") && request.method == http_RequestMethod::Post) {
        HandleSessionConfirm(instance, request, io);
    } else if (TestStr(instance_url, "/api/session/logout") && request.method == http_RequestMethod::Post) {
        HandleSessionLogout(request, io);
    } else if (TestStr(instance_url, "/api/change/password") && request.method == http_RequestMethod::Post) {
        HandleChangePassword(instance, request, io);
    } else if (TestStr(instance_url, "/api/change/qrcode") && request.method == http_RequestMethod::Get) {
        HandleChangeQRcode(request, io);
    } else if (TestStr(instance_url, "/api/change/totp") && request.method == http_RequestMethod::Post) {
        HandleChangeTOTP(request, io);
    } else if (TestStr(instance_url, "/api/change/mode") && request.method == http_RequestMethod::Post) {
        HandleChangeMode(instance, request, io);
    } else if (TestStr(instance_url, "/api/change/export_key") && request.method == http_RequestMethod::Post) {
        HandleChangeExportKey(instance, request, io);
    } else if (TestStr(instance_url, "/api/files/static") && request.method == http_RequestMethod::Get) {
         HandleFileStatic(request, io);
    } else if (TestStr(instance_url, "/api/files/list") && request.method == http_RequestMethod::Get) {
         HandleFileList(instance, request, io);
    } else if (StartsWith(instance_url, "/files/") && request.method == http_RequestMethod::Put) {
        HandleFilePut(instance, request, io);
    } else if (StartsWith(instance_url, "/files/") && request.method == http_RequestMethod::Delete) {
        HandleFileDelete(instance, request, io);
    } else if (StartsWith(instance_url, "/api/files/delta") && request.method == http_RequestMethod::Get) {
        HandleFileDelta(instance, request, io);
    } else if (StartsWith(instance_url, "/api/files/publish") && request.method == http_RequestMethod::Post) {
        HandleFilePublish(instance, request, io);
    } else if (TestStr(instance_url, "/api/records/load") && request.method == http_RequestMethod::Get) {
        HandleRecordLoad(instance, request, io);
    } else if (TestStr(instance_url, "/api/records/save") && request.method == http_RequestMethod::Post) {
        HandleRecordSave(instance, request, io);
    } else if (TestStr(instance_url, "/api/records/export") && request.method == http_RequestMethod::Get) {
        HandleRecordExport(instance, request, io);
    } else if (TestStr(instance_url, "/api/send/mail") && request.method == http_RequestMethod::Post) {
        HandleSendMail(instance, request, io);
    } else if (TestStr(instance_url, "/api/send/sms") && request.method == http_RequestMethod::Post) {
        HandleSendSMS(instance, request, io);
    } else if (TestStr(instance_url, "/api/send/tokenize") && request.method == http_RequestMethod::Post) {
        HandleSendTokenize(instance, request, io);
    } else {
        io->AttachError(404);
    }
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
#ifdef FELIX_HOT_ASSETS
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
            io->AttachError(400);
            return;
        }
        if (!TestStr(host, gp_domain.config.require_host)) {
            LogError("Unexpected Host header '%1'", host);
            io->AttachError(403);
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
    if (TestStr(request.url, "/")) {
        const AssetInfo *render = RenderTemplate("/", *assets_root, [&](const char *key, StreamWriter *writer) {
            if (TestStr(key, "STATIC_URL")) {
                Print(writer, "/admin/static/%1/", shared_etag);
            } else if (TestStr(key, "VERSION")) {
                writer->Write(FelixVersion);
            } else if (TestStr(key, "COMPILER")) {
                writer->Write(FelixCompiler);
            } else {
                Print(writer, "{%1}", key);
            }
        });
        AttachStatic(*render, 0, shared_etag, request, io);
    } else if (TestStr(request.url, "/favicon.png")) {
        const AssetInfo *asset = assets_map.FindValue("/favicon.png", nullptr);
        RG_ASSERT(asset);

        AttachStatic(*asset, 0, shared_etag, request, io);
    } else if (StartsWith(request.url, "/admin/") || TestStr(request.url, "/admin")) {
        HandleAdminRequest(request, io);
    } else {
        HandleInstanceRequest(request, io);
    }
}

static bool PruneOldFiles(const char *dirname, const char *filter, bool recursive, int64_t max_age,
                          int64_t *out_max_mtime = nullptr)
{
    BlockAllocator temp_alloc;

    int64_t threshold = GetUnixTime() - max_age;
    int64_t max_mtime = 0;
    bool complete = true;

    EnumerateDirectory(dirname, nullptr, -1, [&](const char *basename, FileType file_type) {
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

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [serve] [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %3)%!0
        %!..+--sandbox%!0                Run sandboxed (on supported platforms)

        %!..+--no_migrate%!0             Disable automatic migration of database schemas

Other commands:
    %!..+init%!0                         Create new domain
    %!..+migrate%!0                      Migrate existing domain
    %!..+unseal%!0                       Unseal domain archive

For help about those commands, type: %!..+%1 <command> --help%!0)",
                FelixTarget, config_filename, gp_domain.config.http.port);
    };

    // Find config filename
    {
        OptionParser opt(arguments, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
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

#ifndef _WIN32
    {
        const rlim_t max_nofile = 4096;
        struct rlimit lim;

        // Increase maximum number of open file descriptors
        if (getrlimit(RLIMIT_NOFILE, &lim) >= 0) {
            if (lim.rlim_cur < max_nofile) {
                lim.rlim_cur = std::min(max_nofile, lim.rlim_max);

                if (setrlimit(RLIMIT_NOFILE, &lim) >= 0) {
                    if (lim.rlim_cur < max_nofile) {
                        LogError("Maximum number of open descriptors is low: %1 (recommended: %2)", lim.rlim_cur, max_nofile);
                    }
                } else {
                    LogError("Could not raise RLIMIT_NOFILE to %1: %2", max_nofile, strerror(errno));
                }
            }
        } else {
            LogError("getrlimit(RLIMIT_NOFILE) failed: %1", strerror(errno));
        }
    }
#endif

    LogInfo("Init assets");
    InitAssets();
    LogInfo("Init instances");
    if (!gp_domain.Open(config_filename))
        return 1;
    if (!gp_domain.SyncAll())
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &gp_domain.config.http.port))
                    return 1;
            } else if (opt.Test("--sandbox")) {
                // Already handled
            } else if (opt.Test("--no_migrate")) {
                gp_domain.config.auto_migrate = false;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

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

#ifdef __linux__
    if (!NotifySystemd())
        return 1;
#endif

    // Apply sandbox
    if (sandbox) {
        LogInfo("Init sandbox");

        const char *const reveal_paths[] = {
#ifdef FELIX_HOT_ASSETS
            // Needed for asset module
            GetApplicationDirectory(),
#endif
            gp_domain.config.database_directory,
            gp_domain.config.archive_directory,
            gp_domain.config.snapshot_directory
        };
        const char *const mask_files[] = {
            gp_domain.config.config_filename
        };

        if (!ApplySandbox(reveal_paths, mask_files))
            return 1;
    }

    // From here on, don't quit abruptly
    WaitForInterrupt(0);

    // Run!
    if (!daemon.Start(gp_domain.config.http, HandleRequest))
        return 1;

    // Run periodic tasks until exit
    {
        bool run = true;
        bool first = true;
        int timeout = 180 * 1000;

        // Randomize the delay a bit to reduce situations where all goupile
        // services perform cleanups at the same time and cause a load spike.
        timeout += randombytes_uniform(timeout / 4 + 1);
        LogInfo("Periodic timer set to %1 s", FmtDouble((double)timeout / 1000.0, 1));

        while (run) {
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
                    TimeSpec spec = DecomposeTime(time, gp_domain.config.archive_zone);

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

            if (ret == WaitForResult::Interrupt) {
                LogInfo("Exit requested");

                LogDebug("Stop HTTP server");
                daemon.Stop();

                run = false;
            } else if (ret == WaitForResult::Message) {
                LogDebug("Syncing instances");
                gp_domain.SyncAll(true);
            }

            LogDebug("Prune sessions");
            PruneSessions();

            LogDebug("Prune template renders");
            PruneRenders();

#ifdef __GLIBC__
            // Actually release memory to the OS, because for some reason glibc doesn't want to
            // do this automatically even after 98% of the resident memory pool has been freed.
            LogDebug("Release memory (glibc)");
            malloc_trim(0);
#endif

            first = false;
        }
    }

    return 0;
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
int main(int argc, char **argv) { return RG::Main(argc, argv); }
