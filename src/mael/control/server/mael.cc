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

#include "../../../core/libcc/libcc.hh"
#include "config.hh"
#include "../../../core/libnet/libnet.hh"

namespace RG {

static Config mael_config;

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
    if (mael_config.require_host) {
        const char *host = request.GetHeaderValue("Host");

        if (!host) {
            LogError("Request is missing required Host header");
            io->AttachError(400);
            return;
        }
        if (!TestStr(host, mael_config.require_host)) {
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

    // Serve page
    if (TestStr(request.url, "/")) {
        const char *text = Fmt(&io->allocator, "Mael %1", FelixVersion).ptr;
        io->AttachText(200, text);

        return;
    }

    io->AttachError(404);
}

int Main(int argc, char **argv)
{
    // Options
    const char *config_filename = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %2)%!0)",
                FelixTarget, mael_config.http.port);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
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
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Load config file
    if (config_filename && !LoadConfig(config_filename, &mael_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &mael_config.http.port))
                    return 1;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

    // Run!
    LogInfo("Init HTTP server");
    http_Daemon daemon;
    if (!daemon.Start(mael_config.http, HandleRequest))
        return 1;
    if (mael_config.http.sock_type == SocketType::Unix) {
        LogInfo("Listening on socket '%1' (Unix stack)", mael_config.http.unix_path);
    } else {
        LogInfo("Listening on port %1 (%2 stack)",
                mael_config.http.port, SocketTypeNames[(int)mael_config.http.sock_type]);
    }

#ifdef __linux__
    if (!NotifySystemd())
        return 1;
#endif

    // Run until exit
    WaitForInterrupt();

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
