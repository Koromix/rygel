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
#include "src/rekkord/lib/priv_tape.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

#pragma pack(push, 1)
struct OldTagIntro {
    int8_t version;
    rk_ObjectID oid;
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

int RunMigrateTags(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 migrate_tags [-C filename] [option...]%!0
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
    if (!repo->HasMode(rk_AccessMode::Read) || !repo->HasMode(rk_AccessMode::Write)) {
        LogError("Cannot migrate tags with %1 role", repo->GetRole());
        return 1;
    }
    LogInfo();

    const rk_KeySet *keyset = repo->GetKeySet();

    if (!disk->CreateDirectory("tags/M"))
        return 1;
    if (!disk->CreateDirectory("tags/P"))
        return 1;

    HeapArray<Span<const char>> filenames;
    {
        bool success = disk->ListFiles("tags", [&](const char *path, int64_t) {
            if (StartsWith(path, "tags/") && !strchr(path + 5, '/')) {
                Span<const char> filename = DuplicateString(path, &temp_alloc);
                filenames.Append(filename);
            }

            return true;
        });
        if (!success)
            return 1;

        std::sort(filenames.begin(), filenames.end(),
                  [](Span<const char> filename1, Span<const char> filename2) {
            return CmpStr(filename1, filename2) < 0;
        });
    }

    Size offset = 0;

    while (offset < filenames.len) {
        Size end = offset + 1;
        RG_DEFER { offset = end; };

        // Extract common ID
        Span<const char> prefix = {};
        {
            Span<const char> filename = filenames[offset];
            Span<const char> basename = SplitStrReverse(filenames[offset], '/');

            if (basename.len <= 36 || basename[32] != '_' || basename[35] != '_') {
                LogError("Malformed tag file '%1'", filename);
                continue;
            }

            prefix = basename.Take(0, 32);
        }

        // How many fragments?
        while (end < filenames.len) {
            Span<const char> basename = SplitStrReverse(filenames[end], '/');

            // Error will be issued in next loop iteration
            if (basename.len <= 36 || basename[32] != '_' || basename[35] != '_')
                break;
            if (!StartsWith(basename, prefix))
                break;

            end++;
        }

        // Reassemble fragments
        HeapArray<uint8_t> cypher;
        for (Size i = offset; i < end; i++) {
            Span<const char> basename = SplitStrReverse(filenames[i], '/');
            Span<const char> base64 = basename.Take(36, basename.len - 36);

            cypher.Grow(base64.len);

            size_t len = 0;
            sodium_base642bin(cypher.end(), base64.len, base64.ptr, base64.len, nullptr,
                              &len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
            cypher.len += (Size)len;
        }

        if (cypher.len < (Size)crypto_box_SEALBYTES + RG_SIZE(OldTagIntro) + (Size)crypto_sign_BYTES) {
            LogError("Truncated cypher in tag");
            continue;
        }

        // Check signature first to detect tampering
        {
            Span<const uint8_t> msg = MakeSpan(cypher.ptr, cypher.len - crypto_sign_BYTES);
            const uint8_t *sig = msg.end();

            if (crypto_sign_ed25519_verify_detached(sig, msg.ptr, msg.len, keyset->vkey)) {
                LogError("Invalid signature for tag '%1'", prefix);
                continue;
            }
        }

        Size src_len = cypher.len - crypto_box_SEALBYTES - crypto_sign_BYTES;
        Span<uint8_t> src = AllocateSpan<uint8_t>(&temp_alloc, src_len);

        // Get payload back at least
        if (crypto_box_seal_open(src.ptr, cypher.ptr, cypher.len - crypto_sign_BYTES, keyset->tkey, keyset->lkey) != 0) {
            LogError("Failed to unseal tag data from '%1'", prefix);
            continue;
        }

        OldTagIntro intro = {};
        MemCpy(&intro, src.ptr, RG_SIZE(intro));

        if (intro.version == 3) {
            src = src.Take(RG_SIZE(intro), src.len - RG_SIZE(intro));
        } else if (intro.version == 2) {
            MemMove((uint8_t *)&intro.oid + 1, (const uint8_t *)&intro.oid, RG_SIZE(rk_Hash));
            intro.oid.catalog = rk_BlobCatalog::Meta;

            Span<uint8_t> copy = AllocateSpan<uint8_t>(&temp_alloc, src.len - RG_SIZE(OldTagIntro) + 1);
            MemCpy(copy.ptr, src.ptr + RG_SIZE(OldTagIntro) - 1, copy.len);

            src = copy;
        } else {
            LogError("Unexpected tag version %1 (expected %2) in '%3'", intro.version, TagVersion, prefix);
            continue;
        }

        if (!intro.oid.IsValid()) {
            LogError("Invalid tag OID in '%1'", prefix);
            continue;
        }
        if (src.len < (Size)offsetof(SnapshotHeader2, channel) + 1 ||
                src.len > RG_SIZE(SnapshotHeader2)) {
            LogError("Malformed snapshot tag (ignoring)");
            continue;
        }

        // Decode tag payload
        SnapshotHeader3 header = {};
        {
            SnapshotHeader2 header2 = {};

            MemCpy(&header2, src.ptr, src.len);
            header2.channel[RG_SIZE(header2.channel) - 1] = 0;

            header.time = header2.time;
            header.size = header2.size;
            header.stored = header2.stored;
            MemCpy(header.channel, header2.channel, RG_SIZE(header.channel));
        }

        LogInfo("Creating tag for %1 (%2)", intro.oid, header.channel);

        // Create new tag file
        {
            Size payload_len = offsetof(SnapshotHeader3, channel) + strlen(header.channel) + 1;
            Span<const uint8_t> payload = MakeSpan((const uint8_t *)&header, payload_len);

            if (!repo->WriteTag(intro.oid, payload))
                return 1;
        }

        // Delete old tag files
        for (Size i = offset; i < end; i++) {
            const char *filename = filenames[i].ptr;

            LogInfo("Deleting '%1'", filename);
            disk->DeleteFile(filename);
        }
    }

    LogInfo("Done");

    return 0;
}

}
