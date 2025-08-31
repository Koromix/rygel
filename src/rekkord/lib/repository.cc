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
#include "config.hh"
#include "disk.hh"
#include "key.hh"
#include "lz4.hh"
#include "repository.hh"
#include "priv_key.hh"
#include "priv_repository.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#include "vendor/sha1/sha1.h"

namespace K {

static_assert(rk_MasterKeySize == crypto_kdf_blake2b_KEYBYTES);
static_assert(crypto_box_PUBLICKEYBYTES == 32);
static_assert(crypto_box_SECRETKEYBYTES == 32);
static_assert(crypto_box_SEALBYTES == 32 + 16);
static_assert(crypto_secretstream_xchacha20poly1305_HEADERBYTES == 24);
static_assert(crypto_secretstream_xchacha20poly1305_KEYBYTES == 32);
static_assert(crypto_secretbox_KEYBYTES == 32);
static_assert(crypto_secretbox_NONCEBYTES == 24);
static_assert(crypto_secretbox_MACBYTES == 16);
static_assert(crypto_sign_ed25519_SEEDBYTES == 32);
static_assert(crypto_sign_ed25519_BYTES == 64);
static_assert(crypto_kdf_blake2b_KEYBYTES == crypto_box_PUBLICKEYBYTES);
static_assert(crypto_pwhash_argon2id_SALTBYTES == 16);

rk_Repository::rk_Repository(rk_Disk *disk, const rk_Config &config)
    : disk(disk), compression_level(config.compression_level), retain(config.retain),
      tasks(config.threads > 0 ? config.threads : disk->GetDefaultThreads())
{
}

bool rk_Repository::IsRepository()
{
    StatResult ret = disk->TestFile("rekkord");
    return (ret == StatResult::Success);
}

bool rk_Repository::Init(Span<const uint8_t> mkey)
{
    K_ASSERT(!keyset);

    BlockAllocator temp_alloc;

    HeapArray<const char *> directories;
    HeapArray<const char *> files;

    K_DEFER_N(err_guard) {
        Lock();

        for (const char *filename: files) {
            disk->DeleteFile(filename);
        }
        for (Size i = directories.len - 1; i >= 0; i--) {
            const char *dirname = directories[i];
            disk->DeleteDirectory(dirname);
        }
    };

    keyset = (rk_KeySet *)AllocateSafe(K_SIZE(rk_KeySet));

    if (!rk_DeriveMasterKey(mkey, keyset))
        return false;

    // Prepare main directory
    switch (disk->TestDirectory("")) {
        case StatResult::Success: {
            if (!disk->IsEmpty()) {
                if (disk->TestFile("rekkord") == StatResult::Success) {
                    LogError("Repository '%1' looks already initialized", disk->GetURL());
                    return false;
                } else {
                    LogError("Directory '%1' exists and is not empty", disk->GetURL());
                    return false;
                }
            }
        } break;

        case StatResult::MissingPath: {
            if (!disk->CreateDirectory(""))
                return false;
            directories.Append("");
        } break;

        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    // Init subdirectories
    {
        std::mutex mutex;
        Async async(&tasks);

        const auto make_directory = [&](const char *dirname) {
            async.Run([&, dirname, this] {
                if (!disk->CreateDirectory(dirname))
                    return false;

                std::lock_guard lock(mutex);
                directories.Append(dirname);

                return true;
            });
        };

        make_directory("keys");
        make_directory("tags");
        make_directory("blobs");
        make_directory("tmp");

        if (!async.Sync())
            return false;

        make_directory("tags/M");
        make_directory("tags/P");

        for (char catalog: rk_BlobCatalogNames) {
            const char *dirname = Fmt(&temp_alloc, "blobs/%1", catalog).ptr;
            make_directory(dirname);
        }

        if (!async.Sync())
            return false;

        for (char catalog: rk_BlobCatalogNames) {
            for (int i = 0; i < 256; i++) {
                const char *dirname = Fmt(&temp_alloc, "blobs/%1/%2", catalog, FmtHex(i).Pad0(-2)).ptr;
                make_directory(dirname);
            }
        }

        if (!async.Sync())
            return false;
    }

    // Generate unique repository IDs
    {
        FillRandomSafe(ids.rid, K_SIZE(ids.rid));
        FillRandomSafe(ids.cid, K_SIZE(ids.cid));

        uint8_t buf[K_SIZE(ids)];
        MemCpy(buf, &ids, K_SIZE(ids));

        if (!WriteConfig("rekkord", buf, true))
            return false;
        files.Append("rekkord");
    }

    // Dummy file for conditional write support test
    if (disk->WriteFile("cw", {}) != rk_WriteResult::Success)
        return false;
    files.Append("cw");

    err_guard.Disable();
    return true;
}

bool rk_Repository::Authenticate(const char *filename)
{
    K_ASSERT(!keyset);

    keyset = (rk_KeySet *)AllocateSafe(K_SIZE(rk_KeySet));
    K_DEFER_N(err_guard) { Lock(); };

    if (!rk_LoadKeyFile(filename, keyset))
        return false;

    // Read unique identifiers
    {
        uint8_t buf[K_SIZE(ids)];
        if (!ReadConfig("rekkord", buf))
            return false;
        MemCpy(&ids, buf, K_SIZE(ids));
    }

    err_guard.Disable();
    return true;
}

void rk_Repository::Lock()
{
    ZeroSafe(&ids, K_SIZE(ids));
    ReleaseSafe(keyset, K_SIZE(*keyset));
    keyset = nullptr;
    str_alloc.ReleaseAll();
}

void rk_Repository::MakeID(Span<uint8_t> out_id) const
{
    K_ASSERT(out_id.len >= 16 && out_id.len <= 64);

    Span<const char> url = disk->GetURL();

    uint8_t sha512[64];
    {
        crypto_hash_sha512_state state;
        crypto_hash_sha512_init(&state);

        crypto_hash_sha512_update(&state, ids.rid, K_SIZE(ids.rid));
        crypto_hash_sha512_update(&state, (const uint8_t *)url.ptr, (size_t)url.len);
        crypto_hash_sha512_update(&state, (const uint8_t *)&out_id.len, K_SIZE(out_id.len));

        crypto_hash_sha512_final(&state, sha512);
    }

    MemCpy(out_id.ptr, sha512, out_id.len);
}

void rk_Repository::MakeSalt(rk_SaltKind kind, Span<uint8_t> out_buf) const
{
    K_ASSERT(HasMode(rk_AccessMode::Write));
    K_ASSERT(out_buf.len >= 8);
    K_ASSERT(out_buf.len <= 32);

    K_ASSERT(strlen(DerivationContext) == 8);
    uint64_t subkey = (uint64_t)kind;

    uint8_t buf[32] = {};
    crypto_kdf_blake2b_derive_from_key(buf, K_SIZE(buf), subkey, DerivationContext, keyset->keys.wkey);

    MemCpy(out_buf.ptr, buf, out_buf.len);
}

bool rk_Repository::ChangeCID()
{
    K_ASSERT(HasMode(rk_AccessMode::Config));

    IdSet new_ids = ids;
    FillRandomSafe(new_ids.cid, K_SIZE(new_ids.cid));

    // Write new IDs
    {
        uint8_t buf[K_SIZE(new_ids)];
        MemCpy(buf, &new_ids, K_SIZE(new_ids));

        if (!WriteConfig("rekkord", buf, true))
            return false;
    }

    MemCpy(ids.cid, new_ids.cid, K_SIZE(new_ids.cid));

    return true;
}

static inline FmtArg GetBlobPrefix(const rk_Hash &hash)
{
    uint16_t prefix = hash.raw[0];
    return FmtHex(prefix).Pad0(-2);
}

static int64_t PadMe(int64_t len)
{
    K_ASSERT(len > 0);

    uint64_t e = 63 - CountLeadingZeros((uint64_t)len);
    uint64_t s = 63 - CountLeadingZeros(e) + 1;
    uint64_t mask = (1ull << (e - s)) - 1;

    uint64_t padded = (len + mask) & ~mask;
    uint64_t padding = padded - len;

    return (int64_t)padding;
}

bool rk_Repository::ReadBlob(const rk_ObjectID &oid, int *out_type, HeapArray<uint8_t> *out_blob, int64_t *out_size)
{
    K_ASSERT(HasMode(rk_AccessMode::Read));

    char path[256];
    Fmt(path, "blobs/%1/%2/%3", rk_BlobCatalogNames[(int)oid.catalog], GetBlobPrefix(oid.hash), oid.hash);

    Size prev_len = out_blob->len;
    K_DEFER_N(err_guard) { out_blob->RemoveFrom(prev_len); };

    HeapArray<uint8_t> raw;
    if (disk->ReadFile(path, &raw) < 0)
        return false;
    Span<const uint8_t> remain = raw;

    // Init blob decryption
    crypto_secretstream_xchacha20poly1305_state state;
    int8_t type;
    {
        BlobIntro intro;
        if (remain.len < K_SIZE(intro)) {
            LogError("Truncated blob '%1'", oid);
            return false;
        }
        MemCpy(&intro, remain.ptr, K_SIZE(intro));

        if (intro.version != BlobVersion) {
            LogError("Unexpected blob version %1 (expected %2)", intro.version, BlobVersion);
            return false;
        }
        if (intro.type < 0) {
            LogError("Invalid blob type %1", intro.type);
            return false;
        }

        type = intro.type;

        uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
        if (crypto_box_seal_open(key, intro.ekey, K_SIZE(intro.ekey), keyset->keys.wkey, keyset->keys.dkey) != 0) {
            LogError("Failed to unseal blob '%1'", path);
            return false;
        }

        if (crypto_secretstream_xchacha20poly1305_init_pull(&state, intro.header, key) != 0) {
            LogError("Failed to initialize symmetric decryption of '%1'", path);
            return false;
        }

        remain.ptr += K_SIZE(BlobIntro);
        remain.len -= K_SIZE(BlobIntro);
    }

    // Read and decrypt blob
    {
        DecodeLZ4 lz4;
        bool eof = false;

        while (!eof && remain.len) {
            Size in_len = std::min(remain.len, BlobSplit + (Size)crypto_secretstream_xchacha20poly1305_ABYTES);
            Size out_len = in_len - crypto_secretstream_xchacha20poly1305_ABYTES;

            Span<const uint8_t> cypher = MakeSpan(remain.ptr, in_len);
            Span<uint8_t> buf = lz4.PrepareAppend(out_len);

            unsigned long long buf_len = 0;
            uint8_t tag;
            if (crypto_secretstream_xchacha20poly1305_pull(&state, buf.ptr, &buf_len, &tag,
                                                           cypher.ptr, cypher.len, nullptr, 0) != 0) {
                LogError("Failed during symmetric decryption of '%1'", path);
                return false;
            }

            remain.ptr += cypher.len;
            remain.len -= cypher.len;

            eof = (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL);

            bool success = lz4.Flush(eof, [&](Span<const uint8_t> buf) {
                out_blob->Append(buf);
                return true;
            });
            if (!success)
                return false;
        }

        if (!eof) {
            LogError("Truncated blob '%1'", oid);
            return false;
        }
    }

    *out_type = type;
    if (out_size) {
        *out_size = raw.len;
    }

    err_guard.Disable();
    return true;
}

rk_WriteResult rk_Repository::WriteBlob(const rk_ObjectID &oid, int type, Span<const uint8_t> blob, int64_t *out_size)
{
    K_ASSERT(HasMode(rk_AccessMode::Write));
    K_ASSERT(type >= 0 && type < INT8_MAX);

    char path[256];
    Fmt(path, "blobs/%1/%2/%3", rk_BlobCatalogNames[(int)oid.catalog], GetBlobPrefix(oid.hash), oid.hash);

    HeapArray<uint8_t> raw;
    crypto_secretstream_xchacha20poly1305_state state;

    // Write blob intro
    {
        BlobIntro intro = {};

        intro.version = BlobVersion;
        intro.type = (int8_t)type;

        uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
        FillRandomSafe(key, K_SIZE(key));

        if (crypto_secretstream_xchacha20poly1305_init_push(&state, intro.header, key) != 0) {
            LogError("Failed to initialize symmetric encryption");
            return rk_WriteResult::OtherError;
        }
        if (crypto_box_seal(intro.ekey, key, K_SIZE(key), keyset->keys.wkey) != 0) {
            LogError("Failed to seal symmetric key");
            return rk_WriteResult::OtherError;
        }

        Span<const uint8_t> buf = MakeSpan((const uint8_t *)&intro, K_SIZE(intro));
        raw.Append(buf);
    }

    // Initialize compression
    EncodeLZ4 lz4;
    if (!lz4.Start(compression_level))
        return rk_WriteResult::OtherError;

    // Encrypt blob data
    {
        bool complete = false;
        int64_t compressed = 0;

        do {
            Size frag_len = std::min(BlobSplit, blob.len);
            Span<const uint8_t> frag = blob.Take(0, frag_len);

            blob.ptr += frag.len;
            blob.len -= frag.len;

            complete |= (frag.len < BlobSplit);

            if (!lz4.Append(frag))
                return rk_WriteResult::OtherError;

            bool success = lz4.Flush(complete, [&](Span<const uint8_t> buf) {
                // This should rarely loop because data should compress to less
                // than BlobSplit but we ought to be safe ;)

                Size processed = 0;

                while (buf.len >= BlobSplit) {
                    Size piece_len = std::min(BlobSplit, buf.len);
                    Span<const uint8_t> piece = buf.Take(0, piece_len);

                    buf.ptr += piece.len;
                    buf.len -= piece.len;
                    processed += piece.len;

                    uint8_t cypher[BlobSplit + crypto_secretstream_xchacha20poly1305_ABYTES];
                    unsigned long long cypher_len;
                    crypto_secretstream_xchacha20poly1305_push(&state, cypher, &cypher_len, piece.ptr, piece.len, nullptr, 0, 0);

                    Span<const uint8_t> final = MakeSpan(cypher, (Size)cypher_len);
                    raw.Append(final);
                }

                compressed += processed;

                if (!complete)
                    return processed;

                processed += buf.len;
                compressed += buf.len;

                // Reduce size disclosure with Padmé algorithm
                // More information here: https://lbarman.ch/blog/padme/
                int64_t padding = PadMe((uint64_t)compressed);

                // Write remaining bytes and start padding
                {
                    LocalArray<uint8_t, BlobSplit> expand;

                    Size pad = (Size)std::min(padding, (int64_t)K_SIZE(expand.data) - buf.len);

                    MemCpy(expand.data, buf.ptr, buf.len);
                    MemSet(expand.data + buf.len, 0, pad);
                    expand.len = buf.len + pad;

                    padding -= pad;

                    uint8_t cypher[BlobSplit + crypto_secretstream_xchacha20poly1305_ABYTES];
                    unsigned char tag = !padding ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
                    unsigned long long cypher_len;
                    crypto_secretstream_xchacha20poly1305_push(&state, cypher, &cypher_len, expand.data, expand.len, nullptr, 0, tag);

                    Span<const uint8_t> final = MakeSpan(cypher, (Size)cypher_len);
                    raw.Append(final);
                }

                // Finalize padding
                while (padding) {
                    static const uint8_t padder[BlobSplit] = {};

                    Size pad = (Size)std::min(padding, (int64_t)K_SIZE(padder));

                    padding -= pad;

                    uint8_t cypher[BlobSplit + crypto_secretstream_xchacha20poly1305_ABYTES];
                    unsigned char tag = !padding ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
                    unsigned long long cypher_len;
                    crypto_secretstream_xchacha20poly1305_push(&state, cypher, &cypher_len, padder, pad, nullptr, 0, tag);

                    Span<const uint8_t> final = MakeSpan(cypher, (Size)cypher_len);
                    raw.Append(final);
                }

                return processed;
            });
            if (!success)
                return rk_WriteResult::OtherError;
        } while (!complete);
    }

    rk_WriteSettings settings = {};

    settings.conditional = HasConditionalWrites();
    settings.retain = retain;

    settings.checksum = disk->GetChecksumType();

    switch (settings.checksum) {
        case rk_ChecksumType::None: {} break;

        case rk_ChecksumType::CRC32: { settings.hash.crc32 = CRC32(0, raw); } break;
        case rk_ChecksumType::CRC32C: { settings.hash.crc32c = CRC32C(0, raw); } break;
        case rk_ChecksumType::CRC64nvme: { settings.hash.crc64nvme = CRC64nvme(0, raw); } break;
        case rk_ChecksumType::SHA1: { SHA1(settings.hash.sha1, raw.ptr, (size_t)raw.len); } break;
        case rk_ChecksumType::SHA256: { crypto_hash_sha256(settings.hash.sha256, raw.ptr, (size_t)raw.len); } break;
    }

    rk_WriteResult ret = disk->WriteFile(path, raw, settings);

    switch (ret) {
        case rk_WriteResult::Success:
        case rk_WriteResult::AlreadyExists: {} break;

        default: return rk_WriteResult::OtherError;
    }

    if (out_size) {
        *out_size = raw.len;
    }
    return ret;
}

bool rk_Repository::RetainBlob(const rk_ObjectID &oid)
{
    if (!retain)
        return true;

    char path[256];
    Fmt(path, "blobs/%1/%2/%3", rk_BlobCatalogNames[(int)oid.catalog], GetBlobPrefix(oid.hash), oid.hash);

    return disk->RetainFile(path, retain);
}

StatResult rk_Repository::TestBlob(const rk_ObjectID &oid, int64_t *out_size)
{
    char path[256];
    Fmt(path, "blobs/%1/%2/%3", rk_BlobCatalogNames[(int)oid.catalog], GetBlobPrefix(oid.hash), oid.hash);

    return disk->TestFile(path, out_size);
}

bool rk_Repository::WriteTag(const rk_ObjectID &oid, Span<const uint8_t> payload)
{
    // Accounting for the prefix, order value, encryption and base64 overhead, that's what remains per fragment
    const Size MaxFragmentSize = 148;

    K_ASSERT(HasMode(rk_AccessMode::Write));

    BlockAllocator temp_alloc;

    Size fragments = (payload.len + MaxFragmentSize - 1) / MaxFragmentSize;

    // Sanity check
    if (fragments >= 5) {
        LogError("Excessive tag data size");
        return false;
    }

    uint8_t prefix[K_SIZE(TagIntro::prefix)];
    uint8_t pwd[K_SIZE(TagIntro::key)];
    FillRandomSafe(prefix, K_SIZE(prefix));
    FillRandomSafe(pwd, K_SIZE(pwd));

    const char *main = Fmt(&temp_alloc, "tags/M/%1", FmtHex(keyset->kid)).ptr;

    HeapArray<const char *> paths;

    // Create main tag path
    {
        TagIntro intro = {};

        intro.version = TagVersion;
        intro.oid = oid;
        MemCpy(intro.prefix, prefix, K_SIZE(prefix));
        MemCpy(intro.key, pwd, K_SIZE(pwd));
        intro.count = (int8_t)fragments;

        uint8_t cypher[K_SIZE(intro) + crypto_box_SEALBYTES + crypto_sign_BYTES];

        if (crypto_box_seal(cypher, (const uint8_t *)&intro, K_SIZE(intro), keyset->keys.tkey) != 0) {
            LogError("Failed to seal tag payload");
            return false;
        }

        // Sign it to avoid tampering by other users
        crypto_sign_ed25519_detached(std::end(cypher) - crypto_sign_BYTES, nullptr, cypher,
                                     K_SIZE(cypher) - crypto_sign_BYTES, keyset->keys.skey);

        HeapArray<char> buf(&temp_alloc);
        buf.Reserve(512);

        Fmt(&buf, "%1/", main);
        sodium_bin2base64(buf.end(), buf.Available(), cypher, K_SIZE(cypher), sodium_base64_VARIANT_URLSAFE_NO_PADDING);
        buf.len += sodium_base64_encoded_len(K_SIZE(cypher), sodium_base64_VARIANT_URLSAFE_NO_PADDING) - 1;

        Span<const char> path = buf.Leak();
        paths.Append(path.ptr);
    }

    // So, we use 192-bit keys instead of directly 256-bit because the base name has to fit inside 255
    // characters to work on all filesystems (most Linux filesystems limit names to 255 bytes, for example).
    // Derive the full 256-bit key from the random 192-bit key with Argon2id, using the random
    // prefix as a salt.
    uint8_t key[32];
    if (crypto_pwhash_argon2id(key, K_SIZE(key), (const char *)pwd, K_SIZE(pwd), prefix,
                               crypto_pwhash_argon2id_OPSLIMIT_MIN,
                               crypto_pwhash_argon2id_MEMLIMIT_MIN,
                               crypto_pwhash_argon2id_ALG_ARGON2ID13) != 0) {
        LogError("Failed to expand encryption key");
        return false;
    }
    static_assert(K_SIZE(prefix) == crypto_pwhash_argon2id_SALTBYTES);

    // Encrypt payload
    for (uint8_t i = 0; payload.len; i++) {
        Size frag_len = std::min(payload.len, MaxFragmentSize);

        uint8_t cypher[MaxFragmentSize + crypto_secretbox_MACBYTES];
        uint8_t nonce[24] = {};

        nonce[23] = i;
        crypto_secretbox_easy(cypher, payload.ptr, frag_len, nonce, key);

        HeapArray<char> buf(&temp_alloc);
        buf.Reserve(512);

        Fmt(&buf, "tags/P/%1_%2_", FmtHex(prefix), FmtArg(i).Pad0(-2));
        sodium_bin2base64(buf.end(), buf.Available(), cypher, frag_len + crypto_secretbox_MACBYTES, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
        buf.len += sodium_base64_encoded_len(frag_len + crypto_secretbox_MACBYTES, sodium_base64_VARIANT_URLSAFE_NO_PADDING) - 1;

        Span<const char> path = buf.Leak();
        paths.Append(path.ptr);

        payload.ptr += frag_len;
        payload.len -= frag_len;
    }

    if (!disk->CreateDirectory(main))
        return false;

    // Create tag files
    for (const char *path: paths) {
        rk_WriteSettings settings = { .retain = retain };
        rk_WriteResult ret = disk->WriteFile(path, {}, settings);

        if (ret != rk_WriteResult::Success)
            return false;
    }

    // Export keyset badge
    if (keyset->type != rk_KeyType::Master) {
        char path[256];
        Fmt(path, "keys/%1", FmtHex(keyset->kid));

        rk_WriteSettings settings = { .conditional = HasConditionalWrites(), .retain = retain };
        rk_WriteResult ret = disk->WriteFile(path, keyset->badge, settings);

        switch (ret) {
            case rk_WriteResult::Success:
            case rk_WriteResult::AlreadyExists: {} break;
            case rk_WriteResult::OtherError: return false;
        }
    }

    return true;
}

bool rk_Repository::ListTags(Allocator *alloc, HeapArray<rk_TagInfo> *out_tags)
{
    K_ASSERT(HasMode(rk_AccessMode::Log));

    BlockAllocator temp_alloc;

    Size start_len = out_tags->len;
    K_DEFER_N(err_guard) { out_tags->RemoveFrom(start_len); };

    HeapArray<Span<const char>> mains;
    HeapArray<Span<const char>> fragments;
    {
        bool success = disk->ListFiles("tags", [&](const char *path, int64_t) {
            if (StartsWith(path, "tags/M/")) {
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

        if (main.len <= 33 && main[32] != '/')
            continue;
        main = main.Take(33, main.len - 33);

        LocalArray<uint8_t, 255> cypher;
        {
            size_t len = 0;
            sodium_base642bin(cypher.data, K_SIZE(cypher.data), main.ptr, main.len, nullptr,
                              &len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING);

            if (len != K_SIZE(TagIntro) + crypto_box_SEALBYTES + crypto_sign_BYTES) {
                LogError("Invalid tag cypher length");
                continue;
            }

            cypher.len = (Size)len;
        }

        TagIntro intro;
        if (crypto_box_seal_open((uint8_t *)&intro, cypher.data, cypher.len - crypto_sign_BYTES,
                                 keyset->keys.tkey, keyset->keys.lkey) != 0) {
            LogError("Failed to unseal tag data from '%1'", main);
            continue;
        }
        if (intro.version != TagVersion) {
            LogError("Unexpected tag version %1 (expected %2) in '%3'", intro.version, TagVersion, main);
            continue;
        }

        tag.name = main.ptr;
        tag.prefix = Fmt(alloc, "%1", FmtHex(intro.prefix)).ptr;
        tag.oid = intro.oid;

        // Stash key and count in payload to make it available for next step
        tag.payload = AllocateSpan<uint8_t>(&temp_alloc, 32);
        if (crypto_pwhash_argon2id((uint8_t *)tag.payload.ptr, tag.payload.len,
                                   (const char *)intro.key, K_SIZE(intro.key), intro.prefix,
                                   crypto_pwhash_argon2id_OPSLIMIT_MIN,
                                   crypto_pwhash_argon2id_MEMLIMIT_MIN,
                                   crypto_pwhash_argon2id_ALG_ARGON2ID13) != 0) {
            LogError("Failed to expand encryption key");
            continue;
        }
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

                if (frag.len <= 36 || frag[32] != '_' || frag[35] != '_')
                    continue;
                if (!StartsWith(frag, tag.prefix))
                    break;

                uint8_t nonce[24] = {};
                if (!ParseInt(frag.Take(33, 2), &nonce[23], (int)ParseFlag::End))
                    continue;
                if (nonce[23] != count)
                    continue;

                LocalArray<uint8_t, 255> cypher;
                {
                    size_t len = 0;
                    sodium_base642bin(cypher.data, K_SIZE(cypher.data), frag.ptr + 36, frag.len - 36, nullptr,
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

bool rk_Repository::TestConditionalWrites(bool *out_cw)
{
    if (!cw_tested) {
        std::lock_guard<std::mutex> lock(cw_mutex);

        if (!cw_tested) {
            rk_WriteResult ret = disk->WriteFile("cw", {}, { .conditional = true });

            switch (ret) {
                case rk_WriteResult::Success: {
                    LogWarning("This repository does not seem to support conditional writes");
                    cw_support = false;
                } break;
                case rk_WriteResult::AlreadyExists: { cw_support = true; } break;
                case rk_WriteResult::OtherError: return false;
            }
        }

        cw_tested = true;
    }

    if (out_cw) {
        *out_cw = cw_support;
    }
    return true;
}

bool rk_Repository::WriteConfig(const char *path, Span<const uint8_t> data, bool overwrite)
{
    K_ASSERT(HasMode(rk_AccessMode::Config));
    K_ASSERT(crypto_secretbox_MACBYTES + data.len <= K_SIZE(ConfigData::cypher));

    if (!overwrite && !HasConditionalWrites()) {
        switch (disk->TestFile(path)) {
            case StatResult::Success: {
                LogError("Config file '%1' already exists", path);
                return false;
            } break;
            case StatResult::MissingPath: { overwrite = true; } break;
            case StatResult::AccessDenied:
            case StatResult::OtherError: return false;
        }
    }

    ConfigData config = {};

    config.version = ConfigVersion;
    config.len = LittleEndian((uint16_t)data.len);
    FillRandomSafe(config.nonce, K_SIZE(config.nonce));

    // Encrypt and sign to detect tampering
    crypto_secretbox_easy(config.cypher, data.ptr, data.len, config.nonce, keyset->keys.akey);
    crypto_sign_ed25519_detached(config.sig, nullptr, (const uint8_t *)&config, offsetof(ConfigData, sig), keyset->keys.ckey);

    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&config, K_SIZE(config));

    rk_WriteSettings settings = { .conditional = !overwrite };
    rk_WriteResult ret = disk->WriteFile(path, buf, settings);

    switch (ret) {
        case rk_WriteResult::Success: return true;
        case rk_WriteResult::AlreadyExists: {
            LogError("Config file '%1' already exists", path);
            return false;
        } break;
        case rk_WriteResult::OtherError: return false;
    }

    K_UNREACHABLE();
}

bool rk_Repository::ReadConfig(const char *path, Span<uint8_t> out_buf)
{
    ConfigData config = {};

    Span<uint8_t> buf = MakeSpan((uint8_t *)&config, K_SIZE(config));
    Size len = disk->ReadFile(path, buf);

    if (len < 0)
        return false;

    // Sanity checks
    if (len < (Size)offsetof(ConfigData, cypher)) {
        LogError("Malformed config file '%1'", path);
        return false;
    }
    if (config.version != ConfigVersion) {
        LogError("Unexpected config version %1 (expected %2)", config.version, ConfigVersion);
        return false;
    }
    if (LittleEndian(config.len) != out_buf.len) {
        LogError("Malformed config file '%1'", path);
        return false;
    }

    if (crypto_sign_ed25519_verify_detached(config.sig, (const uint8_t *)&config, offsetof(ConfigData, sig), keyset->keys.akey) != 0) {
        LogError("Invalid signature in config '%1'", path);
        return false;
    }
    if (crypto_secretbox_open_easy(out_buf.ptr, config.cypher, 16 + out_buf.len, config.nonce, keyset->keys.akey) != 0) {
        LogError("Failed to decrypt config '%1'", path);
        return false;
    }

    return true;
}

bool rk_Repository::HasConditionalWrites()
{
    bool cw = false;
    TestConditionalWrites(&cw);
    return cw;
}

std::unique_ptr<rk_Repository> rk_OpenRepository(rk_Disk *disk, const rk_Config &config, bool authenticate)
{
#if defined(K_DEBUG)
    unsigned int flags = authenticate ? (int)rk_ConfigFlag::RequireAuth : 0;
    K_ASSERT(config.Validate(flags));
#endif

    if (!disk)
        return nullptr;

    std::unique_ptr<rk_Repository> repo = std::make_unique<rk_Repository>(disk, config);

    if (authenticate && !repo->Authenticate(config.key_filename))
        return nullptr;

    return repo;
}

static inline int ParseHexadecimalChar(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        return -1;
    }
}

bool rk_ParseOID(Span<const char> str, rk_ObjectID *out_oid)
{
    if (str.len != 65)
        return false;

    rk_ObjectID oid = {};

    // Decode prefix
    {
        const auto ptr = std::find(std::begin(rk_BlobCatalogNames), std::end(rk_BlobCatalogNames), str[0]);

        if (ptr == std::end(rk_BlobCatalogNames))
            return false;

        oid.catalog = (rk_BlobCatalog)(ptr - rk_BlobCatalogNames);
    }

    for (Size i = 2, j = 0; i < 66; i += 2, j++) {
        int high = ParseHexadecimalChar(str[i - 1]);
        int low = ParseHexadecimalChar(str[i]);

        if (high < 0 || low < 0)
            return false;

        oid.hash.raw[j] = (uint8_t)((high << 4) | low);
    }

    if (out_oid) {
        *out_oid = oid;
    }
    return true;
}

}
