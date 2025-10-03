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

#include "src/core/base/base.hh"
#include "src/core/wrap/json.hh"
#include "src/core/gui/tray.hh"

#include <sys/socket.h>

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
static int rekkord_fd = -1;

static HeapArray<ItemData> items;
static BlockAllocator items_alloc;

static std::unique_ptr<gui_TrayIcon> tray;

static bool HandleServerData()
{
    BlockAllocator temp_alloc;

    const auto read = [&](Span<uint8_t> out_buf) {
        Size received = recv(rekkord_fd, out_buf.ptr, out_buf.len, 0);
        if (received < 0) {
            LogError("Failed to receive data from server: %1", strerror(errno));
        }
        return received;
    };

    StreamReader reader(read, "<server>");
    json_Parser json(&reader, &temp_alloc);

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
    tray->AddAction(T("&About"), []() { system("xdg-open https://rekkord.org"); });
    tray->AddSeparator();
    tray->AddAction(T("&Exit"), []() {
        run = false;
        InterruptWait();
    });
}

int Main(int argc, char **argv)
{
    // Options
    const char *socket_filename = "/run/rekkord.sock";

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
        rekkord_fd = CreateSocket(SocketType::Unix, SOCK_STREAM);
        if (rekkord_fd < 0)
            return 1;
        K_DEFER { close(rekkord_fd); };

        if (!ConnectUnixSocket(rekkord_fd, socket_filename))
            return 1;

        // React to main service and D-Bus events
        while (run) {
            WaitSource sources[] = {
                { rekkord_fd, -1 },
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
