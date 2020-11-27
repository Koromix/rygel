// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "admin.hh"
#include "domain.hh"
#include "files.hh"
#include "goupile.hh"
#include "instance.hh"
#include "js.hh"
#include "records.hh"
#include "user.hh"
#include "../../web/libhttp/libhttp.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

struct InstanceGuard {
    std::atomic_int refcount {0};

    InstanceGuard *next_free = nullptr;
    bool valid = true;

    InstanceData instance;

    InstanceData *Ref();
    void Unref();
};

DomainData goupile_domain;

static std::shared_mutex instances_mutex;
static BucketArray<InstanceGuard> instances;
static InstanceGuard *instances_free = nullptr;
static HashMap<Span<const char>, InstanceGuard *> instances_map;

InstanceData *InstanceGuard::Ref()
{
    refcount++;
    return &instance;
}

void InstanceGuard::Unref()
{
    if (!--refcount) {
        std::lock_guard<std::shared_mutex> lock(instances_mutex);

        instances_map.Remove(instance.key);
        instance.Close();

        next_free = instances_free;
        instances_free = this;
    }
}

static bool LoadInstance(const char *key, const char *filename)
{
    std::lock_guard<std::shared_mutex> lock(instances_mutex);

    if (!instances_free) {
        instances_free = instances.AppendDefault();
    }

    InstanceGuard *guard = instances_free;
    if (!guard->instance.Open(key, filename))
        return false;
    instances_free = guard->next_free;
    guard->next_free = nullptr;
    guard->valid = true;

    InstanceData *instance = guard->Ref();
    instances_map.Set(instance->key, guard);

    return true;
}

// Can be called multiple times, from main thread only
static bool InitInstances()
{
    BlockAllocator temp_alloc;

    bool success = true;
    HashSet<const char *> keys;

    // Start new instances (if any)
    {
        sq_Statement stmt;
        if (!goupile_domain.db.Prepare("SELECT instance FROM dom_instances;", &stmt))
            return false;

        while (stmt.Next()) {
            const char *key = (const char *)sqlite3_column_text(stmt, 0);
            key = DuplicateString(key, &temp_alloc).ptr;

            InstanceGuard *guard = instances_map.FindValue(key, nullptr);
            if (!guard) {
                LogDebug("Load instance '%1'", key);

                const char *filename = goupile_domain.config.GetInstanceFileName(key, &temp_alloc);
                success &= LoadInstance(key, filename);
            } else if (!guard->valid) {
                LogDebug("Instance key '%1' is not reusable yet, try again");
                success = false;
            }

            keys.Set(key);
        }
        success &= stmt.IsValid();
    }

    // Drop removed instances (if any)
    for (InstanceGuard &guard: instances) {
        if (guard.valid) {
            InstanceData *instance = &guard.instance;

            if (!keys.Find(instance->key)) {
                LogDebug("Drop instance '%1'", instance->key);

                guard.Unref();
                guard.valid = false;
            }
        }
    }

    return success;
}

static void HandleEvents(InstanceData *, const http_RequestInfo &request, http_IO *io)
{
    // Do this to renew session and clear invalid session cookies
    GetCheckedSession(request, io);

    io->AttachText(200, "{}", "application/json");
}

