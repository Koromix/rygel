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
int RunCheck(Span<const char *> arguments);

int RunSnapshots(Span<const char *> arguments);
int RunChannels(Span<const char *> arguments);
int RunList(Span<const char *> arguments);
int RunMount(Span<const char *> arguments);

int RunChangeCID(Span<const char *> arguments);
int RunResetCache(Span<const char *> arguments);

const char *const CommonOptions =
R"(Common options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL
    %!..+-u, --user username%!0            Set repository username
    %!..+-K, --key_file filename%!0        Use master key instead of username/password)";

rk_Config rekkord_config;

static const char *config_filename = nullptr;

bool HandleCommonOption(OptionParser &opt)
{
    if (opt.Test("-C", "--config_file", OptionType::Value)) {
        // Already handled
    } else if (opt.Test("-R", "--repository", OptionType::Value)) {
        if (!rk_DecodeURL(opt.current_value, &rekkord_config))
            return 1;
    } else if (opt.Test("-u", "--username", OptionType::Value)) {
        rekkord_config.username = opt.current_value;
    } else if (opt.Test("-K", "--key_file", OptionType::Value)) {
        rekkord_config.key_filename = opt.current_value;
    } else if (opt.Test("-j", "--threads", OptionType::Value)) {
        if (!ParseInt(opt.current_value, &rekkord_config.threads))
            return 1;
        if (rekkord_config.threads < 1) {
            LogError("Threads count cannot be < 1");
            return 1;
        }
    } else {
        opt.LogUnknownError();
        return false;
    }

    return true;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [](StreamWriter *st) {
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
    %!..+check%!0                          Check snapshots and blobs

Exploration commands:

    %!..+snapshots%!0                      List known snapshots
    %!..+channels%!0                       Show status of snapshot channels
    %!..+list%!0                           List snapshot or directory children
    %!..+mount%!0                          Mount repository readonly as user filesystem

Advanced commands:

    %!..+change_cid%!0                     Change repository cache ID (CID)
    %!..+reset_cache%!0                    Reset or rebuild local repository cache
    %!..+check_blobs%!0                    Detect corrupt blobs

Use %!..+%1 help command%!0 or %!..+%1 command --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(StdErr);
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
            cmd = arguments[0];
            arguments[0] = (cmd[0] == '-') ? cmd : "--help";
        } else {
            print_usage(StdOut);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Find config filename
    {
        OptionParser opt(arguments, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                // Don't try to load anything in this case
                config_filename = nullptr;
                break;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

#define HANDLE_COMMAND(Cmd, Func) \
        do { \
            if (TestStr(cmd, RG_STRINGIFY(Cmd))) { \
                if (config_filename && !rk_LoadConfig(config_filename, &rekkord_config)) \
                    return 1; \
                 \
                return Func(arguments); \
            } \
        } while (false)

    HANDLE_COMMAND(init, RunInit);
    HANDLE_COMMAND(add_user, RunAddUser);
    HANDLE_COMMAND(delete_user, RunDeleteUser);
    HANDLE_COMMAND(list_users, RunListUsers);
    HANDLE_COMMAND(save, RunSave);
    HANDLE_COMMAND(restore, RunRestore);
    HANDLE_COMMAND(check, RunCheck);
    HANDLE_COMMAND(snapshots, RunSnapshots);
    HANDLE_COMMAND(channels, RunChannels);
    HANDLE_COMMAND(list, RunList);
    HANDLE_COMMAND(mount, RunMount);
    HANDLE_COMMAND(change_cid, RunChangeCID);
    HANDLE_COMMAND(reset_cache, RunResetCache);

#undef HANDLE_COMMAND

    LogError("Unknown command '%1'", cmd);
    return 1;
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
