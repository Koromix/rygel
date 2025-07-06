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
#include "rekkord.hh"
#include "src/rekkord/lib/priv_repository.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

#pragma pack(push, 1)
struct OldKeyData {
    uint8_t salt[16];
    uint8_t nonce[24];
    int8_t role;
    uint8_t cypher[16 + MaxKeys * 32];
    uint8_t sig[64];
};
#pragma pack(pop)

int RunChangeCID(Span<const char *> arguments)
{
    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 change_cid [-C filename] [option...]%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!rekkord_config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Config)) {
        LogError("Cannot change ID with %1 role", repo->GetRole());
        return 1;
    }
    LogInfo();

    if (!repo->ChangeCID())
        return 1;

    LogInfo("Changed cache ID");

    return 0;
}

int RunResetCache(Span<const char *> arguments)
{
    // Options
    bool list = true;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 reset_cache [-C filename] [option...]%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
Cache options:

        %!..+--clear%!0                    Skip list of existing blobs)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("--clear")) {
                list = false;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!rekkord_config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    LogInfo();

    rk_Cache cache;
    if (!cache.Open(repo.get(), false))
        return false;

    LogInfo("Resetting cache...");
    if (!cache.Reset(list))
        return 1;
    if (!cache.Close())
        return 1;
    LogInfo("Done");

    return 0;
}

int RunMigrateUser(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *username = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 migrate_user [-C filename] [option...] username%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        username = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!rekkord_config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Config)) {
        LogError("Cannot migrate users with %1 role", repo->GetRole());
        return 1;
    }
    LogInfo();

    const Size PayloadSize = RG_SIZE(OldKeyData::cypher) - 16;

    OldKeyData data = {};
    uint8_t *payload = (uint8_t *)AllocateSafe(PayloadSize);
    RG_DEFER {
        ZeroSafe(&data, RG_SIZE(data));
        ReleaseSafe(payload, PayloadSize);
    };

    // Read file data
    {
        const char *path = Fmt(&temp_alloc, "keys/%1", username).ptr;

        LocalArray<uint8_t, 2048> buf;
        RG_DEFER { ZeroSafe(buf.data, buf.len); };

        buf.len = disk->ReadFile(path, buf.data);

        if (buf.len != RG_SIZE(OldKeyData)) {
            if (buf.len >= 0) {
                LogError("Cannot migrate keys in '%1'", path);
            }
            return 1;
        }

        MemCpy(&data, buf.data, RG_SIZE(data));
    }

    const char *pwd = Prompt("Existing user password: ", nullptr, "*", &temp_alloc);
    if (!pwd)
        return 1;

    // Decrypt payload
    {
        uint8_t key[32];

        if (crypto_pwhash(key, 32, pwd, strlen(pwd), data.salt, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_ARGON2ID13) != 0) {
            LogError("Failed to derive key from password (exhausted resource?)");
            return 1;
        }

        if (crypto_secretbox_open_easy(payload, data.cypher, RG_SIZE(data.cypher), data.nonce, key)) {
            LogError("Failed to open repository (wrong password?)");
            return 1;
        }
    }

    // Sanity checks
    if (data.role < 0 || data.role >= RG_LEN(rk_UserRoleNames)) {
        LogError("Invalid user role %1", data.role);
        return 1;
    }

    rk_UserRole role = (rk_UserRole)data.role;

    if (!repo->DeleteUser(username))
        return 1;
    if (!repo->InitUser(username, role, pwd))
        return 1;

    LogInfo("User successfully migrated");

    return 0;
}

}
