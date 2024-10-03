// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "rekkord.hh"
#include "src/core/password/password.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#include "vendor/pugixml/src/pugixml.hpp"
#include <iostream> // for pugixml

namespace RG {

static bool GeneratePassword(Span<char> out_pwd)
{
    RG_ASSERT(out_pwd.len >= 33);

    // Avoid characters that are annoying in consoles
    unsigned int flags = (int)pwd_GenerateFlag::LowersNoAmbi |
                         (int)pwd_GenerateFlag::UppersNoAmbi |
                         (int)pwd_GenerateFlag::DigitsNoAmbi |
                         (int)pwd_GenerateFlag::Specials;

    return pwd_GeneratePassword(flags, out_pwd);
}

int RunInit(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    const char *full_pwd = nullptr;
    const char *write_pwd = nullptr;
    bool random_full_pwd = true;
    bool random_write_pwd = true;
    const char *key_filename = "master.key";

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 init [-C <config>] [dir]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory

        %!..+--master_password [pwd]%!0  Set master password manually
        %!..+--write_password [pwd]%!0   Set write-only password manually

    %!..+-K, --key_file <file>%!0        Set explicit master key export file
                                 %!D..(default: %2)%!0
        %!..+--skip_key%!0               Skip master key export)", FelixTarget, key_filename);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("--master_password", OptionType::OptionalValue)) {
                full_pwd = opt.current_value;
                random_full_pwd = false;
            } else if (opt.Test("--write_password", OptionType::OptionalValue)) {
                write_pwd = opt.current_value;
                random_write_pwd = false;
            } else if (opt.Test("-K", "--key_file", OptionType::Value)) {
                key_filename = opt.current_value;
            } else if (opt.Test("--skip_key")) {
                key_filename = nullptr;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!config.Complete(false))
        return 1;

    if (key_filename && TestFile(key_filename)) {
        LogError("Master key export file '%1' already exists", key_filename);
        return 1;
    }

    std::unique_ptr<rk_Disk> disk = rk_Open(config, false);
    if (!disk)
        return 1;
    RG_ASSERT(disk->GetMode() == rk_DiskMode::Secure);

    LogInfo("Repository: %!..+%1%!0", disk->GetURL());
    LogInfo();

    // Generate repository passwords
    if (random_full_pwd) {
        Span<char> buf = AllocateSpan<char>(&temp_alloc, 33);
        if (!GeneratePassword(buf))
           return 1;
       full_pwd = buf.ptr;
    } else if (!full_pwd) {
        full_pwd = Prompt("Master password: ", nullptr, "*", &temp_alloc);
        if (!full_pwd)
            return 1;
    }
    if (random_write_pwd) {
        Span<char> buf = AllocateSpan<char>(&temp_alloc, 33);
        if (!GeneratePassword(buf))
           return 1;
        write_pwd = buf.ptr;
    } else if (!write_pwd) {
        write_pwd = Prompt("Write-only password: ", nullptr, "*", &temp_alloc);
        if (!write_pwd)
            return 1;
    }

    LogInfo("Initializing...");
    if (!disk->Init(full_pwd, write_pwd))
        return 1;
    LogInfo();

    if (random_full_pwd) {
        LogInfo("Default master password: %!..+%1%!0", full_pwd);
    } else {
        LogInfo("Default master password: %!D..(hidden)%!0");
    }
    if (random_write_pwd) {
        LogInfo("    write-only password: %!..+%1%!0", write_pwd);
    } else {
        LogInfo("    write-only password: %!D..(hidden)%!0");
    }

    // Continue even if it fails, an error will be shown regardless
    if (key_filename) {
        LogInfo();

        if (WriteFile(disk->GetFullKey(), key_filename)) {
            LogInfo("Wrote master key: %!..+%1%!0", key_filename);
            LogInfo();
            LogInfo("Please %!.._save the master key in a secure place%!0, you can use it to decrypt the data even if the default account is lost or deleted.");
        } else {
            LogInfo("Use %!..+rekkord export_key%!0 to export the master key and keep it safe.");
        }
    }

    return 0;
}

