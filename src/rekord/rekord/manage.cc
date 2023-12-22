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
#include "rekord.hh"
#include "src/core/libpasswd/libpasswd.hh"
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
    rk_Config config;
    const char *full_pwd = nullptr;
    const char *write_pwd = nullptr;
    bool random_full_pwd = true;
    bool random_write_pwd = true;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 init [-C <config>] [dir]

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory

        %!..+--master_password [pwd]%!0  Set master password manually
        %!..+--write_password [pwd]%!0   Set write-only password manually)", FelixTarget);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
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
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

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

    if (!config.Complete(false))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, false);
    if (!disk)
        return 1;
    RG_ASSERT(disk->GetMode() == rk_DiskMode::Secure);

    LogInfo("Repository: %!..+%1%!0", disk->GetURL());
    LogInfo();

    LogInfo("Initializing...");
    if (!disk->Init(full_pwd, write_pwd))
        return 1;
    LogInfo();

    // Export master key
    char master_key[257] = {};
    {
        Span<const uint8_t> key = disk->GetFullKey();
        sodium_bin2base64(master_key, RG_SIZE(master_key), key.ptr, (size_t)key.len, sodium_base64_VARIANT_ORIGINAL);
    }

    LogInfo("Master key: %!..+%1%!0", master_key);
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
    LogInfo();
    LogInfo("Please %!.._save the master key in a secure place%!0, you can use it to decrypt the data even if the default account is lost or deleted.");

    return 0;
}

int RunExportKey(Span<const char *> arguments)
{
    // Options
    rk_Config config;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 export_key [-C <config>]

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory
    %!..+-u, --user <user>%!0            Set repository username
        %!..+--password <pwd>%!0         Set repository password)", FelixTarget);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
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
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

    if (!config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, true);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != rk_DiskMode::ReadWrite) {
        LogError("You must use the read-write password with this command");
        return 1;
    }
    LogInfo();

    // Export master key
    char master64[257] = {};
    {
        Span<const uint8_t> key = disk->GetFullKey();
        sodium_bin2base64(master64, RG_SIZE(master64), key.ptr, (size_t)key.len, sodium_base64_VARIANT_ORIGINAL);
    }

    LogInfo("Master key: %!..+%1%!0", master64);

    return 0;
}

int RunAddUser(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    bool authenticate = true;
    const char *master64 = nullptr;
    bool write_only = false;
    const char *full_pwd = nullptr;
    const char *write_pwd = nullptr;
    bool random_full_pwd = true;
    bool random_write_pwd = true;
    bool force = false;
    const char *username = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 add_user [-C <config>] <username>

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory
    %!..+-u, --user <user>%!0            Set repository username
        %!..+--password <pwd>%!0         Set repository password
    %!..+-k, --master_key [key]%!0       Use master key instead of username/password

    %!..+-w, --write_only%!0             Create write-only user
        %!..+--master_password [pwd]%!0  Set master password manually
        %!..+--write_password [pwd]%!0   Set write-only password manually

        %!..+--force%!0                  Overwrite exisiting user %!D..(if any)%!0)", FelixTarget);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
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
            } else if (opt.Test("-k", "--master_key", OptionType::OptionalValue)) {
                master64 = opt.current_value;
                authenticate = false;
            } else if (opt.Test("-w", "--write-only")) {
                write_only = true;
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
    }

    if (!username) {
        LogError("Missing username");
        return 1;
    }

    // Use master key instead of username/password
    if (!authenticate && !master64) {
        master64 = Prompt("Master key: ", nullptr, "*", &temp_alloc);
        if (!master64)
            return 1;
    }

    // Generate repository passwords
    if (!write_only) {
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

    if (!config.Complete(authenticate))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, authenticate);
    if (!disk)
        return 1;

    if (!authenticate) {
        RG_ASSERT(disk->GetMode() == rk_DiskMode::Secure);

        LocalArray<uint8_t, 128> master_key;
        size_t key_len;
        if (sodium_base642bin(master_key.data, RG_SIZE(master_key.data), master64, strlen(master64),
                              nullptr, &key_len, nullptr, sodium_base64_VARIANT_ORIGINAL) < 0) {
            LogError("Malformed master key");
            return 1;
        }
        master_key.len = (Size)key_len;

        if (!disk->Authenticate(master_key))
            return false;
    }

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != rk_DiskMode::ReadWrite) {
        LogError("You must use the read-write password with this command");
        return 1;
    }
    LogInfo();

    if (!disk->InitUser(username, full_pwd, write_pwd, force))
        return 1;

    LogInfo("Added user: %!..+%1%!0", username);
    LogInfo();
    if (write_only) {
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

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 delete_user [-C <config>] <username>

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
                print_usage(stdout);
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
    }

    if (!username) {
        LogError("Missing username");
        return 1;
    }

    if (!config.Complete(false))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, false);
    if (!disk)
        return 1;
    RG_ASSERT(disk->GetMode() == rk_DiskMode::Secure);

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    LogInfo();

    if (!force && TestStr(username, config.username)) {
        LogError("Cannot delete yourself (unless --force is used)");
        return 1;
    }

    if (!disk->DeleteUser(username))
        return 1;

    LogInfo("Deleted user: %!..+%1%!0", username);

    return 0;
}

}
