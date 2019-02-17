// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WIN32
    #include <dlfcn.h>
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

#include "../../libcc/libcc.hh"
#include "../../libdrd/libdrd.hh"
#include "config.hh"
#include "../../wrappers/http.hh"

static Config goupil_config;

static int HandleRequest(const http_Request &request, http_Response *out_response)
{
    *out_response = MHD_create_response_from_buffer(5, (void *)"Hello", MHD_RESPMEM_PERSISTENT);
    return 200;
}

int main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil [options]

Options:
    -C, --config_file <file>     Set configuration file
    -P, --profile_dir <dir>      Set profile directory

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
            } else if (opt.Test("-P", "--profile_dir", OptionType::Value)) {
                goupil_config.profile_directory = opt.current_value;
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
    if (!goupil_config.profile_directory) {
        LogError("Profile directory is missing");
        return 1;
    }

    // Run!
    http_Daemon daemon;
    daemon.handle_func = HandleRequest;
    if (!daemon.Start(goupil_config.ip_stack, goupil_config.port, goupil_config.threads,
                      goupil_config.base_url))
        return 1;
    LogInfo("Listening on port %1 (%2 stack)",
            goupil_config.port, IPStackNames[(int)goupil_config.ip_stack]);

    WaitForConsoleInterruption();

    LogInfo("Exit");
    return 0;
}