int RunExportKey(Span<const char *> arguments)
{
    // Options
    rk_Config config;
    const char *key_filename = "master.key";

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 export_key [-C <config>]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory
    %!..+-u, --user <user>%!0            Set repository username
        %!..+--password <pwd>%!0         Set repository password

    %!..+-K, --key_file <file>%!0        Set explicit master key export file
                                 %!D..(default: %2)%!0)", FelixTarget, key_filename);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                config.username = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                config.password = opt.current_value;
            } else if (opt.Test("-K", "--key_file", OptionType::Value)) {
                key_filename = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!config.Complete(true))
        return 1;

    if (TestFile(key_filename)) {
        LogError("Master key export file '%1' already exists", key_filename);
        return 1;
    }

    std::unique_ptr<rk_Disk> disk = rk_Open(config, true);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != rk_DiskMode::Full) {
        LogError("You must use the read-write password with this command");
        return 1;
    }
    LogInfo();

    if (!WriteFile(disk->GetFullKey(), key_filename))
        return 1;

    LogInfo("Wrote master key: %!..+%1%!0", key_filename);
    LogInfo();
    LogInfo("Please %!.._save the master key in a secure place%!0, you can use it to decrypt the data even if the default account is lost or deleted.");

    return 0;
}

int RunAddUser(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    const char *key_filename = nullptr;
    rk_DiskMode mode = rk_DiskMode::Full;
    const char *full_pwd = nullptr;
    const char *write_pwd = nullptr;
    bool random_full_pwd = true;
    bool random_write_pwd = true;
    bool force = false;
    const char *username = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 add_user [-C <config>] <username>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory
    %!..+-u, --user <user>%!0            Set repository username
        %!..+--password <pwd>%!0         Set repository password

    %!..+-K, --key_file <file>%!0        Use master key instead of username/password

    %!..+-m, --mode <mode>%!0            Access mode (see below)

        %!..+--master_password [pwd]%!0  Set master password manually
        %!..+--write_password [pwd]%!0   Set write-only password manually

        %!..+--force%!0                  Overwrite exisiting user %!D..(if any)%!0

Available access modes: %!..+%2, %3%!0)", FelixTarget, rk_DiskModeNames[(int)rk_DiskMode::Full],
                                                       rk_DiskModeNames[(int)rk_DiskMode::WriteOnly]);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                config.username = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                config.password = opt.current_value;
            } else if (opt.Test("-K", "--key_file", OptionType::Value)) {
                key_filename = opt.current_value;
            } else if (opt.Test("-m", "--mode", OptionType::Value)) {
                if (TestStr(opt.current_value, rk_DiskModeNames[(int)rk_DiskMode::Full])) {
                    mode = rk_DiskMode::Full;
                } else if (TestStr(opt.current_value, rk_DiskModeNames[(int)rk_DiskMode::WriteOnly])) {
                    mode = rk_DiskMode::WriteOnly;
                } else {
                    LogError("Unknown mode '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("--master_password", OptionType::OptionalValue)) {
                full_pwd = opt.current_value;
                random_full_pwd = false;
            } else if (opt.Test("--write_password", OptionType::OptionalValue)) {
                write_pwd = opt.current_value;
                random_write_pwd = false;
            } else if (opt.Test("--force")) {
                force = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        username = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!username) {
        LogError("Missing username");
        return 1;
    }

    bool authenticate = !key_filename;

    if (!config.Complete(authenticate))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, authenticate);
    if (!disk)
        return 1;

    // Use master key instead of username/password
    if (!authenticate) {
        RG_ASSERT(disk->GetMode() == rk_DiskMode::Secure);

        LocalArray<uint8_t, 128> master_key;
        master_key.len = ReadFile(key_filename, master_key.data);
        if (master_key.len < 0)
            return 1;

        if (!disk->Authenticate(master_key))
            return false;
    }

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (mode == rk_DiskMode::Full && disk->GetMode() != rk_DiskMode::Full) {
        LogError("You must use the read-write password with this command");
        return 1;
    }
    LogInfo();

    // Generate repository passwords
    if (mode == rk_DiskMode::Full) {
        if (random_full_pwd) {
            Span<char> buf = AllocateSpan<char>(&temp_alloc, 33);
            if (!GeneratePassword(buf))
               return 1;
           full_pwd = buf.ptr;
        } else if (!full_pwd) {
            full_pwd = Prompt("Master password: ", nullptr, "*", &temp_alloc);
            if (!full_pwd)
                return 1;
        }
    } else if (!random_full_pwd) {
        LogError("Don't set master password for write-only user");
        return 1;
    }
    if (random_write_pwd) {
        Span<char> buf = AllocateSpan<char>(&temp_alloc, 33);
        if (!GeneratePassword(buf))
           return 1;
        write_pwd = buf.ptr;
    } else if (!write_pwd) {
        write_pwd = Prompt("Write-only password: ", nullptr, "*", &temp_alloc);
        if (!write_pwd)
            return 1;
    }

    if (!disk->InitUser(username, full_pwd, write_pwd, force))
        return 1;

    LogInfo("Added user: %!..+%1%!0", username);
    LogInfo();
    if (mode != rk_DiskMode::Full) {
        LogInfo("New user master password: %!D..(none)%!0");
    } else if (random_full_pwd) {
        LogInfo("New user master password: %!..+%1%!0", full_pwd);
    } else {
        LogInfo("New user master password: %!D..(hidden)%!0");
    }
    if (random_write_pwd) {
        LogInfo("     write-only password: %!..+%1%!0", write_pwd);
    } else {
        LogInfo("     write-only password: %!D..(hidden)%!0");
    }

    return 0;
}

