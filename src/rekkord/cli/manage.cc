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
#include "src/core/wrap/xml.hh"
#include "rekkord.hh"
#include "src/core/password/password.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

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
    const char *key_filename = nullptr;
    bool generate_key = true;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 init [-C filename] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository filename%!0      Set repository URL

    %!..+-K, --key_file filename%!0        Set explicit master key export file
        %!..+--reuse_key%!0                Load master key from existing file)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("--reuse_key")) {
                generate_key = false;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    // Reuse common -K option even if we use it slightly differently
    key_filename = rekkord_config.key_filename;

    if (!rekkord_config.Complete(false))
        return 1;
    if (!key_filename && !generate_key) {
        LogError("Missing master key filename");
        return 1;
    }

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, false);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0", disk->GetURL());
    LogInfo();

    if (repo->IsRepository()) {
        LogError("Repository is already initialized");
        return 1;
    }

    if (!key_filename) {
        key_filename = Prompt("Master key export file: ", "master.key", nullptr, &temp_alloc);

        if (!key_filename)
            return 1;
        if (!key_filename[0]) {
            LogError("Cannot export to empty path");
            return 1;
        }
    }

    Span<uint8_t> mkey = {};
    RG_DEFER { ReleaseSafe(mkey.ptr, mkey.len); };

    if (generate_key) {
        if (TestFile(key_filename)) {
            LogError("Master key export file '%1' already exists", key_filename);
            return 1;
        }

        mkey = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize), rk_MasterKeySize);
        randombytes_buf(mkey.ptr, mkey.len);
    } else {
        mkey = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize), rk_MasterKeySize + 1);
        mkey.len = ReadFile(key_filename, mkey);

        if (mkey.len < 0)
            return 1;
        if (mkey.len != rk_MasterKeySize) {
            LogError("Unexpected master key size in '%1'", key_filename);
            return 1;
        }
    }

    LogInfo("Initializing...");
    if (!repo->Init(mkey))
        return 1;

    if (generate_key) {
        if (!WriteFile(mkey, key_filename, (int)StreamWriterFlag::NoBuffer))
            return 1;

        LogInfo();
        LogInfo("Wrote master key: %!..+%1%!0", key_filename);
        LogInfo();
        LogInfo("Please %!.._save the master key in a secure place%!0, you can use it to decrypt the data even if the default accounts are lost or deleted.");
    } else {
        LogInfo("Done");
    }

    return 0;
}

int RunAddUser(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_UserRole role = rk_UserRole::WriteOnly;
    const char *pwd = nullptr;
    const char *username = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 add_user [-C filename] [option...] username%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
User options:

    %!..+-r, --role role%!0                User role (see below)
                                   %!D..(default: %1)%!0
        %!..+--password password%!0        Set password explicitly

Available user roles: %!..+%2%!0)", rk_UserRoleNames[(int)role], FmtSpan(rk_UserRoleNames));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-r", "--role", OptionType::Value)) {
                if (!OptionToEnumI(rk_UserRoleNames, opt.current_value, &role)) {
                    LogError("Unknown user role '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("--password", OptionType::Value)) {
                pwd = opt.current_value;
            } else if (!HandleCommonOption(opt)) {
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

    if (!rekkord_config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Config)) {
        LogError("Cannot create user with %1 role", repo->GetRole());
        return 1;
    }
    LogInfo();

    bool random_pwd = false;

    if (!pwd) {
        pwd = Prompt("User password (leave empty to autogenerate): ", nullptr, "*", &temp_alloc);
        if (!pwd)
            return 1;

        if (!pwd[0]) {
            Span<char> buf = AllocateSpan<char>(&temp_alloc, 33);
            if (!GeneratePassword(buf))
               return 1;

            pwd = buf.ptr;
            random_pwd = true;
        }
    }

    if (!repo->InitUser(username, role, pwd))
        return 1;

    LogInfo("Added user: %!..+%1%!0", username);
    LogInfo();
    LogInfo("Role: %!..+%1%!0", rk_UserRoleNames[(int)role]);
    if (random_pwd) {
        LogInfo("Password: %!..+%1%!0", pwd);
    } else {
        LogInfo("Password: %!D..(hidden)%!0");
    }

    return 0;
}

int RunDeleteUser(Span<const char *> arguments)
{
    // Options
    const char *username = nullptr;
    bool force = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 delete_user [-C filename] [option...] username%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
User options:

    %!..+-f, --force%!0                    Force deletion %!D..(to delete yourself)%!0

Please note that deleting users only requires the ability to delete objects from the underlying repository storage, even though this command asks for repository authentication as a precaution.)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-f", "--force")) {
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

    if (!rekkord_config.Complete(!force))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, !force);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    LogInfo();

    if (!force) {
        if (!repo->HasMode(rk_AccessMode::Config)) {
            LogError("Refusing to delete without config access");
            return 1;
        }
        if (rekkord_config.username && TestStr(username, rekkord_config.username)) {
            LogError("Cannot delete yourself (unless --force is used)");
            return 1;
        }
    }

    if (!repo->DeleteUser(username))
        return 1;

    LogInfo("Deleted user: %!..+%1%!0", username);

    return 0;
}

int RunListUsers(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    OutputFormat format = OutputFormat::Plain;
    bool verify = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 list_users [-C filename] [option...]%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
List options:

        %!..+--verify%!0                   Check user signatures with master key file

    %!..+-f, --format format%!0            Change output format
                                   %!D..(default: %1)%!0

Available output formats: %!..+%2%!0)", OutputFormatNames[(int)format], FmtSpan(OutputFormatNames));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("--verify")) {
                verify = true;
            } else if (opt.Test("-f", "--format", OptionType::Value)) {
                if (!OptionToEnumI(OutputFormatNames, opt.current_value, &format)) {
                    LogError("Unknown output format '%1'", opt.current_value);
                    return 1;
                }
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (verify && !rekkord_config.key_filename) {
        LogError("Specify master key file with --key_file to verify users");
        return 1;
    }

    if (!rekkord_config.Complete(verify))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, verify);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    LogInfo();

    HeapArray<rk_UserInfo> users;
    if (!repo->ListUsers(&temp_alloc, verify, &users))
        return 1;

    switch (format) {
        case OutputFormat::Plain: {
            if (users.len) {
                for (const rk_UserInfo &user: users) {
                    PrintLn("%!..+%1%!0 [%2]", FmtArg(user.username).Pad(24), rk_UserRoleNames[(int)user.role]);
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
                json.Key("role"); json.String(rk_UserRoleNames[(int)user.role]);

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
                element.append_attribute("Role") = rk_UserRoleNames[(int)user.role];
            }

            xml_PugiWriter writer(StdOut);
            doc.save(writer, "    ");
        } break;
    }

    return 0;
}

}