static void HandleFileStatic(InstanceData *instance, const http_RequestInfo &request, http_IO *io)
{
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    for (const AssetInfo &asset: instance->assets) {
        char buf[512];
        json.String(Fmt(buf, "%1%2", instance->base_url, asset.name + 1).ptr);
    }
    json.EndArray();

    json.Finish(io);
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
    if (ReloadAssets()) {
        std::lock_guard<std::shared_mutex> lock(instances_mutex);

        for (InstanceGuard &guard: instances) {
            if (guard.valid) {
                InstanceData *instance = &guard.instance;
                instance->InitAssets();
            }
        }
    }

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");

    // Separate base URL and path
    Span<const char> inst_key;
    const char *inst_path;
    {
        Size offset = SplitStr(request.url + 1, '/').len + 1;

        if (request.url[offset] != '/') {
            if (offset == 1) {
                io->AttachError(404);
            } else {
                const char *redirect = Fmt(&io->allocator, "%1/", request.url).ptr;
                io->AddHeader("Location", redirect);
                io->AttachNothing(301);
            }
            return;
        }

        inst_key = MakeSpan(request.url + 1, offset - 1);
        inst_path = request.url + offset;
    }

    if (inst_key == "admin") {
        if (TestStr(inst_path, "/api/instances/list") && request.method == http_RequestMethod::Get) {
            HandleListInstances(request, io);
        } else if (TestStr(inst_path, "/api/users/list") && request.method == http_RequestMethod::Get) {
            HandleListUsers(request, io);
        } else {
            io->AttachError(404);
        }
    } else {
        InstanceGuard *guard;
        {
            std::shared_lock<std::shared_mutex> lock(instances_mutex);
            guard = instances_map.FindValue(inst_key, nullptr);
        }
        if (!guard || !guard->valid) {
            io->AttachError(404);
            return;
        }

        InstanceData *instance = guard->Ref();
        io->AddFinalizer([=]() { guard->Unref(); });

        // Try application and static assets
        if (request.method == http_RequestMethod::Get) {
            const AssetInfo *asset;

            // Instance asset?
            if (HandleFileGet(instance, request, io))
                return;

            if (TestStr(inst_path, "/") || StartsWith(inst_path, "/app/") || StartsWith(inst_path, "/main/")) {
                asset = instance->assets_map.FindValue("/static/goupile.html", nullptr);
                RG_ASSERT(asset);
            } else {
                asset = instance->assets_map.FindValue(inst_path, nullptr);
            }

            if (asset) {
                const char *client_etag = request.GetHeaderValue("If-None-Match");

                if (client_etag && TestStr(client_etag, instance->etag)) {
                    MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
                    io->AttachResponse(304, response);
                } else {
                    const char *mimetype = http_GetMimeType(GetPathExtension(asset->name));
                    io->AttachBinary(200, asset->data, mimetype, asset->compression_type);

                    io->AddCachingHeaders(goupile_domain.config.max_age, instance->etag);
                    if (asset->source_map) {
                        io->AddHeader("SourceMap", asset->source_map);
                    }
                }

                return;
            }
        }

        // And last (but not least), API endpoints
        if (TestStr(inst_path, "/api/events") && request.method == http_RequestMethod::Get) {
            HandleEvents(instance, request, io);
        } else if (TestStr(inst_path, "/api/user/profile") && request.method == http_RequestMethod::Get) {
            HandleUserProfile(instance, request, io);
        } else if (TestStr(inst_path, "/api/user/login") && request.method == http_RequestMethod::Post) {
            HandleUserLogin(instance, request, io);
        } else if (TestStr(inst_path, "/api/user/logout") && request.method == http_RequestMethod::Post) {
            HandleUserLogout(instance, request, io);
        } else if (TestStr(inst_path, "/api/files/list") && request.method == http_RequestMethod::Get) {
             HandleFileList(instance, request, io);
        } else if (TestStr(inst_path, "/api/files/static") && request.method == http_RequestMethod::Get) {
            HandleFileStatic(instance, request, io);
        } else if (StartsWith(inst_path, "/files/") && request.method == http_RequestMethod::Put) {
            HandleFilePut(instance, request, io);
        } else if (StartsWith(inst_path, "/files/") && request.method == http_RequestMethod::Delete) {
            HandleFileDelete(instance, request, io);
        } else if (TestStr(inst_path, "/api/records/load") && request.method == http_RequestMethod::Get) {
            HandleRecordLoad(instance, request, io);
        } else if (TestStr(inst_path, "/api/records/columns") && request.method == http_RequestMethod::Get) {
            HandleRecordColumns(instance, request, io);
        } else if (TestStr(inst_path, "/api/records/sync") && request.method == http_RequestMethod::Post) {
            HandleRecordSync(instance, request, io);
        } else {
            io->AttachError(404);
        }
    }
}

int Main(int argc, char **argv)
{
    const char *config_filename = "goupile.ini";

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %3)%!0)",
                FelixTarget, config_filename, goupile_domain.config.http.port);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %2 (domain: %!..+%3%!0, instance: %!..+%4%!0)",
                FelixTarget, FelixVersion, DomainVersion, InstanceVersion);
        return 0;
    }

    // Find config filename
    {
        OptionParser opt(argc, argv, (int)OptionParser::Flag::SkipNonOptions);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            }
        }
    }

    // Load domain information
    if (!goupile_domain.Open(config_filename))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &goupile_domain.config.http.port))
                    return 1;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        // We may have changed some stuff (such as HTTP port), so revalidate
        if (!goupile_domain.config.Validate())
            return 1;
    }

    if (!InitInstances())
        return 1;
    InitJS();

    // Run!
    http_Daemon daemon;
    if (!daemon.Start(goupile_domain.config.http, HandleRequest))
        return 1;
#ifndef _WIN32
    if (goupile_domain.config.http.sock_type == SocketType::Unix) {
        LogInfo("Listening on socket '%1' (Unix stack)", goupile_domain.config.http.unix_path);
    } else
#endif
    LogInfo("Listening on port %1 (%2 stack)",
            goupile_domain.config.http.port, SocketTypeNames[(int)goupile_domain.config.http.sock_type]);

    while (!WaitForInterruption(30000)) {
        InitInstances();
    }

    daemon.Stop();
    LogInfo("Exit");

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
