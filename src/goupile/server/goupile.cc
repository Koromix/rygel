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

#include "../../core/libcc/libcc.hh"
#include "admin.hh"
#include "domain.hh"
#include "files.hh"
#include "goupile.hh"
#include "instance.hh"
#include "messages.hh"
#include "records.hh"
#include "session.hh"
#include "../../core/libnet/libnet.hh"
#include "../../core/libsecurity/sandbox.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
#include "../../../vendor/curl/include/curl/curl.h"
#ifndef _WIN32
    #include <sys/time.h>
    #include <sys/resource.h>
#endif
#ifdef __GLIBC__
    #include <malloc.h>
#endif

namespace RG {

DomainHolder gp_domain;

static HashMap<const char *, const AssetInfo *> assets_map;
static const AssetInfo *assets_root;
static HeapArray<const char *> assets_for_cache;
static LinkedAllocator assets_alloc;
static char etag[17];

static bool ApplySandbox(Span<const char *const> paths)
{
    if (!sec_IsSandboxSupported()) {
        LogError("Sandbox mode is not supported on this platform");
        return false;
    }

    sec_SandboxBuilder sb;

    sb.RevealPaths(paths, false);

#ifdef __linux__
    sb.FilterSyscalls(sec_FilterAction::Kill, {
        {"exit", sec_FilterAction::Allow},
        {"exit_group", sec_FilterAction::Allow},
        {"brk", sec_FilterAction::Allow},
        {"mmap/anon", sec_FilterAction::Allow},
        {"mmap/shared", sec_FilterAction::Allow},
        {"munmap", sec_FilterAction::Allow},
        {"mprotect/noexec", sec_FilterAction::Allow},
        {"mlock", sec_FilterAction::Allow},
        {"mlock2", sec_FilterAction::Allow},
        {"mlockall", sec_FilterAction::Allow},
        {"madvise", sec_FilterAction::Allow},
        {"pipe", sec_FilterAction::Allow},
        {"pipe2", sec_FilterAction::Allow},
        {"open", sec_FilterAction::Allow},
        {"openat", sec_FilterAction::Allow},
        {"openat2", sec_FilterAction::Allow},
        {"close", sec_FilterAction::Allow},
        {"fcntl", sec_FilterAction::Allow},
        {"read", sec_FilterAction::Allow},
        {"readv", sec_FilterAction::Allow},
        {"write", sec_FilterAction::Allow},
        {"writev", sec_FilterAction::Allow},
        {"lseek", sec_FilterAction::Allow},
        {"ftruncate", sec_FilterAction::Allow},
        {"fsync", sec_FilterAction::Allow},
        {"fstat", sec_FilterAction::Allow},
        {"stat", sec_FilterAction::Allow},
        {"lstat", sec_FilterAction::Allow},
        {"lstat64", sec_FilterAction::Allow},
        {"ioctl/tty", sec_FilterAction::Allow},
        {"getrandom", sec_FilterAction::Allow},
        {"getpid", sec_FilterAction::Allow},
        {"gettid", sec_FilterAction::Allow},
        {"getuid", sec_FilterAction::Allow},
        {"getgid", sec_FilterAction::Allow},
        {"geteuid", sec_FilterAction::Allow},
        {"getegid", sec_FilterAction::Allow},
        {"getcwd", sec_FilterAction::Allow},
        {"rt_sigaction", sec_FilterAction::Allow},
        {"rt_sigpending", sec_FilterAction::Allow},
        {"rt_sigprocmask", sec_FilterAction::Allow},
        {"rt_sigqueueinfo", sec_FilterAction::Allow},
        {"rt_sigreturn", sec_FilterAction::Allow},
        {"rt_sigsuspend", sec_FilterAction::Allow},
        {"rt_sigtimedwait", sec_FilterAction::Allow},
        {"rt_sigtimedwait_time64", sec_FilterAction::Allow},
        {"tgkill", sec_FilterAction::Allow},
        {"mkdir", sec_FilterAction::Allow},
        {"mkdirat", sec_FilterAction::Allow},
        {"unlink", sec_FilterAction::Allow},
        {"unlinkat", sec_FilterAction::Allow},
        {"rename", sec_FilterAction::Allow},
        {"renameat", sec_FilterAction::Allow},
        {"renameat2", sec_FilterAction::Allow},
        {"chown", sec_FilterAction::Allow},
        {"chmod", sec_FilterAction::Allow},
        {"clone/thread", sec_FilterAction::Allow},
        {"futex", sec_FilterAction::Allow},
        {"set_robust_list", sec_FilterAction::Allow},
        {"socket", sec_FilterAction::Allow},
        {"getsockopt", sec_FilterAction::Allow},
        {"setsockopt", sec_FilterAction::Allow},
        {"bind", sec_FilterAction::Allow},
        {"listen", sec_FilterAction::Allow},
        {"accept", sec_FilterAction::Allow},
        {"accept4", sec_FilterAction::Allow},
        {"eventfd", sec_FilterAction::Allow},
        {"eventfd2", sec_FilterAction::Allow},
        {"getdents", sec_FilterAction::Allow},
        {"getdents64", sec_FilterAction::Allow},
        {"prctl", sec_FilterAction::Allow},
        {"epoll_create", sec_FilterAction::Allow},
        {"epoll_create1", sec_FilterAction::Allow},
        {"epoll_ctl", sec_FilterAction::Allow},
        {"epoll_pwait", sec_FilterAction::Allow},
        {"epoll_wait", sec_FilterAction::Allow},
        {"clock_nanosleep", sec_FilterAction::Allow},
        {"clock_gettime", sec_FilterAction::Allow},
        {"clock_gettime64", sec_FilterAction::Allow},
        {"clock_nanosleep", sec_FilterAction::Allow},
        {"clock_nanosleep_time64", sec_FilterAction::Allow},
        {"nanosleep", sec_FilterAction::Allow},
        {"sched_yield", sec_FilterAction::Allow},
        {"recv", sec_FilterAction::Allow},
        {"recvfrom", sec_FilterAction::Allow},
        {"recvmmsg", sec_FilterAction::Allow},
        {"recvmmsg_time64", sec_FilterAction::Allow},
        {"recvmsg", sec_FilterAction::Allow},
        {"sendmsg", sec_FilterAction::Allow},
        {"sendmmsg", sec_FilterAction::Allow},
        {"sendfile", sec_FilterAction::Allow},
        {"sendfile64", sec_FilterAction::Allow},
        {"sendto", sec_FilterAction::Allow},
        {"shutdown", sec_FilterAction::Allow}
    });
#endif

    return sb.Apply();
}

static void InitAssets()
{
    assets_map.Clear();
    assets_for_cache.Clear();
    assets_alloc.ReleaseAll();

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
        } else if (StartsWith(asset.name, "src/goupile/client/") ||
                   StartsWith(asset.name, "vendor/")) {
            const char *name = SplitStrReverseAny(asset.name, RG_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&assets_alloc, "/static/%1", name).ptr;

            assets_map.Set(url, &asset);
            assets_for_cache.Append(url);
        }
    }

    // Update ETag
    {
        uint64_t buf;
        randombytes_buf(&buf, RG_SIZE(buf));
        Fmt(etag, "%1", FmtHex(buf).Pad0(-16));
    }
}

