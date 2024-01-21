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
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/libcc/libcc.hh"
#include "src/core/libnet/curl.hh"
#include "rekord.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#ifndef _WIN32
    #include <sys/time.h>
    #include <sys/resource.h>
#endif

namespace RG {

int RunInit(Span<const char *> arguments);
int RunExportKey(Span<const char *> arguments);
int RunChangeID(Span<const char *> arguments);
int RunAddUser(Span<const char *> arguments);
int RunDeleteUser(Span<const char *> arguments);
int RunListUsers(Span<const char *> arguments);

int RunPut(Span<const char *> arguments);
int RunGet(Span<const char *> arguments);

int RunSnapshots(Span<const char *> arguments);
int RunList(Span<const char *> arguments);

bool FindAndLoadConfig(Span<const char *> arguments, rk_Config *out_config)
{
    OptionParser opt(arguments, OptionMode::Skip);

    const char *config_filename = getenv("REKORD_CONFIG_FILE");

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

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

Management commands:
    %!..+init%!0                         Init new backup repository

    %!..+export_key%!0                   Export master repository key
    %!..+change_id%!0                    Change repository cache ID

    %!..+add_user%!0                     Add user
    %!..+delete_user%!0                  Delete user
    %!..+list_users%!0                   List repository users

Snapshot commands:
    %!..+put%!0                          Store directory or file and make snapshot
    %!..+get%!0                          Get and decrypt snapshot, directory or file

Exploration commands:
    %!..+snapshots%!0                    List known snapshots
    %!..+list%!0                         List snapshot or directory children

Use %!..+%1 help <command>%!0 or %!..+%1 <command> --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(stderr);
        PrintLn(stderr);
        LogError("No command provided");
        return 1;
    }

#ifdef _WIN32
    _setmaxstdio(4096);
#else
    {
        const rlim_t max_nofile = 16384;
        struct rlimit lim;

        // Increase maximum number of open file descriptors
        if (getrlimit(RLIMIT_NOFILE, &lim) >= 0) {
            if (lim.rlim_cur < max_nofile) {
                lim.rlim_cur = std::min(max_nofile, lim.rlim_max);

                if (setrlimit(RLIMIT_NOFILE, &lim) >= 0) {
                    if (lim.rlim_cur < max_nofile) {
                        LogError("Maximum number of open descriptors is low: %1 (recommended: %2)", lim.rlim_cur, max_nofile);
                    }
                } else {
                    LogError("Could not raise RLIMIT_NOFILE to %1: %2", max_nofile, strerror(errno));
                }
            }
        } else {
            LogError("getrlimit(RLIMIT_NOFILE) failed: %1", strerror(errno));
        }
    }
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
            print_usage(stdout);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    if (TestStr(cmd, "init")) {
        return RunInit(arguments);
    } else if (TestStr(cmd, "export_key")) {
        return RunExportKey(arguments);
    } else if (TestStr(cmd, "change_id")) {
        return RunChangeID(arguments);
    } else if (TestStr(cmd, "add_user")) {
        return RunAddUser(arguments);
    } else if (TestStr(cmd, "delete_user")) {
        return RunDeleteUser(arguments);
    } else if (TestStr(cmd, "list_users")) {
        return RunListUsers(arguments);
    } else if (TestStr(cmd, "put")) {
        return RunPut(arguments);
    } else if (TestStr(cmd, "get")) {
        return RunGet(arguments);
    } else if (TestStr(cmd, "snapshots")) {
        return RunSnapshots(arguments);
    } else if (TestStr(cmd, "list")) {
        return RunList(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
