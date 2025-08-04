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
#include "src/core/request/curl.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

#if !defined(_WIN32)
    #include <sys/stat.h>
#endif

namespace RG {

static const char *BaseConfig =
R"([Repository]
URL = %1
KeyFile = %2

[Settings]
# Threads = 
# CompressionLevel =

[Protection]
# RetainDuration =
)";

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

static const char *PromptNonEmpty(const char *object, const char *value, const char *mask, Allocator *alloc)
{
    char prompt[128];
    Fmt(prompt, "%1: ", object);

    const char *ret = Prompt(prompt, value, mask, alloc);

    if (!ret)
        return nullptr;
    if (!ret[0]) {
        LogError("Cannot use empty %1", FmtLowerAscii(object));
        return nullptr;
    }

    return ret;
}

static const char *PromptNonEmpty(const char *object, Allocator *alloc)
{
    const char *ret = PromptNonEmpty(object, nullptr, nullptr, alloc);
    return ret;
}

static bool CheckEndpoint(const char *url)
{
    CURLU *h = curl_url();
    RG_DEFER { curl_url_cleanup(h); };

    // Parse URL
    {
        CURLUcode ret = curl_url_set(h, CURLUPART_URL, url, 0);
        if (ret == CURLUE_OUT_OF_MEMORY)
            RG_BAD_ALLOC();
        if (ret != CURLUE_OK) {
            LogError("Malformed endpoint URL '%1'", url);
            return false;
        }
    }

    char *path = nullptr;
    if (curl_url_get(h, CURLUPART_PATH, &path, 0) == CURLUE_OUT_OF_MEMORY)
        RG_BAD_ALLOC();
    RG_DEFER { curl_free(path); };

    if (path && !TestStr(path, "/")) {
        LogError("Endpoint URL must not include path");
        return false;
    }

    return true;
}

int RunSetup(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = nullptr;
    bool force = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 setup [-C filename] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-f, --force%!0                    Overwrite existing files)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.Test("-f", "--force")) {
                force = true;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!config_filename) {
        HeapArray<const char *> choices;
        FindConfigFile(DefaultConfigDirectory, DefaultConfigName, &temp_alloc, &choices);

        choices.len--;
        for (Size i = 0; i < choices.len; i++) {
            choices[i] = choices.ptr[i + 1];
        }
        choices.Append("Custom path");

        Size idx = PromptEnum("Config file: ", choices);
        if (idx < 0)
            return 1;

        if (idx == choices.len - 1) {
            config_filename = PromptNonEmpty("Custom config filename", DefaultConfigName, nullptr, &temp_alloc);
            if (!config_filename)
                return 1;
        } else {
            config_filename = choices[idx];

            if (!EnsureDirectoryExists(config_filename))
                return 1;
        }
    }

    const char *key_path;
    const char *key_filename;
    {
        Span<const char> dirname;
        Span<const char> basename = SplitStrReverseAny(config_filename, RG_PATH_SEPARATORS, &dirname);

        Span<const char> prefix;
        SplitStrReverse(basename, '.', &prefix);

        if (!prefix.len) {
            prefix = basename;
        }

        key_path = Fmt(&temp_alloc, "%1.key", prefix).ptr;
        key_path = PromptNonEmpty("Master key filename", key_path, nullptr, &temp_alloc);
        if (!key_path)
            return 1;

        key_filename = NormalizePath(key_path, dirname, &temp_alloc).ptr;
    }

    if (!force) {
        if (TestFile(config_filename)) {
            Size idx = PromptEnum("Do you want to overwrite existing config file? ", {{'y', "Yes"}, {'n', "No"}}, 1);
            if (idx < 0 || idx)
                return 1;
        }
        if (TestFile(key_filename)) {
            Size idx = PromptEnum("Do you want to overwrite existing key file? ", {{'y', "Yes"}, {'n', "No"}}, 1);
            if (idx < 0 || idx)
                return 1;
        }
    }

    StreamWriter st(config_filename, (int)StreamWriterFlag::Atomic);
    if (!st.IsValid())
        return 1;

    // Prompt for repository type
    rk_DiskType type;
    {
        Size idx = PromptEnum("Repository type: ", rk_DiskTypeNames);
        if (idx < 0)
            return 1;
        type = (rk_DiskType)idx;
    }

    switch (type) {
        case rk_DiskType::Local: {
            const char *url = PromptNonEmpty("Repository path", &temp_alloc);
            if (!url)
                return 1;
            Print(&st, BaseConfig, url, key_path);
        } break;

        case rk_DiskType::S3: {
            const char *endpoint = nullptr;
            const char *bucket = nullptr;
            const char *key_id = nullptr;
            const char *secret_key = nullptr;

            endpoint = PromptNonEmpty("S3 endpoint URL", &temp_alloc);
            if (!endpoint || !CheckEndpoint(endpoint))
                return 1;
            bucket = PromptNonEmpty("Bucket name", &temp_alloc);
            if (!bucket)
                return 1;
            key_id = PromptNonEmpty("S3 access key ID", &temp_alloc);
            if (!key_id)
                return 1;
            secret_key = PromptNonEmpty("S3 secret key", nullptr, "*", &temp_alloc);
            if (!secret_key)
                return 1;

            const char *url = Fmt(&temp_alloc, "s3:%1/%2", TrimStrRight(endpoint, '/'), bucket).ptr;
            Print(&st, BaseConfig, url, key_path);

            PrintLn(&st);
            PrintLn(&st, "[S3]");
            if (key_id) {
                PrintLn(&st, "KeyID = %1", key_id);
            }
            if (secret_key) {
                PrintLn(&st, "SecretKey = %1", secret_key);
            }
        } break;

        case rk_DiskType::SFTP: {
            const char *host = nullptr;
            const char *username = nullptr;
            const char *password = nullptr;
            const char *keyfile = nullptr;
            const char *path = nullptr;
            const char *fingerprint = nullptr;

            bool use_password;
            bool use_keyfile;
            {
                Size ret = PromptEnum("SSH authentication mode: ", {
                    "Password",
                    "Keyfile"
                });
                if (ret < 0)
                    return 1;
                use_password = (ret == 0);
                use_keyfile = (ret == 1);
            }

            host = PromptNonEmpty("SSH host", &temp_alloc);
            if (!host)
                return 1;
            username = PromptNonEmpty("SSH user", &temp_alloc);
            if (!username)
                return 1;
            if (use_password) {
                password = PromptNonEmpty("SSH password", nullptr, "*", &temp_alloc);
                if (!password)
                    return 1;
            }
            if (use_keyfile) {
                keyfile = PromptNonEmpty("SSH keyfile", &temp_alloc);
                if (!keyfile)
                    return 1;
            }
            path = PromptNonEmpty("SSH path", &temp_alloc);
            if (!path)
                return 1;
            fingerprint = Prompt("Host fingerprint (optional): ", &temp_alloc);
            if (!fingerprint)
                return 1;

            const char *url = Fmt(&temp_alloc, "ssh://%1@%2/%3", username, host, path).ptr;
            Print(&st, BaseConfig, url, key_path);

            PrintLn(&st);
            PrintLn(&st, "[SFTP]");
            if (password) {
                PrintLn(&st, "Password = %1", password);
            }
            if (keyfile) {
                PrintLn(&st, "KeyFile = %1", keyfile);
            }
            if (fingerprint[0]) {
                PrintLn(&st, "Fingerprint = %1", fingerprint);
            }
        } break;
    }

    // Generate master key file
    {
        Span<uint8_t> mkey = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize), rk_MasterKeySize);
        RG_DEFER { ReleaseSafe(mkey.ptr, mkey.len); };

        randombytes_buf(mkey.ptr, mkey.len);

        if (!WriteFile(mkey, key_filename, (int)StreamWriterFlag::NoBuffer))
            return 1;