static void AttachStatic(const AssetInfo &asset, const char *etag, const http_RequestInfo &request, http_IO *io)
{
    const char *client_etag = request.GetHeaderValue("If-None-Match");

    if (client_etag && TestStr(client_etag, etag)) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        io->AttachResponse(304, response);
    } else {
        const char *mimetype = http_GetMimeType(GetPathExtension(asset.name));
        io->AttachBinary(200, asset.data, mimetype, asset.compression_type);

        io->AddCachingHeaders(gp_domain.config.max_age, etag);
        if (asset.source_map) {
            // io->AddHeader("SourceMap", asset.source_map);
        }
    }
}

static void HandlePing(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    // Do this to renew session and clear invalid session cookies
    GetCheckedSession(instance, request, io);

    io->AddHeader("Cache-Control", "no-store");
    io->AttachText(200, "Pong!");
}

static void HandleFileStatic(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
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

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
#ifndef NDEBUG
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        if (ReloadAssets()) {
            LogInfo("Reload assets");
            InitAssets();
        }
    }
#endif

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");
    io->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    io->AddHeader("X-Robots-Tag", "noindex");
    io->AddHeader("Permissions-Policy", "interest-cohort=()");

    // If new base URLs are added besides "/admin", RunCreateInstance() must be modified
    // to forbid the instance key.
    if (TestStr(request.url, "/")) {
        AssetInfo copy = *assets_root;
        copy.data = PatchAsset(copy, &io->allocator, [&](const char *key, StreamWriter *writer) {
            if (TestStr(key, "VERSION")) {
                writer->Write(FelixVersion);
            } else {
                Print(writer, "{%1}", key);
            }
        });

        AttachStatic(copy, etag, request, io);
    } else if (StartsWith(request.url, "/admin/") || TestStr(request.url, "/admin")) {
        const char *admin_url = request.url + 6;

        // Missing trailing slash, redirect
        if (!admin_url[0]) {
            const char *redirect = Fmt(&io->allocator, "%1/", request.url).ptr;
            io->AddHeader("Location", redirect);
            io->AttachNothing(301);
            return;
        }

        // Try static assets
        {
            const AssetInfo *asset = assets_map.FindValue(admin_url, nullptr);

            if (TestStr(admin_url, "/")) {
                RG_ASSERT(asset);

                AssetInfo copy = *asset;
                copy.data = PatchAsset(copy, &io->allocator, [&](const char *key, StreamWriter *writer) {
                    if (TestStr(key, "VERSION")) {
                        writer->Write(FelixVersion);
                    } else if (TestStr(key, "TITLE")) {
                        writer->Write("Goupile Admin");
                    } else if (TestStr(key, "BASE_URL")) {
                        writer->Write("/admin/");
                    } else if (TestStr(key, "ENV_JSON")) {
                        json_Writer json(writer);
                        char buf[128];

                        json.StartObject();
                        json.Key("urls"); json.StartObject();
                            json.Key("base"); json.String("/admin/");
                            json.Key("instance"); json.String("/admin/");
                        json.EndObject();
                        json.Key("title"); json.String("Goupile Admin");
                        json.Key("permissions"); json.StartArray();
                        for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
                            Span<const char> str = ConvertToJsonName(UserPermissionNames[i], buf);
                            json.String(str.ptr, (size_t)str.len);
                        }
                        json.EndArray();
                        json.EndObject();
                    } else if (TestStr(key, "HEAD_TAGS")) {
                        // Nothing to add
                    } else {
                        Print(writer, "{%1}", key);
                    }
                });

                AttachStatic(copy, etag, request, io);
                return;
            } else if (asset) {
                AttachStatic(*asset, etag, request, io);
                return;
            }
        }

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
        } else if (TestStr(admin_url, "/api/password/change") && request.method == http_RequestMethod::Post) {
            HandlePasswordChange(request, io);
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
        } else if (TestStr(admin_url, "/api/archives/download") && request.method == http_RequestMethod::Get) {
            HandleArchiveDownload(request, io);
        } else if (TestStr(admin_url, "/api/archives/upload") && request.method == http_RequestMethod::Put) {
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
    } else {
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

        // Handle sessions triggered by query parameters
        if (instance->config.auto_key && HandleSessionKey(instance, request, io))
            return;

        // Enforce trailing slash on base URLs. Use 302 instead of 301 to avoid
        // problems with query strings being erased without question.
        if (!instance_url[0]) {
            const char *redirect = Fmt(&io->allocator, "%1/", request.url).ptr;
            io->AddHeader("Location", redirect);
            io->AttachNothing(302);
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

                // XXX: Use some kind of dynamic cache to avoid doing this all the time
                AssetInfo copy = *asset;
                copy.data = PatchAsset(copy, &io->allocator, [&](const char *key, StreamWriter *writer) {
                    const InstanceHolder *master = instance->master;

                    if (TestStr(key, "VERSION")) {
                        writer->Write(FelixVersion);
                    } else if (TestStr(key, "TITLE")) {
                        writer->Write(master->title);
                    } else if (TestStr(key, "BASE_URL")) {
                        Print(writer, "/%1/", master->key);
                    } else if (TestStr(key, "ENV_JSON")) {
                        json_Writer json(writer);
                        char buf[512];

                        json.StartObject();
                        json.Key("urls"); json.StartObject();
                            json.Key("base"); json.String(Fmt(buf, "/%1/", master->key).ptr);
                            json.Key("instance"); json.String(Fmt(buf, "/%1/", master->key).ptr);
                        json.EndObject();
                        json.Key("title"); json.String(master->title);
                        json.Key("cache_offline"); json.Bool(master->config.use_offline);
                        if (master->config.use_offline) {
                            json.Key("cache_key"); json.String(Fmt(buf, "%1_%2", etag, master->unique).ptr);
                        }
                        {
                            Span<const char> str = ConvertToJsonName(SyncModeNames[(int)master->config.sync_mode], buf);
                            json.Key("sync_mode"); json.String(str.ptr, (size_t)str.len);
                        }
                        if (master->config.backup_key) {
                            json.Key("backup_key"); json.String(master->config.backup_key);
                        }
                        if (instance != master || instance->slaves.len) {
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

                char specific_etag[33];
                Fmt(specific_etag, "%1_%2", etag, instance->unique);

                AttachStatic(copy, specific_etag, request, io);
                return;
            } else if (asset) {
                AttachStatic(*asset, etag, request, io);
                return;
            }
        }

        // And now, API endpoints
        if (TestStr(instance_url, "/api/session/ping") && request.method == http_RequestMethod::Get) {
            HandlePing(instance, request, io);
        } else if (TestStr(instance_url, "/api/session/profile") && request.method == http_RequestMethod::Get) {
            HandleSessionProfile(instance, request, io);
        } else if (TestStr(instance_url, "/api/session/login") && request.method == http_RequestMethod::Post) {
            HandleSessionLogin(instance, request, io);
        } else if (TestStr(instance_url, "/api/session/token") && request.method == http_RequestMethod::Post) {
            HandleSessionToken(instance, request, io);
        } else if (TestStr(instance_url, "/api/session/confirm") && request.method == http_RequestMethod::Post) {
            HandleSessionConfirm(instance, request, io);
        } else if (TestStr(instance_url, "/api/session/logout") && request.method == http_RequestMethod::Post) {
            HandleSessionLogout(request, io);
        } else if (TestStr(instance_url, "/api/password/change") && request.method == http_RequestMethod::Post) {
            HandlePasswordChange(request, io);
        } else if (TestStr(instance_url, "/api/files/static") && request.method == http_RequestMethod::Get) {
             HandleFileStatic(instance, request, io);
        } else if (TestStr(instance_url, "/api/files/list") && request.method == http_RequestMethod::Get) {
             HandleFileList(instance, request, io);
        } else if (StartsWith(instance_url, "/files/") && request.method == http_RequestMethod::Put) {
            HandleFilePut(instance, request, io);
        } else if (StartsWith(instance_url, "/files/") && request.method == http_RequestMethod::Delete) {
            HandleFileDelete(instance, request, io);
        } else if (TestStr(instance_url, "/api/records/load") && request.method == http_RequestMethod::Get) {
            HandleRecordLoad(instance, request, io);
        } else if (TestStr(instance_url, "/api/records/save") && request.method == http_RequestMethod::Post) {
            HandleRecordSave(instance, request, io);
        } else if (TestStr(instance_url, "/api/send/mail") && request.method == http_RequestMethod::Post) {
            HandleSendMail(instance, request, io);
        } else if (TestStr(instance_url, "/api/send/sms") && request.method == http_RequestMethod::Post) {
            HandleSendSMS(instance, request, io);
        } else {
            io->AttachError(404);
        }
    }
}

static void PruneTemporaryFiles()
{
    BlockAllocator temp_alloc;

    int64_t treshold = GetUnixTime() - 7200 * 1000;

    EnumerateDirectory(gp_domain.config.temp_directory, nullptr, -1,
                       [&](const char *filename, FileType file_type) {
        filename = Fmt(&temp_alloc, "%1%/%2", gp_domain.config.temp_directory, filename).ptr;

        FileInfo file_info;
        if (!StatFile(filename, &file_info))
            return true;
        if (file_info.type != FileType::File)
            return true;

        if (file_info.modification_time < treshold) {
            LogInfo("Delete leftover temporary file %1", filename);
            UnlinkFile(filename);
        }

        return true;
    });
}

static int RunServe(Span<const char *> arguments)
{
    const char *config_filename = "goupile.ini";
    bool sandbox = false;

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 serve [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %3)%!0

        %!..+--sandbox%!0                Run sandboxed (on supported platforms)

Other commands:
    %!..+init%!0                         Create new domain
    %!..+migrate%!0                      Migrate existing domain
    %!..+unreal%!0                       Unseal domain archive

For help about those commands, type: %!..+%1 <command> --help%!0)",
                FelixTarget, config_filename, gp_domain.config.http.port);
    };

    // Find config filename
    {
        OptionParser opt(arguments, (int)OptionParser::Flag::SkipNonOptions);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.Test("--sandbox")) {
                sandbox = true;
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
    if (!gp_domain.Sync())
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
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
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

        const char *const paths[] = {
#ifndef NDEBUG
            // Needed for asset module
            GetApplicationDirectory(),
#endif
            gp_domain.config.database_filename,
            gp_domain.config.instances_directory,
            gp_domain.config.temp_directory,
            gp_domain.config.backup_directory
        };

        if (!ApplySandbox(paths))
            return 1;
    }

    // Run!
    if (!daemon.Start(gp_domain.config.http, HandleRequest))
        return 1;