int RunDeleteUser(Span<const char *> arguments)
{
    // Options
    rk_Config config;
    const char *username = nullptr;
    bool force = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 delete_user [-C <config>] <username>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory

        %!..+--force%!0                  Force deletion %!D..(to delete yourself)%!0)", FelixTarget);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("--force")) {
                force = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        username = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!username) {
        LogError("Missing username");
        return 1;
    }

    if (!config.Complete(!force))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, !force);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    LogInfo();

    if (!force) {
        if (disk->GetMode() != rk_DiskMode::Full) {
            LogError("Refusing to delete without full authentification (unless --force is used)");
            return 1;
        }
        if (TestStr(username, config.username)) {
            LogError("Cannot delete yourself (unless --force is used)");
            return 1;
        }
    }

    if (!disk->DeleteUser(username))
        return 1;

    LogInfo("Deleted user: %!..+%1%!0", username);

    return 0;
}

int RunListUsers(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    OutputFormat format = OutputFormat::Plain;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 list_users [-C <config>]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory

    %!..+-f, --format <format>%!0        Change output format
                                 %!D..(default: %2)%!0

Available output formats: %!..+%3%!0)", FelixTarget, OutputFormatNames[(int)format], FmtSpan(OutputFormatNames));
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("-f", "--format", OptionType::Value)) {
                if (!OptionToEnumI(OutputFormatNames, opt.current_value, &format)) {
                    LogError("Unknown output format '%1'", opt.current_value);
                    return 1;
                }
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!config.Complete(false))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, false);
    if (!disk)
        return 1;
    RG_ASSERT(disk->GetMode() == rk_DiskMode::Secure);

    LogInfo("Repository: %!..+%1%!0", disk->GetURL());
    LogInfo();

    HeapArray<rk_UserInfo> users;
    if (!disk->ListUsers(&temp_alloc, &users))
        return 1;

    switch (format) {
        case OutputFormat::Plain: {
            if (users.len) {
                for (const rk_UserInfo &user: users) {
                    PrintLn("%!..+%1%!0 [%2]", FmtArg(user.username).Pad(24), rk_DiskModeNames[(int)user.mode]);
                }
            } else {
                LogInfo("There does not seem to be any user");
            }
        } break;

        case OutputFormat::JSON: {
            json_PrettyWriter json(StdOut);

            json.StartArray();
            for (const rk_UserInfo &user: users) {
                json.StartObject();

                json.Key("name"); json.String(user.username);
                json.Key("mode"); json.String(rk_DiskModeNames[(int)user.mode]);

                json.EndObject();
            }
            json.EndArray();

            json.Flush();
            PrintLn();
        } break;

        case OutputFormat::XML: {
            pugi::xml_document doc;
            pugi::xml_node root = doc.append_child("Users");

            for (const rk_UserInfo &user: users) {
                pugi::xml_node element = root.append_child("User");

                element.append_attribute("Name") = user.username;
                element.append_attribute("Mode") = rk_DiskModeNames[(int)user.mode];
            }

            doc.save(std::cout, "    ");
        } break;
    }

    return 0;
}

}