#if !defined(_WIN32)
        chmod(key_filename, 0600);
#endif
    }

    if (!st.Close())
        return 1;

    return 0;
}

int RunInit(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *key_filename = nullptr;
    bool generate_key = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 init [-C filename] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository filename%!0      Set repository URL

    %!..+-K, --key_file filename%!0        Set explicit master key export file
    %!..+-g, --generate_key%!0             Generate new master key)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-g", "--generate_key")) {
                generate_key = true;
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

    Span<uint8_t> mkey = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize), rk_MasterKeySize);
    RG_DEFER { ReleaseSafe(mkey.ptr, mkey.len); };

    if (generate_key) {
        if (TestFile(key_filename)) {
            LogError("Master key export file '%1' already exists", key_filename);
            return 1;
        }

        randombytes_buf(mkey.ptr, mkey.len);
    } else {
        // Use separate buffer to make sure file has correct size
        Span<uint8_t> buf = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize + 1), rk_MasterKeySize + 1);
        RG_DEFER { ReleaseSafe(buf.ptr, buf.len); };

        Size len = ReadFile(key_filename, buf);

        if (len < 0)
            return 1;
        if (len != mkey.len) {
            LogError("Unexpected master key size in '%1'", key_filename);
            return 1;
        }

        MemCpy(mkey.ptr, buf.ptr, mkey.len);
    }

    LogInfo("Initializing...");
    if (!repo->Init(mkey))
        return 1;

    if (generate_key) {
        if (!WriteFile(mkey, key_filename, (int)StreamWriterFlag::NoBuffer))
            return 1;
#if !defined(_WIN32)
        chmod(key_filename, 0600);
#endif

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
    int role = -1;
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
    if (role < 0) {
        LogError("Missing user role");
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

    if (!repo->InitUser(username, (rk_UserRole)role, pwd))
        return 1;

    LogInfo("Added user: %!..+%1%!0", username);
    LogInfo();
    LogInfo("Role: %!..+%1%!0", rk_UserRoleNames[role]);
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