#ifndef _WIN32
    if (gp_domain.config.http.sock_type == SocketType::Unix) {
        LogInfo("Listening on socket '%1' (Unix stack)", gp_domain.config.http.unix_path);
    } else
#endif
    LogInfo("Listening on port %1 (%2 stack)",
            gp_domain.config.http.port, SocketTypeNames[(int)gp_domain.config.http.sock_type]);

    // Run periodic tasks until exit
    {
        bool run = true;
        int timeout = 120 * 1000;

        // Randomize the delay a bit to reduce situations where all goupile
        // services perform cleanups at the same time and cause a load spike.
        timeout += (randombytes_random() & 0x7F) * 500;
        LogDebug("Periodic cleanup timer set to %1 s", FmtDouble((double)timeout / 1000.0, 1));

        while (run) {
            // In theory, all temporary files are deleted. But if any remain behind (crash, etc.)
            // we need to make sure they get deleted eventually.
            LogDebug("Prune temporary files");
            PruneTemporaryFiles();

            WaitForResult ret = WaitForInterrupt(timeout);

            if (ret == WaitForResult::Interrupt) {
                LogInfo("Exit requested");

                LogDebug("Stop HTTP server");
                daemon.Stop();

                run = false;
            } else if (ret == WaitForResult::Message) {
                LogDebug("Syncing instances");
                gp_domain.Sync();
            }

            // Make sure data loss (if it happens) is very limited in time
            if (!gp_domain.config.sync_full) {
                LogDebug("Checkpoint databases");
                gp_domain.Checkpoint();
            }

            LogDebug("Prune sessions");
            PruneSessions();

#ifdef __GLIBC__
            // Actually release memory to the OS, because for some reason glibc doesn't want to
            // do this automatically even after 98% of the resident memory pool has been freed.
            LogDebug("Release memory (glibc)");
            malloc_trim(0);
#endif
        }
    }

    return 0;
}

int Main(int argc, char **argv)
{
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
            PrintLn("%!R..%1%!0 %2", FelixTarget, FelixVersion);
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

    int (*cmd_func)(Span<const char *> arguments);
    Span<const char *> arguments;
    if (argc >= 2) {
        const char *cmd = argv[1];

        if (TestStr(cmd, "init")) {
            cmd_func = RunInit;
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        } else if (TestStr(cmd, "migrate")) {
            cmd_func = RunMigrate;
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        } else if (TestStr(cmd, "unseal")) {
            cmd_func = RunUnseal;
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        } else if (TestStr(cmd, "serve")) {
            cmd_func = RunServe;
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        } else if (cmd[0] == '-') {
            cmd_func = RunServe;
            arguments = MakeSpan((const char **)argv + 1, argc - 1);
        } else {
            LogError("Unknown command '%1'", cmd);
            return 1;
        }
    } else {
        cmd_func = RunServe;
        arguments = {};
    }

    return cmd_func(arguments);
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
