// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
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

DomainData goupile_domain;

static HeapArray<InstanceData> instances;
static HashMap<Span<const char>, InstanceData *> instances_map;

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
        json.String(Fmt(buf, "%1%2", instance->config.base_url, asset.name + 1).ptr);
    }
    json.EndArray();

    json.Finish(io);
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
    if (ReloadAssets()) {
        for (InstanceData &instance: instances) {
            instance.InitAssets();
        }
    }

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");

    // Decode URL to instance/path pair
    InstanceData *instance;
    const char *path;
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

        Span<const char> base_url = MakeSpan(request.url, offset + 1);

        instance = instances_map.FindValue(base_url, nullptr);
        if (!instance) {
            io->AttachError(404);
            return;
        }
        path = request.url + offset;
    }

    // Try application and static assets
    if (request.method == http_RequestMethod::Get) {
        const AssetInfo *asset;

        // Instance asset?
        if (HandleFileGet(instance, request, io))
            return;

        if (TestStr(path, "/") || StartsWith(path, "/app/") || StartsWith(path, "/main/")) {
            asset = instance->assets_map.FindValue("/static/goupile.html", nullptr);
            RG_ASSERT(asset);
        } else {
            asset = instance->assets_map.FindValue(path, nullptr);
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
    if (TestStr(path, "/api/events") && request.method == http_RequestMethod::Get) {
        HandleEvents(instance, request, io);
    } else if (TestStr(path, "/api/user/profile") && request.method == http_RequestMethod::Get) {
        HandleUserProfile(instance, request, io);
    } else if (TestStr(path, "/api/user/login") && request.method == http_RequestMethod::Post) {
        HandleUserLogin(instance, request, io);
    } else if (TestStr(path, "/api/user/logout") && request.method == http_RequestMethod::Post) {
        HandleUserLogout(instance, request, io);
    } else if (TestStr(path, "/api/files/list") && request.method == http_RequestMethod::Get) {
         HandleFileList(instance, request, io);
    } else if (TestStr(path, "/api/files/static") && request.method == http_RequestMethod::Get) {
        HandleFileStatic(instance, request, io);
    } else if (StartsWith(path, "/files/") && request.method == http_RequestMethod::Put) {
        HandleFilePut(instance, request, io);
    } else if (StartsWith(path, "/files/") && request.method == http_RequestMethod::Delete) {
        HandleFileDelete(instance, request, io);
    } else if (TestStr(path, "/api/records/load") && request.method == http_RequestMethod::Get) {
        HandleRecordLoad(instance, request, io);
    } else if (TestStr(path, "/api/records/columns") && request.method == http_RequestMethod::Get) {
        HandleRecordColumns(instance, request, io);
    } else if (TestStr(path, "/api/records/sync") && request.method == http_RequestMethod::Post) {
        HandleRecordSync(instance, request, io);
    } else {
        io->AttachError(404);
    }
}

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    const char *config_filename = "goupile.ini";
    bool migrate = false;

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %3)%!0

        %!..+--migrate%!0                Migrate database if needed)",
                FelixTarget, config_filename, goupile_domain.config.http.port);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %2", FelixTarget, FelixVersion);
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
            } else if (opt.Test("--migrate")) {
                migrate = true;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        // We may have changed some stuff (such as HTTP port), so revalidate
        if (!goupile_domain.config.Validate())
            return 1;
    }

    // Load instances
    {
        sq_Statement stmt;
        if (!goupile_domain.db.Prepare("SELECT instance FROM dom_instances;", &stmt))
            return 1;

        while (stmt.Next()) {
            InstanceData *instance = instances.AppendDefault();

            const char *key = (const char *)sqlite3_column_text(stmt, 0);
            const char *filename = Fmt(&temp_alloc, "%1%/%2.db",
                                       goupile_domain.config.instances_directory, key).ptr;

            if (migrate && !MigrateInstance(filename))
                return 1;
            if (!instance->Open(key, filename))
                return 1;
        }
        if (!stmt.IsValid())
            return 1;

        for (InstanceData &instance: instances) {
            instances_map.Set(instance.config.base_url, &instance);
        }
    }

    // Init JS
    InitJS();

    // Run!
    http_Daemon daemon;
    if (!daemon.Start(goupile_domain.config.http, HandleRequest))
        return 1;
    LogInfo("Listening on port %1 (%2 stack)",
            goupile_domain.config.http.port, IPStackNames[(int)goupile_domain.config.http.ip_stack]);

    WaitForInterruption();

    daemon.Stop();
    for (InstanceData &instance: instances) {
        instance.Close();
    }
    goupile_domain.Close();

    LogInfo("Exit");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
