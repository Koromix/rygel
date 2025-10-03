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

namespace K {

extern "C" const AssetInfo RekkordPng;

static bool run = true;
static std::unique_ptr<gui_TrayIcon> tray;

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
    // tray->OnContext(UpdateTray);

    // From here on, don't quit abruptly
    WaitEvents(0);

    int status = 0;
    while (run) {
        WaitSource sources[] = {
            tray->GetWaitSource()
        };

        WaitResult ret = WaitEvents(sources, -1);

        if (ret == WaitResult::Exit) {
            LogInfo("Exit requested");
            run = false;
        } else if (ret == WaitResult::Interrupt) {
            LogInfo("Process interrupted");
            status = 1;
            run = false;
        } else if (ret == WaitResult::Error) {
            status = 1;
            run = false;
        }

        tray->ProcessEvents();
    }

    return status;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
