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

static const rk_UserInfo DefaultUsers[] = {
    { "admin", rk_UserRole::Admin, nullptr },
    { "data", rk_UserRole::ReadWrite, nullptr },
    { "write", rk_UserRole::WriteOnly, nullptr },
    { "log", rk_UserRole::LogOnly, nullptr }
};

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
    bool create_users = true;
    const char *key_filename = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 init [-C filename] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository filename%!0      Set repository URL

        %!..+--skip_users%!0               Omit default users

    %!..+-K, --key_file filename%!0        Set explicit master key export file)", FelixTarget);
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
            } else if (opt.Test("--skip_users")) {
                create_users = false;
            } else if (opt.Test("-K", "--key_file", OptionType::Value)) {
                key_filename = opt.current_value;
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

    LogInfo("Repository: %!..+%1%!0", disk->GetURL());
    LogInfo();

    if (!key_filename) {
        key_filename = Prompt("Master key export file: ", "master.key", nullptr, &temp_alloc);

        if (!key_filename)
            return 1;
        if (!key_filename[0]) {
            LogError("Cannot export to empty path");
            return 1;
        }
    }

    if (TestFile(key_filename)) {
        LogError("Master key export file '%1' already exists", key_filename);
        return 1;
    }

    LocalArray<rk_UserInfo, RG_LEN(DefaultUsers)> users;
    LocalArray<bool, RG_LEN(DefaultUsers)> show_pwds;

    // Generate repository passwords
    if (create_users) {
        for (rk_UserInfo user: DefaultUsers) {
            bool show_pwd = false;

            char prompt[256];
            Fmt(prompt, "User '%1' password (leave empty to autogenerate): ", user.username);

            user.pwd = Prompt(prompt, nullptr, "*", &temp_alloc);
            if (!user.pwd)
                return 1;

            if (!user.pwd[0]) {
                Span<char> buf = AllocateSpan<char>(&temp_alloc, 33);
                if (!GeneratePassword(buf))
                   return 1;

                user.pwd = buf.ptr;
                show_pwd = true;
            }

            users.Append(user);
            show_pwds.Append(show_pwd);
        }
    }

    Span<uint8_t> mkey = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize), rk_MasterKeySize);
    RG_DEFER { ReleaseSafe(mkey.ptr, mkey.len); };

    randombytes_buf(mkey.ptr, mkey.len);

    LogInfo("Initializing...");
    if (!disk->Init(mkey, users))
        return 1;
    LogInfo();

    if (users.len) {
        int align = 0;

        for (const rk_UserInfo &user: users) {
            align = std::max(align, (int)strlen(user.username));
        }

        for (Size i = 0; i < users.len; i++) {
            const rk_UserInfo &user = users[i];

            if (show_pwds[i]) {
                LogInfo("%1 %2 user password: %!..+%3%!0", i ? "       " : "Default", FmtArg(user.username).Pad(-align), user.pwd);
            } else {
                LogInfo("%1 %2 user password: %!D..(hidden)%!0", i ? "       " : "Default", FmtArg(user.username).Pad(-align));
            }
        }

        LogInfo();
    }

    // Continue even if it fails, an error will be shown regardless
    if (WriteFile(mkey, key_filename, (int)StreamWriterFlag::NoBuffer)) {
        LogInfo("Wrote master key: %!..+%1%!0", key_filename);
        LogInfo();
        LogInfo("Please %!.._save the master key in a secure place%!0, you can use it to decrypt the data even if the default accounts are lost or deleted.");
    }

    return 0;
}

int RunAddUser(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    const char *key_filename = nullptr;
    rk_UserRole role = rk_UserRole::WriteOnly;
    const char *pwd = nullptr;
    bool force = false;
    const char *username = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 add_user [-C filename] [option...] username%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL
    %!..+-u, --user username%!0            Set repository username

    %!..+-K, --key_file filename%!0        Use master key instead of username/password

    %!..+-r, --role role%!0                User role (see below)
                                   %!D..(default: %2)%!0
        %!..+--password password%!0        Set password explicitly

    %!..+-f, --force%!0                    Overwrite existing user %!D..(if any)%!0

Available user roles: %!..+%3%!0)", FelixTarget, rk_UserRoleNames[(int)role], FmtSpan(rk_UserRoleNames));
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
            } else if (opt.Test("-K", "--key_file", OptionType::Value)) {
                key_filename = opt.current_value;
            } else if (opt.Test("-r", "--role", OptionType::Value)) {
                if (!OptionToEnumI(rk_UserRoleNames, opt.current_value, &role)) {
                    LogError("Unknown user role '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("--password", OptionType::Value)) {
                pwd = opt.current_value;
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

    bool authenticate = !key_filename;

    if (!config.Complete(authenticate))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, authenticate);
    if (!disk)
        return 1;

    // Use master key instead of username/password
    if (!authenticate) {
        Span<uint8_t> mkey = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize), rk_MasterKeySize);
        RG_DEFER_C(len = mkey.len) { ReleaseSafe(mkey.ptr, len); };

        mkey.len = ReadFile(key_filename, mkey);
        if (mkey.len < 0)
            return 1;

        if (!disk->Authenticate(mkey))
            return 1;
    }

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), disk->GetRole());
    if (!disk->HasMode(rk_AccessMode::Config)) {
        LogError("Cannot create user with %1 role", disk->GetRole());
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

    if (!disk->InitUser(username, role, pwd, force))
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
    rk_Config config;
    const char *username = nullptr;
    bool force = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 delete_user [-C filename] [option...] username%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL

    %!..+-f, --force%!0                    Force deletion %!D..(to delete yourself)%!0)", FelixTarget);
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

    if (!config.Complete(!force))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, !force);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), disk->GetRole());
    LogInfo();

    if (!force) {
        if (!disk->HasMode(rk_AccessMode::Config)) {
            LogError("Refusing to delete without config access");
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
    const char *key_filename = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 list_users [-C filename] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL

    %!..+-K, --key_file filename%!0        Check user signatures with master key file

    %!..+-f, --format format%!0            Change output format
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
            } else if (opt.Test("-K", "--key_file", OptionType::Value)) {
                key_filename = opt.current_value;
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

    bool verify = key_filename;

    if (!config.Complete(false))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, false);
    if (!disk)
        return 1;

    if (verify) {
        Span<uint8_t> mkey = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize), rk_MasterKeySize);
        RG_DEFER_C(len = mkey.len) { ReleaseSafe(mkey.ptr, len); };

        mkey.len = ReadFile(key_filename, mkey);
        if (mkey.len < 0)
            return 1;

        if (!disk->Authenticate(mkey))
            return 1;
    }

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), disk->GetRole());
    LogInfo();

    HeapArray<rk_UserInfo> users;
    if (!disk->ListUsers(&temp_alloc, verify, &users))
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
