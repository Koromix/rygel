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

int RunSetup(Span<const char *> arguments);
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

int RunAgent(Span<const char *> arguments);

int RunChangeCID(Span<const char *> arguments);
int RunResetCache(Span<const char *> arguments);

const char *const DefaultConfigDirectory = "rekkord";
const char *const DefaultConfigName = "rekkord.ini";
const char *const DefaultConfigEnv = "REKKORD_CONFIG_FILE";

const char *const CommonOptions =
R"(Common options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL
    %!..+-u, --user username%!0            Set repository username
    %!..+-K, --key_file filename%!0        Use master key instead of username/password

    %!..+-j, --threads threads%!0          Change number of threads
                                   %!D..(default: automatic)%!0)";

rk_Config rekkord_config;

bool HandleCommonOption(OptionParser &opt, bool ignore_unknown)
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
    } else if (!ignore_unknown) {
        opt.LogUnknownError();
        return false;
    }

    return !opt.TestHasFailed();
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    BlockAllocator temp_alloc;

    // Global options
    HeapArray<const char *> config_filenames;
    const char *config_filename = FindConfigFile(DefaultConfigDirectory, DefaultConfigName, &temp_alloc, &config_filenames);

    if (const char *str = GetEnv(DefaultConfigEnv); str) {
        config_filename = str;
    }

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 command [arg...]%!0

Management commands:

    %!..+setup%!0                          Run simple wizard to create basic config file
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

    %!..+agent%!0                          Run background agent

    %!..+change_cid%!0                     Change repository cache ID (CID)
    %!..+reset_cache%!0                    Reset or rebuild local repository cache

Most commands try to find a configuration file if one exists. Unless the path is explicitly defined, the first of the following config files will be used:
)", FelixTarget);

        for (const char *filename: config_filenames) {
            PrintLn(st, "    %!..+%1%!0", filename);
        }

        PrintLn(st, R"(
Use %!..+%1 help command%!0 or %!..+%1 command --help%!0 for more specific help.)", FelixTarget);
    };

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

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    const char *cmd = nullptr;
    Span<const char *> arguments = {};

    // Find command and config filename (if any)
    {
        OptionParser opt(argc, argv, OptionMode::Skip);

        for (;;) {
            if (!cmd) {
                cmd = opt.ConsumeNonOption();

                if (cmd && !arguments.len) {
                    Size pos = opt.GetPosition();
                    arguments = MakeSpan((const char **)argv + pos, argc - pos);
                }
            }

            if (!opt.Next())
                break;

            if (opt.Test("--help")) {
                static const char *HelpArguments[] = { "--help" };
                arguments = HelpArguments;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (!HandleCommonOption(opt, cmd)) {
                return 1;
            }
        }
    }

    if (!cmd) {
        if (arguments.len && TestStr(arguments[0], "--help")) {
            cmd = "help";
        } else {
            print_usage(StdErr);
            PrintLn(StdErr);
            LogError("No command provided");
            return 1;
        }
    }

    if (TestStr(cmd, "help")) {
        if (!arguments.len || arguments[0][0] == '-') {
            print_usage(StdOut);
            return 0;
        }

        static const char *HelpArguments[] = { "--help" };

        cmd = arguments[0];
        arguments = HelpArguments;

        config_filename = nullptr;
    } else if (arguments.len && TestStr(arguments[0], "--help")) {
        config_filename = nullptr;
    }

#define HANDLE_COMMAND(Cmd, Func, ReadConfig) \
        do { \
            if (TestStr(cmd, RG_STRINGIFY(Cmd))) { \
                if ((ReadConfig) && config_filename) { \
                    if (!rk_LoadConfig(config_filename, &rekkord_config)) \
                        return 1; \
                     \
                    /* Reload common options to override config file values */ \
                    OptionParser opt(argc, argv, OptionMode::Stop); \
                     \
                    while (opt.Next()) { \
                        if (opt.Test("--help")) { \
                            /* Already handled */ \
                        } else if (!HandleCommonOption(opt)) { \
                            return 1; \
                        } \
                    } \
                } \
                 \
                return Func(arguments); \
            } \
        } while (false)

    HANDLE_COMMAND(setup, RunSetup, false);
    HANDLE_COMMAND(init, RunInit, true);
    HANDLE_COMMAND(add_user, RunAddUser, true);
    HANDLE_COMMAND(delete_user, RunDeleteUser, true);
    HANDLE_COMMAND(list_users, RunListUsers, true);
    HANDLE_COMMAND(save, RunSave, true);
    HANDLE_COMMAND(restore, RunRestore, true);
    HANDLE_COMMAND(check, RunCheck, true);
    HANDLE_COMMAND(snapshots, RunSnapshots, true);
    HANDLE_COMMAND(channels, RunChannels, true);
    HANDLE_COMMAND(list, RunList, true);
    HANDLE_COMMAND(mount, RunMount, true);
    HANDLE_COMMAND(agent, RunAgent, true);
    HANDLE_COMMAND(change_cid, RunChangeCID, true);
    HANDLE_COMMAND(reset_cache, RunResetCache, true);

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
