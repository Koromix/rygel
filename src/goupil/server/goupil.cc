// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../lib/libmicrohttpd/src/include/microhttpd.h"
#include "../../libdrd/libdrd.hh"

#include "config.hh"

static Config goupil_config;

static int HandleHttpConnection(void *, MHD_Connection *conn, const char *url, const char *method,
                                const char *, const char *, size_t *, void **)
{
    MHD_Response *response = MHD_create_response_from_buffer(5, (void *)"Hello", MHD_RESPMEM_PERSISTENT);
    return MHD_queue_response(conn, 200, response);
}

int main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil [options]

Options:
    -C, --config_file <file>     Set configuration file
                                 (default: <executable_dir>%/profile%/goupil.ini)

        --port <port>            Change web server port
                                 (default: %1))
        --base_url <url>         Change base URL
                                 (default: %2))",
                goupil_config.port, goupil_config.base_url);
    };

    // Find config filename
    const char *config_filename = nullptr;
    {
        OptionParser opt(argc, argv, (int)OptionParser::Flag::SkipNonOptions);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
                config_filename = opt.current_value;
            }
        }

        if (!config_filename) {
            const char *app_directory = GetApplicationDirectory();
            if (app_directory) {
                const char *test_filename = Fmt(&goupil_config.str_alloc, "%1%/profile/goupil.ini", app_directory).ptr;
                if (TestPath(test_filename, FileType::File)) {
                    config_filename = test_filename;
                }
            }
        }
    }

    // Load config file
    if (config_filename && !LoadConfig(config_filename, &goupil_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseDec(opt.current_value, &goupil_config.port))
                    return 1;
            } else if (opt.Test("--base_url", OptionType::Value)) {
                goupil_config.base_url = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    // Configuration errors
    {
        bool valid = true;

        if (!goupil_config.profile_directory) {
            LogError("Profile directory is missing");
            valid = false;
        }
        if (goupil_config.port < 1 || goupil_config.port > UINT16_MAX) {
            LogError("HTTP port %1 is invalid (range: 1 - %2)", goupil_config.port, UINT16_MAX);
            valid = false;
        }
        if (goupil_config.threads < 0 || goupil_config.threads > 128) {
            LogError("HTTP threads %1 is invalid (range: 0 - 128)", goupil_config.threads);
            valid = false;
        }
        if (goupil_config.base_url[0] != '/' ||
                goupil_config.base_url[strlen(goupil_config.base_url) - 1] != '/') {
            LogError("Base URL '%1' does not start and end with '/'", goupil_config.base_url);
            valid = false;
        }

        if (!valid)
            return 1;
    }

    MHD_Daemon *daemon;
    {
        int flags = MHD_USE_AUTO_INTERNAL_THREAD | MHD_USE_ERROR_LOG;
        LocalArray<MHD_OptionItem, 16> mhd_options;
        switch (goupil_config.ip_version) {
            case Config::IPVersion::Dual: { flags |= MHD_USE_DUAL_STACK; } break;
            case Config::IPVersion::IPv4: {} break;
            case Config::IPVersion::IPv6: { flags |= MHD_USE_IPv6; } break;
        }
        if (!goupil_config.threads) {
            flags |= MHD_USE_THREAD_PER_CONNECTION;
        } else if (goupil_config.threads > 1) {
            mhd_options.Append({MHD_OPTION_THREAD_POOL_SIZE, goupil_config.threads});
        }
        mhd_options.Append({MHD_OPTION_END, 0, nullptr});
#ifndef NDEBUG
        flags |= MHD_USE_DEBUG;
#endif

        daemon = MHD_start_daemon(flags, (int16_t)goupil_config.port, nullptr, nullptr,
                                  HandleHttpConnection, nullptr,
                                  MHD_OPTION_ARRAY, mhd_options.data, MHD_OPTION_END);
        if (!daemon)
            return 1;
    }
    DEFER { MHD_stop_daemon(daemon); };

    LogInfo("Listening on port %1", MHD_get_daemon_info(daemon, MHD_DAEMON_INFO_BIND_PORT)->port);

    // Run
    {
#ifdef _WIN32
        static HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        Assert(event);

        SetConsoleCtrlHandler([](DWORD) {
            SetEvent(event);
            return (BOOL)TRUE;
        }, TRUE);

        WaitForSingleObject(event, INFINITE);
#else
        static volatile bool run = true;

        signal(SIGINT, [](int) { run = false; });
        signal(SIGTERM, [](int) { run = false; });

        while (run) {
            pause();
        }
#endif
    }

    LogInfo("Exit");
    return 0;
}
