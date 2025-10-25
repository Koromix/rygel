// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/base/base.hh"
#include "src/core/base/tower.hh"
#include "src/core/wrap/json.hh"
#include "src/core/gui/tray.hh"

#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <shellapi.h>
#endif

namespace K {

struct ItemData {
    const char *channel;
    int clock;
    int days;
    HeapArray<const char *> paths;

    int64_t timestamp;
    bool success;
};

extern "C" const AssetInfo RekkordPng;

static bool run = true;
static TowerClient client;

static HeapArray<ItemData> items;
static BlockAllocator items_alloc;

static std::unique_ptr<gui_TrayIcon> tray;

static bool HandleServerData(StreamReader *reader)
{
    BlockAllocator temp_alloc;

    json_Parser json(reader, &temp_alloc);

    for (json.ParseObject(); json.InObject(); ) {
        Span<const char> key = json.ParseKey();

        if (key == "items") {
            items.Clear();
            items_alloc.Reset();

            for (json.ParseArray(); json.InArray(); ) {
                ItemData item = {};

                for (json.ParseObject(); json.InObject(); ) {
                    Span<const char> key = json.ParseKey();

                    if (key == "channel") {
                        item.channel = DuplicateString(json.ParseString(), &items_alloc).ptr;
                    } else if (key == "clock") {
                        json.ParseInt(&item.clock);
                    } else if (key == "days") {
                        json.ParseInt(&item.days);
                    } else if (key == "timestamp") {
                        json.ParseInt(&item.timestamp);
                    } else if (key == "success") {
                        json.ParseBool(&item.success);
                    } else {
                        json.UnexpectedKey(key);
                        return false;
                    }
                }

                items.Append(item);
            }
        } else {
            json.UnexpectedKey(key);
            return false;
        }
    }
    if (!json.IsValid())
        return false;

    return true;
}

static void UpdateTray()
{
    tray->ClearMenu();

    for (const ItemData &item: items) {
        char label[512];

        if (item.timestamp && item.success) {
            TimeSpec spec = DecomposeTimeLocal(item.timestamp);
            Fmt(label, T("Plan %1 : last run %2"), item.channel, FmtTimeNice(spec));
        } else if (item.timestamp) {
            TimeSpec spec = DecomposeTimeLocal(item.timestamp);
            Fmt(label, T("Plan %1 : error occured at %2"), item.channel, FmtTimeNice(spec));
        } else {
            Fmt(label, T("Plan %1 : never executed"), item.channel);
        }

        tray->AddAction(label, []() {});
    }

    tray->AddSeparator();
    tray->AddAction(T("&About"), []() {
#if defined(_WIN32)
        ShellExecuteA(nullptr, "open", "https://rekkord.org", nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
        K_IGNORE system("open https://rekkord.org");
#else
        K_IGNORE system("xdg-open https://rekkord.org");
#endif
    });
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
    const char *socket_filename = GetControlSocketPath(ControlScope::System, "rekkord", &temp_alloc);

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 [option...]%!0

Options:

    %!..+-S, --socket_file socket%!0       Change control socket
                                   %!D..(default: %2)%!0)"),
                FelixTarget, socket_filename);
    };

    // Parse arguments
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

        opt.LogUnusedArguments();
    }

    K_ASSERT(RekkordPng.compression_type == CompressionType::None);

    tray = gui_CreateTrayIcon(RekkordPng.data);
    if (!tray)
        return 1;
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
