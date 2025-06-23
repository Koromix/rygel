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

namespace K {

#pragma pack(push, 1)
struct OldTagIntro {
    int8_t version;
    rk_ObjectID oid;
    char prefix[10];
    uint8_t key[32];
    int8_t count;
};
#pragma pack(pop)

int RunChangeCID(Span<const char *> arguments)
{
    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 change_cid [-C filename] [option...]%!0)"), FelixTarget);
        PrintCommonOptions(st);
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

    if (!rekkord_config.Complete())
        return 1;
    if (!rekkord_config.Validate())
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Config)) {
        LogError("Cannot change cache ID with %1 key", repo->GetRole());
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
    bool list = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 reset_cache [-C filename] [option...]%!0)"), FelixTarget);
        PrintCommonOptions(st);
        PrintLn(st, T(R"(
Cache options:

        %!..+--list%!0                     List existing blobs)"));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("--list")) {
                list = true;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!rekkord_config.Complete())
        return 1;
    if (!rekkord_config.Validate())
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

static bool ListOldTags(rk_Disk *disk, const rk_KeySet &keyset, Allocator *alloc, HeapArray<rk_TagInfo> *out_tags)
{
    K_ASSERT(keyset.HasMode(rk_AccessMode::Log));

    BlockAllocator temp_alloc;

    Size start_len = out_tags->len;
    K_DEFER_N(err_guard) { out_tags->RemoveFrom(start_len); };

    HeapArray<Span<const char>> mains;
    HeapArray<Span<const char>> fragments;
    {
        bool success = disk->ListFiles("tags", [&](const char *path, int64_t) {
            if (StartsWith(path, "tags/M/") && !strchr(path + 7, '/')) {
                Span<const char> filename = DuplicateString(path + 7, alloc);
                mains.Append(filename);
            } else if (StartsWith(path, "tags/P/")) {
                Span<const char> filename = DuplicateString(path + 7, &temp_alloc);
                fragments.Append(filename);
            }

            return true;
        });
        if (!success)
            return false;
    }

    std::sort(fragments.begin(), fragments.end(),
              [](Span<const char> filename1, Span<const char> filename2) {
        return CmpStr(filename1, filename2) < 0;
    });

    for (Span<const char> main: mains) {
        rk_TagInfo tag = {};

        LocalArray<uint8_t, 255> cypher;
        {
            size_t len = 0;
            sodium_base642bin(cypher.data, K_SIZE(cypher.data), main.ptr, main.len, nullptr,
                              &len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING);

            if (len != K_SIZE(OldTagIntro) + crypto_box_SEALBYTES + crypto_sign_BYTES) {
                LogError("Invalid tag cypher length");
                continue;
            }

            cypher.len = (Size)len;
        }

        OldTagIntro intro;
        if (crypto_box_seal_open((uint8_t *)&intro, cypher.data, cypher.len - crypto_sign_BYTES, keyset.keys.tkey, keyset.keys.lkey) != 0) {
            LogError("Failed to unseal tag data from '%1'", main);
            continue;
        }
        if (intro.version != TagVersion) {
            LogError("Unexpected tag version %1 (expected %2) in '%3'", intro.version, TagVersion, main);
            continue;
        }

        tag.name = main.ptr;
        tag.prefix = DuplicateString(MakeSpan(intro.prefix, K_SIZE(intro.prefix)), alloc).ptr;
        tag.oid = intro.oid;

        // Stash key and count in payload to make it available for next step
        tag.payload = AllocateSpan<uint8_t>(&temp_alloc, K_SIZE(intro.key));
        MemCpy((uint8_t *)tag.payload.ptr, intro.key, K_SIZE(intro.key));
        tag.payload.len = intro.count;

        out_tags->Append(tag);
    }

    Size j = start_len;
    for (Size i = start_len; i < out_tags->len; i++) {
        rk_TagInfo &tag = (*out_tags)[i];

        (*out_tags)[j] = tag;

        // Find relevant fragment names
        Span<const char> *first = std::lower_bound(fragments.begin(), fragments.end(), tag.prefix,
                                                  [](Span<const char> name, const char *prefix) {
            return CmpStr(name, prefix) < 0;
        });
        Size idx = first - fragments.ptr;

        HeapArray<uint8_t> payload(alloc);
        {
            int count = 0;

            for (Size k = idx; k < fragments.len; k++) {
                Span<const char> frag = fragments[k];

                if (frag.len <= 14 || frag[10] != '_' || frag[13] != '_')
                    continue;
                if (!StartsWith(frag, tag.prefix))
                    break;

                uint8_t nonce[24] = {};
                if (!ParseInt(frag.Take(11, 2), &nonce[23], (int)ParseFlag::End))
                    continue;
                if (nonce[23] != count)
                    continue;

                LocalArray<uint8_t, 255> cypher;
                {
                    size_t len = 0;
                    sodium_base642bin(cypher.data, K_SIZE(cypher.data), frag.ptr + 14, frag.len - 14, nullptr,
                                      &len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING);

                    if (len < crypto_secretbox_MACBYTES)
                        continue;

                    cypher.len = (Size)len;
                }

                payload.Grow(255);

                const uint8_t *key = tag.payload.ptr;
                if (crypto_secretbox_open_easy(payload.end(), cypher.data, cypher.len, nonce, key) != 0)
                    continue;
                payload.len += cypher.len - crypto_secretbox_MACBYTES;

                count++;
            }

            if (!count) {
                LogError("Cannot find fragment for tag '%1'", tag.name);
                continue;
            } else if (count != tag.payload.len) {
                LogError("Mismatch between tag and fragments of '%1'", tag.name);
                continue;
            }

            tag.payload = payload.TrimAndLeak();
        }

        j++;
    }
    out_tags->len = j;

    err_guard.Disable();
    return true;
}

int RunMigrateTags(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 migrate_tags [-C filename] [option...]%!0)"), FelixTarget);
        PrintCommonOptions(st);
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
        LogError("Cannot migrate tags with %1 key", repo->GetRole());
        return 1;
    }
    LogInfo();

    HeapArray<rk_TagInfo> tags;
    if (!ListOldTags(disk.get(), repo->GetKeys(), &temp_alloc, &tags))
        return 1;

    bool success = true;

    for (const rk_TagInfo &tag: tags) {
        LogInfo("Migrating tag for '%1'", tag.oid);

        if (repo->WriteTag(tag.oid, tag.payload)) {
            const char *path = Fmt(&temp_alloc, "tags/M/%1", tag.name).ptr;
            disk->DeleteFile(path);
        } else {
            success = false;
        }
    }

    if (!success)
        return 1;

    LogInfo("Done");

    return 0;
}

}
