// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__linux__)

#include "lib/native/base/base.hh"
#include "lib/native/base/tower.hh"
#include "config.hh"
#include "light.hh"
#include "lib/native/wrap/json.hh"
#include "lib/native/gui/tray.hh"

#include <sys/socket.h>

namespace K {

extern "C" const AssetInfo MeesticPng;

static bool run = true;
static TowerClient client;

static HeapArray<const char *> profiles;
static BlockAllocator profiles_alloc;
static Size active_idx = -1;

static std::unique_ptr<gui_TrayIcon> tray;

static bool ApplyProfile(Size idx)
{
    LocalArray<char, 128> buf;
    buf.len = Fmt(buf.data, "{\"apply\": %1}\n", idx).len;

    return client.Send(buf);
}

static bool ToggleProfile(int direction)
{
    Span<const char> buf = {};

    if (direction > 0) {
        buf = "{\"toggle\": \"next\"}\n";
    } else if (direction < 0) {
        buf = "{\"toggle\": \"previous\"}\n";
    } else {
        K_UNREACHABLE();
    }

    return client.Send(buf);
}

static bool HandleServerData(StreamReader *reader)
{
    BlockAllocator temp_alloc;

    json_Parser json(reader, &temp_alloc);

    Size prev_idx = active_idx;

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
    if (!json.IsValid())
        return false;

    if (active_idx != prev_idx) {
        LogInfo("Applying profile %1", active_idx);
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
    tray->AddAction(T("&About"), []() { K_IGNORE system("xdg-open https://koromix.dev/meestic"); });
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
    const char *socket_filename = GetControlSocketPath(ControlScope::System, "meestic", &temp_alloc);

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
        if (!client.Connect(socket_filename))
            return 1;
        client.Start(HandleServerData);

        if (!client.Send("{\"refresh\": true}\n")) {
            WaitDelay(3000);
            continue;
        }

        // React to main service and D-Bus events
        while (run) {
            WaitSource sources[] = {
                client.GetWaitSource(),
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

            if (!client.Process()) {
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
