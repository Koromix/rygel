// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

#if defined(__linux__)

#include "src/core/base/base.hh"
#include "config.hh"
#include "light.hh"
#include "src/core/wrap/json.hh"
#include "src/core/gui/tray.hh"

#include <sys/socket.h>

namespace K {

extern "C" const AssetInfo MeesticPng;

static bool run = true;
static int meestic_fd = -1;

static HeapArray<const char *> profiles;
static BlockAllocator profiles_alloc;
static Size active_idx = -1;

static std::unique_ptr<gui_TrayIcon> tray;

static bool ApplyProfile(Size idx)
{
    LogInfo("Applying profile %1", idx);

    LocalArray<char, 128> buf;
    buf.len = Fmt(buf.data, "{\"apply\": %1}\n", idx).len;

    if (send(meestic_fd, buf.data, (size_t)buf.len, 0) < 0) {
        LogError("Failed to send message to server: %1", strerror(errno));
        return false;
    }

    return true;
}

static bool ToggleProfile(int direction)
{
    Span<const char> buf = {};

    if (direction > 0) {
        LogInfo("Applying next profile");
        buf = "{\"toggle\": \"next\"}\n";
    } else if (direction < 0) {
        LogInfo("Applying previous profile");
        buf = "{\"toggle\": \"previous\"}\n";
    } else {
        K_UNREACHABLE();
    }

    if (send(meestic_fd, buf.ptr, (size_t)buf.len, 0) < 0) {
        LogError("Failed to send message to server: %1", strerror(errno));
        return false;
    }

    return true;
}

static bool HandleServerData()
{
    BlockAllocator temp_alloc;

    const auto read = [&](Span<uint8_t> out_buf) {
        Size received = recv(meestic_fd, out_buf.ptr, out_buf.len, 0);
        if (received < 0) {
            LogError("Failed to receive data from server: %1", strerror(errno));
        }
        return received;
    };

    StreamReader reader(read, "<server>");
    json_Parser json(&reader, &temp_alloc);

    for (json.ParseObject(); json.InObject(); ) {
        Span<const char> key = json.ParseKey();

        if (key == "profiles") {
            profiles.Clear();
            profiles_alloc.Reset();

            for (json.ParseArray(); json.InArray(); ) {
                const char *name = DuplicateString(json.ParseString(), &profiles_alloc).ptr;
                profiles.Append(name);
            }

            active_idx = -1;
        } else if (key == "active") {
            if (!json.ParseInt(&active_idx))
                return false;
        } else {
            json.UnexpectedKey(key);
            return false;
        }
    }
    if (!json.IsValid()) {
        if (!reader.GetRawRead()) {
            LogError("Lost connection to server");
        }
        return false;
    }

    return true;
}

static void UpdateTray()
{
    tray->ClearMenu();

    for (Size i = 0; i < profiles.len; i++) {
        const char *profile = profiles[i];
        tray->AddAction(profile, i == active_idx, [i]() { ApplyProfile(i); });
    }

    tray->AddSeparator();
    tray->AddAction(T("&About"), []() { system("xdg-open https://koromix.dev/meestic"); });
    tray->AddSeparator();
    tray->AddAction(T("&Exit"), []() {
        run = false;
        PostWaitMessage();
    });
}

int Main(int argc, char **argv)
{
    InitLocales(TranslationTables);

    BlockAllocator temp_alloc;

    // Options
    const char *socket_filename = "/run/meestic.sock";

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 [option...]%!0

Options:

    %!..+-S, --socket_file socket%!0       Change control socket
                                   %!D..(default: %2)%!0)"),
                FelixTarget, socket_filename);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn(T("Compiler: %1"), FelixCompiler);
        return 0;
    }

    // Parse options
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-S", "--socket_file", OptionType::Value)) {
                socket_filename = opt.current_value;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    K_ASSERT(MeesticPng.compression_type == CompressionType::None);

    tray = gui_CreateTrayIcon(MeesticPng.data);
    if (!tray)
        return 1;
    tray->OnScroll(ToggleProfile);
    tray->OnContext(UpdateTray);

    // From here on, don't quit abruptly
    WaitEvents(0);

    int status = 0;
    while (run) {
        meestic_fd = CreateSocket(SocketType::Unix, SOCK_STREAM);
        if (meestic_fd < 0)
            return 1;
        K_DEFER { close(meestic_fd); };

        if (!ConnectUnixSocket(meestic_fd, socket_filename))
            return 1;

        // React to main service and D-Bus events
        while (run) {
            WaitSource sources[] = {
                { meestic_fd, -1 },
                tray->GetWaitSource()
            };

            uint64_t ready = 0;
            WaitResult ret = WaitEvents(sources, -1, &ready);

            if (ret == WaitResult::Exit) {
                LogInfo("Exit requested");
                run = false;
            } else if (ret == WaitResult::Interrupt) {
                LogInfo("Process interrupted");
                status = 1;
                run = false;
            }

            if ((ready & 1) && !HandleServerData()) {
                WaitDelay(3000);
                break;
            }

            tray->ProcessEvents();
        }
    }

    return status;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }

#endif
