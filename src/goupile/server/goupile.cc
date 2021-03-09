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
#include "records.hh"
#include "user.hh"
#include "../../web/libhttp/libhttp.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
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

static void HandlePing(const http_RequestInfo &request, http_IO *io)
{
    // Do this to renew session and clear invalid session cookies
    GetCheckedSession(request, io);

    io->AddHeader("Cache-Control", "no-store");
    io->AttachText(200, "Pong!");
}

static void HandleFileStatic(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    for (const char *url: assets_for_cache) {
        // Skip the leading slash
        json.String(url + 1);
    }
    json.EndArray();

    json.Finish(io);
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
    io->AddHeader("X-Robots-Tag", "noindex");

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
            HandlePing(request, io);
        } else if (TestStr(admin_url, "/api/session/profile") && request.method == http_RequestMethod::Get) {
            HandleUserProfile(nullptr, request, io);
        } else if (TestStr(admin_url, "/api/session/login") && request.method == http_RequestMethod::Post) {
            HandleUserLogin(nullptr, request, io);
        } else if (TestStr(admin_url, "/api/session/logout") && request.method == http_RequestMethod::Post) {
            HandleUserLogout(nullptr, request, io);
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
        } else if (TestStr(admin_url, "/api/users/create") && request.method == http_RequestMethod::Post) {
            HandleUserCreate(request, io);
        } else if (TestStr(admin_url, "/api/users/edit") && request.method == http_RequestMethod::Post) {
            HandleUserEdit(request, io);
        } else if (TestStr(admin_url, "/api/users/delete") && request.method == http_RequestMethod::Post) {
            HandleUserDelete(request, io);
        } else if (TestStr(admin_url, "/api/users/list") && request.method == http_RequestMethod::Get) {
            HandleUserList(request, io);
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

            bool reload;
            InstanceHolder *ref = gp_domain.Ref(new_key, &reload);

            if (ref) {
                instance_url = new_url;

                if (instance) {
                    instance->Unref();
                }
                instance = ref;

                // No need to look further
                if (!instance->GetSlaveCount())
                    break;
            } else if (RG_UNLIKELY(reload)) {
                io->AttachError(503);
                return;
            } else {
                break;
            }
        }
        if (!instance) {
            io->AttachError(404);
            return;
        }
        io->AddFinalizer([=]() { instance->Unref(); });

        // Enforce trailing slash on base URLs
        if (!instance_url[0]) {
            const char *redirect = Fmt(&io->allocator, "%1/", request.url).ptr;
            io->AddHeader("Location", redirect);
            io->AttachNothing(301);
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
                        writer->Write(master->config.title);
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
                        json.Key("title"); json.String(master->config.title);
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
            HandlePing(request, io);
        } else if (TestStr(instance_url, "/api/session/profile") && request.method == http_RequestMethod::Get) {
            HandleUserProfile(instance, request, io);
        } else if (TestStr(instance_url, "/api/session/login") && request.method == http_RequestMethod::Post) {
            HandleUserLogin(instance, request, io);
        } else if (TestStr(instance_url, "/api/session/logout") && request.method == http_RequestMethod::Post) {
            HandleUserLogout(instance, request, io);
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
        } else {
            io->AttachError(404);
        }
    }
}

static int RunServe(Span<const char *> arguments)
{
    const char *config_filename = "goupile.ini";

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 serve [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %3)%!0

Other commands:
    %!..+init%!0                         Create new domain
    %!..+migrate%!0                      Migrate existing domain

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
            }
        }
    }

#ifndef _WIN32
    {
        const rlim_t max_nofile = 16384;
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
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        // We may have changed some stuff (such as HTTP port), so revalidate
        if (!gp_domain.config.Validate())
            return 1;
    }

#ifdef NDEBUG
    if (!gp_domain.config.http.use_xrealip) {
        LogInfo("If you run this behind a reverse proxy, you may want to enable the HTTP.TrustXRealIP setting in goupile.ini");
    }
#endif

    // Run!
    http_Daemon daemon;
    if (!daemon.Start(gp_domain.config.http, HandleRequest))
        return 1;
#ifndef _WIN32
    if (gp_domain.config.http.sock_type == SocketType::Unix) {
        LogInfo("Listening on socket '%1' (Unix stack)", gp_domain.config.http.unix_path);
    } else
#endif
    LogInfo("Listening on port %1 (%2 stack)",
            gp_domain.config.http.port, SocketTypeNames[(int)gp_domain.config.http.sock_type]);

#ifdef __linux__
    if (!NotifySystemd())
        return 1;
#endif

    // Run periodic tasks until exit
    {
        int timeout = 120 * 1000;
        bool run = true;

        // Randomize the delay a bit to reduce situations where all goupile
        // services perform cleanups at the same time and cause a load spike.
        timeout += (randombytes_random() & 0x7F) * 500;
        LogDebug("Periodic cleanup timer set to %1 s", FmtDouble((double)timeout / 1000.0, 1));

        while (run) {
            int iter_timeout = (gp_domain.config.sync_full && gp_domain.IsSynced()) ? -1 : timeout;
            if (WaitForInterrupt(iter_timeout) == WaitForResult::Interrupt)
                run = false;

            // React to new and deleted instances
            LogDebug("Syncing instances");
            gp_domain.Sync();

            // Make sure data loss (if it happens) is very limited in time
            if (!gp_domain.config.sync_full) {
                LogDebug("Checkpointing databases");
                gp_domain.Checkpoint();
            }

#ifdef __GLIBC__
            // Actually release memory to the OS, because for some reason glibc doesn't want to
            // do this automatically even after 98% of the resident memory pool has been freed.
            LogDebug("Release memory (glibc)");
            malloc_trim(0);
#endif
        }
    }

    daemon.Stop();
    LogInfo("Exit");

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
