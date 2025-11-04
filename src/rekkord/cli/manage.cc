// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
#include "rekkord.hh"
#include "src/core/native/password/password.hh"
#include "src/core/native/request/curl.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

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

static const char *PromptNonEmpty(const char *prompt, const char *value, const char *mask, Allocator *alloc)
{
retry:
    const char *ret = Prompt(prompt, value, mask, alloc);
    if (!ret)
        return nullptr;

    if (!ret[0]) {
        Span<const char> object = TrimStrRight(prompt, ".:;,-_ ");
        LogError("Cannot use empty %1", FmtLowerAscii(object));
        goto retry;
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
    K_DEFER { curl_url_cleanup(h); };

    // Parse URL
    {
        CURLUcode ret = curl_url_set(h, CURLUPART_URL, url, 0);
        if (ret == CURLUE_OUT_OF_MEMORY)
            K_BAD_ALLOC();
        if (ret != CURLUE_OK) {
            LogError("Malformed endpoint URL '%1'", url);
            return false;
        }
    }

    char *path = nullptr;
    if (curl_url_get(h, CURLUPART_PATH, &path, 0) == CURLUE_OUT_OF_MEMORY)
        K_BAD_ALLOC();
    K_DEFER { curl_free(path); };

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
    bool custom_config = false;
    bool force = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 setup [-C filename] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-f, --force%!0                    Overwrite existing files)"), FelixTarget);
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
                custom_config = true;
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
        choices.Append(T("Custom path"));

        const char *custom = GetEnv(DefaultConfigEnv);

        Size idx = PromptEnum(T("Config file:"), choices, custom ? choices.len - 1 : 0);
        if (idx < 0)
            return 1;

        if (idx == choices.len - 1) {
            const char *value = custom ? custom : DefaultConfigName;

            config_filename = PromptNonEmpty(T("Custom config filename:"), value, nullptr, &temp_alloc);
            if (!config_filename)
                return 1;

            custom_config = true;
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
        Span<const char> basename = SplitStrReverseAny(config_filename, K_PATH_SEPARATORS, &dirname);

        Span<const char> prefix;
        SplitStrReverse(basename, '.', &prefix);

        if (!prefix.len) {
            prefix = basename;
        }

        key_path = Fmt(&temp_alloc, "%1.key", prefix).ptr;
        key_path = PromptNonEmpty(T("Master key filename:"), key_path, nullptr, &temp_alloc);
        if (!key_path)
            return 1;

        key_filename = NormalizePath(key_path, dirname, &temp_alloc).ptr;
    }

    if (!force) {
        if (TestFile(config_filename) && PromptYN(T("Do you want to overwrite existing config file?")) != 1)
            return 1;
        if (TestFile(key_filename) && PromptYN(T("Do you want to overwrite existing key file?")) != 1)
            return 1;
    }

    StreamWriter st(config_filename, (int)StreamWriterFlag::Atomic);
    if (!st.IsValid())
        return 1;

    // Prompt for repository type
    rk_DiskType type;
    {
        Size idx = PromptEnum(T("Repository type:"), rk_DiskTypeNames);
        if (idx < 0)
            return 1;
        type = (rk_DiskType)idx;
    }

    switch (type) {
        case rk_DiskType::Local: {
            const char *url = PromptNonEmpty(T("Repository path:"), &temp_alloc);
            if (!url)
                return 1;
            Print(&st, BaseConfig, url, key_path);
        } break;

        case rk_DiskType::S3: {
            const char *endpoint = nullptr;
            const char *bucket = nullptr;
            const char *key_id = nullptr;
            const char *secret_key = nullptr;

            do {
                endpoint = PromptNonEmpty(T("S3 endpoint URL:"), &temp_alloc);
                if (!endpoint)
                    return 1;
            } while (!CheckEndpoint(endpoint));
            bucket = PromptNonEmpty(T("Bucket name:"), &temp_alloc);
            if (!bucket)
                return 1;
            key_id = PromptNonEmpty(T("S3 access key ID:"), &temp_alloc);
            if (!key_id)
                return 1;
            secret_key = PromptNonEmpty(T("S3 secret key:"), nullptr, "*", &temp_alloc);
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
                Size ret = PromptEnum(T("SSH authentication mode:"), {
                    T("Password"),
                    T("Keyfile")
                });
                if (ret < 0)
                    return 1;
                use_password = (ret == 0);
                use_keyfile = (ret == 1);
            }

            host = PromptNonEmpty(T("SSH host:"), &temp_alloc);
            if (!host)
                return 1;
            username = PromptNonEmpty(T("SSH user:"), &temp_alloc);
            if (!username)
                return 1;
            if (use_password) {
                password = PromptNonEmpty(T("SSH password:"), nullptr, "*", &temp_alloc);
                if (!password)
                    return 1;
            }
            if (use_keyfile) {
                keyfile = PromptNonEmpty(T("SSH keyfile:"), &temp_alloc);
                if (!keyfile)
                    return 1;
            }
            path = PromptNonEmpty(T("SSH path:"), &temp_alloc);
            if (!path)
                return 1;
            fingerprint = Prompt(T("Host fingerprint (optional):"), &temp_alloc);
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
        K_DEFER { ReleaseSafe(mkey.ptr, mkey.len); };

        FillRandomSafe(mkey.ptr, mkey.len);

        if (!rk_SaveRawKey(mkey, key_filename))
            return 1;
    }

    if (!st.Close())
        return 1;

    LogInfo();
    LogInfo("Created config file '%1'", config_filename);

    if (custom_config) {
        const char *fullpath = NormalizePath(config_filename, GetWorkingDirectory(), &temp_alloc).ptr;

        LogInfo();
        LogInfo("You have used a custom config path.");
        LogInfo("Use %!..+export REKKORD_CONFIG_FILE=\"%1\"%!0 before you execute other commands.", FmtEscape(fullpath));
    }

    LogInfo();
    LogInfo("Run %!..+rekkord init%!0 to initialize the repository.");

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
T(R"(Usage: %!..+%1 init [-C filename] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL
    %!..+-K, --key_file filename%!0        Set master key file

    %!..+-g, --generate_key%!0             Generate new master key)"), FelixTarget);
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

    if (!rk_config.Complete(0))
        return 1;
    if (!rk_config.Validate(0))
        return 1;

    // Reuse common -K option even if we use it slightly differently
    key_filename = rk_config.key_filename;

    if (!key_filename && !generate_key) {
        LogError("Missing master key filename");
        return 1;
    }

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rk_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rk_config, false);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0", disk->GetURL());
    LogInfo();

    if (repo->IsRepository()) {
        LogError("Repository is already initialized");
        return 1;
    }

    if (!key_filename) {
        for (;;) {
            key_filename = Prompt(T("Master key export file:"), "master.key", nullptr, &temp_alloc);

            if (!key_filename)
                return 1;
            if (key_filename[0])
                break;

            LogError("Cannot export to empty path");
        }
    }

    Span<uint8_t> mkey = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize), rk_MasterKeySize);
    K_DEFER { ReleaseSafe(mkey.ptr, mkey.len); };

    if (generate_key) {
        if (TestFile(key_filename)) {
            LogError("Master key export file '%1' already exists", key_filename);
            return 1;
        }

        FillRandomSafe(mkey.ptr, mkey.len);
    } else {
        // Use separate buffer to make sure file has correct size
        Span<uint8_t> buf = MakeSpan((uint8_t *)AllocateSafe(rk_MaximumKeySize), rk_MaximumKeySize);
        K_DEFER { ReleaseSafe(buf.ptr, buf.len); };

        Size len = rk_ReadRawKey(key_filename, buf);
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
        if (!rk_SaveRawKey(mkey, key_filename))
            return 1;

        LogInfo();
        LogInfo("Wrote master key: %!..+%1%!0", key_filename);
        LogInfo();
        LogInfo("Please %!.._save the master key in a secure place%!0, you can use it to decrypt the data even derived keys are lost or deleted.");
    } else {
        LogInfo("Done");
    }

    return 0;
}

