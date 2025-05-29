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
#include "src/core/request/curl.hh"
#include "rekkord.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#if !defined(_WIN32)
    #include <sys/time.h>
#endif

namespace RG {

int RunInit(Span<const char *> arguments);
int RunAddUser(Span<const char *> arguments);
int RunDeleteUser(Span<const char *> arguments);
int RunListUsers(Span<const char *> arguments);

int RunSave(Span<const char *> arguments);
int RunRestore(Span<const char *> arguments);

int RunSnapshots(Span<const char *> arguments);
int RunChannels(Span<const char *> arguments);
int RunList(Span<const char *> arguments);
int RunMount(Span<const char *> arguments);

int RunChangeCID(Span<const char *> arguments);
int RunResetCache(Span<const char *> arguments);
int RunCheckBlobs(Span<const char *> arguments);

bool FindAndLoadConfig(Span<const char *> arguments, rk_Config *out_config)
{
    OptionParser opt(arguments, OptionMode::Skip);

    const char *config_filename = GetEnv("REKKORD_CONFIG_FILE");

    while (opt.Next()) {
        if (opt.Test("-C", "--config_file", OptionType::Value)) {
            config_filename = opt.current_value;
        }
    }

    if (config_filename && !rk_LoadConfig(config_filename, out_config))
        return false;

    return true;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [](StreamWriter *st, bool advanced) {
        PrintLn(st,
R"(Usage: %!..+%1 command [arg...]%!0

Management commands:

    %!..+init%!0                           Init new backup repository

    %!..+add_user%!0                       Add user
    %!..+delete_user%!0                    Delete user
    %!..+list_users%!0                     List repository users

Snapshot commands:

    %!..+save%!0                           Store directory or file and make snapshot
    %!..+restore%!0                        Get and decrypt snapshot, directory or file

Exploration commands:

    %!..+snapshots%!0                      List known snapshots
    %!..+channels%!0                       Show status of snapshot channels
    %!..+list%!0                           List snapshot or directory children
    %!..+mount%!0                          Mount repository readonly as user filesystem)", FelixTarget);

        if (advanced) {
            PrintLn(st, R"(
Advanced commands:

    %!..+change_cid%!0                     Change repository cache ID (CID)
    %!..+reset_cache%!0                    Reset or rebuild local repository cache
    %!..+check_blobs%!0                    Detect corrupt blobs)");
        } else {
            PrintLn(st, R"(
Advanced and low-level commands are hidden, use %!..+rekkord --help all%!0 to reveal them.)");
        }

        PrintLn(st, R"(
Use %!..+%1 help command%!0 or %!..+%1 command --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(StdErr, false);
        PrintLn(StdErr);
        LogError("No command provided");
        return 1;
    }

#if !defined(_WIN32)
    RaiseMaximumOpenFiles(16384);
#endif

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }
    if (curl_global_init(CURL_GLOBAL_ALL)) {
        LogError("Failed to initialize libcurl");
        return 1;
    }
    if (ssh_init() < 0) {
        LogError("Failed to initialize libssh");
        return 1;
    }
    RG_DEFER { ssh_finalize(); };

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);

    // Handle help and version arguments
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            if (TestStr(arguments[0], "all")) {
                print_usage(StdOut, true);
                return 0;
            } else {
                cmd = arguments[0];
                arguments[0] = (cmd[0] == '-') ? cmd : "--help";
            }
        } else {
            print_usage(StdOut, false);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    if (TestStr(cmd, "init")) {
        return RunInit(arguments);
    } else if (TestStr(cmd, "add_user")) {
        return RunAddUser(arguments);
    } else if (TestStr(cmd, "delete_user")) {
        return RunDeleteUser(arguments);
    } else if (TestStr(cmd, "list_users")) {
        return RunListUsers(arguments);
    } else if (TestStr(cmd, "save")) {
        return RunSave(arguments);
    } else if (TestStr(cmd, "restore")) {
        return RunRestore(arguments);
    } else if (TestStr(cmd, "snapshots")) {
        return RunSnapshots(arguments);
    } else if (TestStr(cmd, "channels")) {
        return RunChannels(arguments);
    } else if (TestStr(cmd, "list")) {
        return RunList(arguments);
    } else if (TestStr(cmd, "mount")) {
        return RunMount(arguments);
    } else if (TestStr(cmd, "change_cid")) {
        return RunChangeCID(arguments);
    } else if (TestStr(cmd, "reset_cache")) {
        return RunResetCache(arguments);
    } else if (TestStr(cmd, "check_blobs")) {
        return RunCheckBlobs(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

#if !defined(__linux__) && !defined(__FreeBSD__) && !defined(__OpenBSD__)

int RunMount(Span<const char *>)
{
    LogError("Mount is not supported on this platform");
    return 1;
}

#endif

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
