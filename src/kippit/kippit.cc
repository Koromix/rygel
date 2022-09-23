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
#include "disk.hh"
#include "repository.hh"
#include "src/core/libpasswd/libpasswd.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#include "vendor/curl/include/curl/curl.h"

namespace RG {

static bool GeneratePassword(Span<char> out_pwd)
{
    static const char *const AllowedChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#%&()*+,-./:;<=>?[]_{}|";

    for (Size i = 0; i < 1000; i++) {
        Fmt(out_pwd, "%1", FmtRandom(out_pwd.len - 1, AllowedChars));

        if (pwd_CheckPassword(out_pwd))
            return true;
    }

    LogError("Failed to generate secure password");
    return false;
}

static int RunInit(Span<const char *> arguments)
{
    // Options
    const char *repo_directory = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 init <dir>%!0)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        repo_directory = opt.ConsumeNonOption();
    }

    if (!repo_directory) {
        LogError("Missing repository directory");
        return 1;
    }

    // Generate repository passwords
    char full_pwd[33] = {};
    char write_pwd[33] = {};
    if (!GeneratePassword(full_pwd))
        return 1;
    if (!GeneratePassword(write_pwd))
        return 1;

    if (!kt_CreateLocalDisk(repo_directory, full_pwd, write_pwd))
        return 1;

    LogInfo("Repository: %!..+%1%!0", TrimStrRight(repo_directory, RG_PATH_SEPARATORS));
    LogInfo();
    LogInfo("Default full password: %!..+%1%!0", full_pwd);
    LogInfo("  write-only password: %!..+%1%!0", write_pwd);
    LogInfo();
    LogInfo("Please write them down, they cannot be recovered and the backup will be lost if you lose them.");

    return 0;
}

static int RunPutFile(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *repo_directory = nullptr;
    const char *pwd = nullptr;
    const char *filename = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 put_file -R <dir> <filename>%!0

Options:
    %!..+-R, --repository <dir>%!0       Set repository directory
        %!..+--password <pwd>%!0         Set repository password)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                repo_directory = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                pwd = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        filename = opt.ConsumeNonOption();
    }

    if (!filename) {
        LogError("No filename provided");
        return 1;
    }
    if (!repo_directory) {
        LogError("Missing repository directory");
        return 1;
    }

    if (!pwd) {
        pwd = Prompt("Repository password: ", nullptr, "*", &temp_alloc);
        if (!pwd)
            return 1;
    }

    kt_Disk *disk = kt_OpenLocalDisk(repo_directory, pwd);
    if (!disk)
        return 1;
    RG_DEFER { delete disk; };

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), kt_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != kt_DiskMode::WriteOnly) {
        LogError("You should use the write-only key with this command");
    }

    LogInfo();
    LogInfo("Backing up...");

    kt_ID id = {};
    Size written = 0;
    if (!kt_BackupFile(disk, filename, &id, &written))
        return 1;

    LogInfo("File ID: %!..+%1%!0", id);
    LogInfo("Total written: %!..+%1%!0", FmtDiskSize(written));

    return 0;
}

static int RunGetFile(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *repo_directory = nullptr;
    const char *pwd = nullptr;
    const char *dest_filename = nullptr;
    const char *name = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 get_file -R <repo> -O <file> <ID>%!0

Options:
    %!..+-R, --repository <dir>%!0       Set repository directory
        %!..+--password <pwd>%!0         Set repository password

    %!..+-O, --output_file <file>%!0     Restore file to <file>)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                repo_directory = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                pwd = opt.current_value;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                dest_filename = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        name = opt.ConsumeNonOption();
    }

    if (!repo_directory) {
        LogError("Missing repository directory");
        return 1;
    }
    if (!name) {
        LogError("No name provided");
        return 1;
    }
    if (!dest_filename) {
        LogError("Missing destination filename");
        return 1;
    }

    if (!pwd) {
        pwd = Prompt("Repository password: ", nullptr, "*", &temp_alloc);
        if (!pwd)
            return 1;
    }

    kt_Disk *disk = kt_OpenLocalDisk(repo_directory, pwd);
    if (!disk)
        return 1;
    RG_DEFER { delete disk; };

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), kt_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != kt_DiskMode::ReadWrite) {
        LogError("Cannot decrypt with write-only key");
        return 1;
    }

    LogInfo();
    LogInfo("Extracting file...");

    Size file_len = 0;
    {
        kt_ID id = {};
        if (!kt_ParseID(name, &id))
            return 1;
        if (!kt_ExtractFile(disk, id, dest_filename, &file_len))
            return 1;
    }

    LogInfo("Restored file: %!..+%1%!0 (%2)", dest_filename, FmtDiskSize(file_len));

    return 0;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

Main commands:
    %!..+init%!0                         Init new backup repository

Specialized commands:
    %!..+put_file%!0                     Store encrypted file to storage
    %!..+get_file%!0                     Get and decrypt file from storage

Use %!..+%1 help <command>%!0 or %!..+%1 <command> --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(stderr);
        PrintLn(stderr);
        LogError("No command provided");
        return 1;
    }

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
    } else if (TestStr(cmd, "put_file")) {
        return RunPutFile(arguments);
    } else if (TestStr(cmd, "get_file")) {
        return RunGetFile(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