int RunDerive(Span<const char *> arguments)
{
    // Options
    const char *output_filename = nullptr;
    bool force = false;
    rk_KeyType type = rk_KeyType::Master;

    const auto print_usage = [=](StreamWriter *st) {
        Span<const char *const> types = MakeSpan(rk_KeyTypeNames + 1, K_LEN(rk_KeyTypeNames) - 1);

        PrintLn(st,
T(R"(Usage: %!..+%1 derive [-C filename] [option...] -O destination%!0)"), FelixTarget);
        PrintCommonOptions(st);
        PrintLn(st, T(R"(
Key options:

    %!..+-O, --output_file file%!0         Write keys to destination file

    %!..+-f, --force%!0                    Overwrite existing file

    %!..+-t, --type type%!0                Set key type and permissions (see below)

Available key types: %!..+%1%!0)"), FmtList(types));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-O", "--output_filename", OptionType::Value)) {
                output_filename = opt.current_value;
            } else if (opt.Test("-f", "--force")) {
                force = true;
            } else if (opt.Test("-t", "--type", OptionType::Value)) {
                if (!OptionToEnumI(rk_KeyTypeNames, opt.current_value, &type) || type == rk_KeyType::Master) {
                    LogError("Unknown key type '%1'", opt.current_value);
                    return 1;
                }
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!output_filename) {
        LogError("Missing output filename");
        return 1;
    }
    if (type == rk_KeyType::Master) {
        LogError("Missing key type");
        return 1;
    }

    if (TestFile(output_filename) && !force) {
        LogError("File '%1' already exists", output_filename);
        return 1;
    }

    if (!rk_config.Complete())
        return 1;
    if (!rk_config.Validate())
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rk_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rk_config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Config)) {
        LogError("Cannot derive keys with %1 key", repo->GetRole());
        return 1;
    }
    LogInfo();

    rk_KeySet *keys = (rk_KeySet *)AllocateSafe(K_SIZE(rk_KeySet));
    K_DEFER { ReleaseSafe(keys, K_SIZE(*keys)); };

    if (!rk_ExportKeyFile(repo->GetKeys(), type, output_filename, keys))
        return 1;

    LogInfo("Key file: %!..+%1%!0", output_filename);
    LogInfo("Key ID: %!..+%1%!0", FmtHex(keys->kid));
    LogInfo("Key type: %!..+%1%!0", rk_KeyTypeNames[(int)type]);

    return 0;
}

int RunIdentify(Span<const char *> arguments)
{
    // Options
    bool online = true;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 identify [-C filename] [option...]%!0)"), FelixTarget);
        PrintCommonOptions(st);
        PrintLn(st, T(R"(
Identify options:

        %!..+--offline%!0                  Analyse key file without opening repository)"));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("--offline")) {
                online = false;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!rk_config.Complete())
        return 1;

    rk_KeySet *keyset = (rk_KeySet *)AllocateSafe(K_SIZE(rk_KeySet));
    K_DEFER { ReleaseSafe(keyset, K_SIZE(*keyset)); };

    if (online) {
        if (!rk_config.Validate())
            return 1;

        std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rk_config);
        std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rk_config, true);
        if (!repo)
            return 1;

        LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
        LogInfo();

        *keyset = repo->GetKeys();
    } else {
        if (!rk_config.key_filename) {
            LogError("Missing repository key file");
            return 1;
        }

        if (!rk_LoadKeyFile(rk_config.key_filename, keyset))
            return 1;
    }

    LogInfo("Key ID: %!..+%1%!0", FmtHex(keyset->kid));
    LogInfo("Key type: %!..+%1%!0", rk_KeyTypeNames[(int)keyset->type]);

    return 0;
}

}
