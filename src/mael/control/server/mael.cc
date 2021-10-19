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
#include "../../../../vendor/libhs/libhs.h"
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <unistd.h>
#endif
#include <thread>

namespace RG {

static Config mael_config;

static hs_monitor *monitor = nullptr;
static std::thread monitor_thread;
// static hs_device *monitor_dev = nullptr;
#ifdef _WIN32
static HANDLE monitor_event;
#else
static int monitor_pfd[2] = {-1, -1};
#endif

static int DeviceCallback(hs_device *dev, void *)
{
    const char *event = "?";

    switch (dev->status) {
        case HS_DEVICE_STATUS_DISCONNECTED: { event = "Remove"; } break;
        case HS_DEVICE_STATUS_ONLINE: { event = "Add"; } break;
    }

    PrintLn("%1 %2@%3 %4:%5 (%6)",
            event, dev->location, dev->iface_number, FmtHex(dev->vid).Pad0(-4),
            FmtHex(dev->pid).Pad0(-4), hs_device_type_strings[dev->type]);
    PrintLn("  - device key:    %1", dev->key);
    PrintLn("  - device node:   %1", dev->path);
    if (dev->manufacturer_string) {
        PrintLn("  - manufacturer:  %1", dev->manufacturer_string);
    }
    if (dev->product_string) {
        PrintLn("  - product:       %1", dev->product_string);
    }
    if (dev->serial_number_string) {
        PrintLn("  - serial number: %1", dev->serial_number_string);
    }

    return 0;
}

// XXX: Do something when an error happens and the thread fails
static void RunMonitorThread()
{
    hs_poll_source sources[2] = {
        {hs_monitor_get_poll_handle(monitor)},
#ifdef _WIN32
        {monitor_event}
#else
        {monitor_pfd[0]}
#endif
    };

    do {
        if (hs_monitor_refresh(monitor, DeviceCallback, nullptr) < 0)
            return;
        if (hs_poll(sources, RG_LEN(sources), -1) < 0)
            return;
    } while (!sources[1].ready);
}

static void StopMonitor();
static bool InitMonitor()
{
    RG_DEFER_N(err_guard) { StopMonitor(); };

#ifdef _WIN32
    monitor_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!monitor_event) {
        LogError("CreateEvent() failed: %1", GetWin32ErrorString());
        return false;
    }
#else
    if (!CreatePipe(monitor_pfd))
        return false;
#endif

    if (hs_monitor_new(nullptr, 0, &monitor) < 0)
        return false;
    if (hs_monitor_start(monitor) < 0)
        return false;

    if (hs_monitor_list(monitor, DeviceCallback, nullptr) < 0)
        return false;
    monitor_thread = std::thread(RunMonitorThread);

    err_guard.Disable();
    return true;
}

static void StopMonitor()
{
    // hs_device_unref(monitor_dev);
    // monitor_dev = nullptr;

    if (monitor) {
#ifdef _WIN32
        SetEvent(monitor_event);
#else
        char dummy = 0;
        write(monitor_pfd[1], &dummy, 1);
#endif

        if (monitor_thread.joinable()) {
            monitor_thread.join();
        }
        monitor_thread = {};

        hs_monitor_stop(monitor);
    }

    hs_monitor_free(monitor);
    monitor = nullptr;

#ifdef _WIN32
    if (monitor_event) {
        CloseHandle(monitor_event);
        monitor_event = nullptr;
    }
#else
    close(monitor_pfd[0]);
    close(monitor_pfd[1]);
    monitor_pfd[0] = -1;
    monitor_pfd[1] = -1;
#endif
}

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

    // Init device access
    hs_log_set_handler([](hs_log_level level, int, const char *msg, void *) {
        switch (level) {
            case HS_LOG_ERROR:
            case HS_LOG_WARNING: { LogError("%1", msg); } break;
            case HS_LOG_DEBUG: { LogDebug("%1", msg); } break;
        }
    }, nullptr);
    if (!InitMonitor())
        return 1;
    RG_DEFER { StopMonitor(); };

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
