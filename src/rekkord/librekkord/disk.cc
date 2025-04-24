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
#include "config.hh"
#include "disk.hh"
#include "lz4.hh"
#include "priv_disk.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

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

static void SeedSigningPair(uint8_t sk[32], uint8_t pk[32])
{
    crypto_hash_sha512(pk, sk, 32);
    crypto_scalarmult_ed25519_base(pk, pk);
}

rk_Disk::~rk_Disk()
{
    Lock();
}

bool rk_Disk::Authenticate(const char *username, const char *pwd)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);
    RG_ASSERT(!keyset);

    RG_DEFER_N(err_guard) { Lock(); };

    const char *filename = Fmt(&str_alloc, "keys/%1", username).ptr;

    if (!CheckRepository())
        return false;

    // Does user exist?
    switch (TestRaw(filename)) {
        case StatResult::Success: {} break;
        case StatResult::MissingPath: {
            LogError("User '%1' does not exist", username);
            return false;
        } break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    keyset = (KeySet *)AllocateSafe(RG_SIZE(KeySet));

    // Open disk and determine mode
    {
        rk_UserRole role;
        LocalArray<uint8_t[32], MaxKeys> keys;

        if (!ReadKeys(filename, pwd, &role, keyset))
            return false;

        switch (role) {
            case rk_UserRole::ReadWrite: {
                mode = rk_DiskMode::Full;

                SeedSigningPair(keyset->ckey, keyset->akey);
                crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
                crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
                SeedSigningPair(keyset->ukey, keyset->vkey);
            } break;

            case rk_UserRole::WriteOnly: {
                mode = rk_DiskMode::WriteOnly;

                ZeroSafe(keyset->ckey, RG_SIZE(keyset->ckey));
                ZeroSafe(keyset->dkey, RG_SIZE(keyset->dkey));
                ZeroSafe(keyset->lkey, RG_SIZE(keyset->lkey));
                ZeroSafe(keyset->ukey, RG_SIZE(keyset->ukey));
            } break;
        }

        user = DuplicateString(username, &str_alloc).ptr;
    }

    // Get cache ID
    if (!ReadConfig("rekkord", id))
        return false;

    err_guard.Disable();
    return true;
}

bool rk_Disk::Authenticate(Span<const uint8_t> mkey)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);
    RG_ASSERT(!keyset);

    RG_DEFER_N(err_guard) { Lock(); };

    if (mkey.len != rk_MasterKeySize) {
        LogError("Malformed master key");
        return false;
    }

    if (!CheckRepository())
        return false;

    keyset = (KeySet *)AllocateSafe(RG_SIZE(KeySet));
    mode = rk_DiskMode::Full;

    // Derive encryption keys
    crypto_kdf_blake2b_derive_from_key(keyset->ckey, RG_SIZE(keyset->ckey), (int)MasterDerivation::ConfigKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->dkey, RG_SIZE(keyset->dkey), (int)MasterDerivation::DataKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->lkey, RG_SIZE(keyset->lkey), (int)MasterDerivation::LogKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->ukey, RG_SIZE(keyset->ukey), (int)MasterDerivation::UserKey, DerivationContext, mkey.ptr);
    SeedSigningPair(keyset->ckey, keyset->akey);
    crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
    crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
    SeedSigningPair(keyset->ukey, keyset->vkey);

    user = nullptr;

    // Get cache ID
    if (!ReadConfig("rekkord", id))
        return false;

    err_guard.Disable();
    return true;
}

void rk_Disk::Lock()
{
    mode = rk_DiskMode::Secure;
    user = nullptr;

    ZeroSafe(id, RG_SIZE(id));
    ReleaseSafe(keyset, RG_SIZE(*keyset));
    keyset = nullptr;
    str_alloc.ReleaseAll();

    cache_db.Close();
}

static bool CheckUserName(Span<const char> username)
{
    const auto test_char = [](char c) { return (c >= 'a' && c <= 'z') || IsAsciiDigit(c) || c == '_' || c == '.' || c == '-'; };

    if (!username.len) {
        LogError("Username cannot be empty");
        return false;
    }
    if (username.len > 32) {
        LogError("Username cannot be have more than 32 characters");
        return false;
    }
    if (!std::all_of(username.begin(), username.end(), test_char)) {
        LogError("Username must only contain lowercase alphanumeric, '_', '.' or '-' characters");
        return false;
    }

    return true;
}

static bool IsUserName(Span<const char> username)
{
    PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
    RG_DEFER_N(log_guard) { PopLogFilter(); };

    return CheckUserName(username);
}

void rk_Disk::MakeSalt(rk_SaltKind kind, Span<uint8_t> out_buf) const
{
    RG_ASSERT(mode != rk_DiskMode::Secure);
    RG_ASSERT(out_buf.len >= 8);
    RG_ASSERT(out_buf.len <= 32);

    RG_ASSERT(strlen(DerivationContext) == 8);
    uint64_t subkey = (uint64_t)kind;

    crypto_kdf_blake2b_derive_from_key(out_buf.ptr, out_buf.len, subkey, DerivationContext, keyset->wkey);
}

bool rk_Disk::ChangeID()
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Full);

    uint8_t new_id[32];
    randombytes_buf(new_id, RG_SIZE(new_id));

    if (!WriteConfig("rekkord", new_id, true))
        return false;

    MemCpy(id, new_id, RG_SIZE(id));
    cache_db.Close();

    return true;
}

sq_Database *rk_Disk::OpenCache(bool build)
{
    RG_ASSERT(mode != rk_DiskMode::Secure);

    cache_db.Close();

    uint8_t cache_id[32] = {};
    {
        // Combine repository URL and ID to create a secure ID

        static_assert(RG_SIZE(cache_id) == crypto_hash_sha256_BYTES);

        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        crypto_hash_sha256_update(&state, id, RG_SIZE(id));
        crypto_hash_sha256_update(&state, (const uint8_t *)url, strlen(url));

        crypto_hash_sha256_final(&state, cache_id);
    }

    const char *cache_dir = GetUserCachePath("rekkord", &str_alloc);
    if (!cache_dir) {
        LogError("Cannot find user cache path");
        return nullptr;
    }
    if (!MakeDirectory(cache_dir, false))
        return nullptr;

    const char *cache_filename = Fmt(&str_alloc, "%1%/%2.db", cache_dir, FmtSpan(cache_id, FmtType::SmallHex, "").Pad0(-2)).ptr;
    LogDebug("Cache file: %1", cache_filename);

    if (!cache_db.Open(cache_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return nullptr;
    if (!cache_db.SetWAL(true))
        return nullptr;

    int version;
    if (!cache_db.GetUserVersion(&version))
        return nullptr;

    if (version > CacheVersion) {
        LogError("Cache schema is too recent (%1, expected %2)", version, CacheVersion);
        return nullptr;
    } else if (version < CacheVersion) {
        bool success = cache_db.Transaction([&]() {
            switch (version) {
                case 0: {
                    bool success = cache_db.RunMany(R"(
                        CREATE TABLE objects (
                            key TEXT NOT NULL
                        );
                        CREATE UNIQUE INDEX objects_k ON objects (key);
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 1: {
                    bool success = cache_db.RunMany(R"(
                        CREATE TABLE stats (
                            path TEXT NOT NULL,
                            mtime INTEGER NOT NULL,
                            mode INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            id BLOB NOT NULL
                        );
                        CREATE UNIQUE INDEX stats_p ON stats (path);
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 2: {
                    bool success = cache_db.RunMany(R"(
                        ALTER TABLE stats RENAME COLUMN id TO hash;
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 3: {
                    bool success = cache_db.RunMany(R"(
                        DROP TABLE stats;

                        CREATE TABLE stats (
                            path TEXT NOT NULL,
                            mtime INTEGER NOT NULL,
                            btime INEGER NOT NULL,
                            mode INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            hash BLOB NOT NULL
                        );
                        CREATE UNIQUE INDEX stats_p ON stats (path);
                    )");
                    if (!success)
                        return false;
                } // [[fallthrough]];

                static_assert(CacheVersion == 4);
            }

            if (build && !version && !RebuildCache())
                return false;
            if (!cache_db.SetUserVersion(CacheVersion))
                return false;

            return true;
        });

        if (!success) {
            cache_db.Close();
            return nullptr;
        }
    }

    RG_ASSERT(cache_db.IsValid());
    return &cache_db;
}

bool rk_Disk::RebuildCache()
{
    if (!cache_db.IsValid()) {
        LogError("Cache is not open");
        return false;
    }

    LogInfo("Rebuilding local cache...");

    BlockAllocator temp_alloc;

    bool success = cache_db.Transaction([&]() {
        if (!cache_db.Run("DELETE FROM objects"))
            return false;
        if (!cache_db.Run("DELETE FROM stats"))
            return false;

        bool success = ListRaw(nullptr, [&](const char *path) {
            if (!cache_db.Run(R"(INSERT INTO objects (key) VALUES (?1)
                                 ON CONFLICT (key) DO NOTHING)", path))
                return false;

            return true;
        });
        return success;
    });
    if (!success)
        return false;

    return true;
}

bool rk_Disk::InitUser(const char *username, rk_UserRole role, const char *pwd, bool force)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Full);
    RG_ASSERT(pwd);

    BlockAllocator temp_alloc;

    if (!CheckUserName(username))
        return false;

    const char *filename = Fmt(&temp_alloc, "keys/%1", username).ptr;
    bool exists = false;

    switch (TestRaw(filename)) {
        case StatResult::Success: { exists = true; } break;
        case StatResult::MissingPath: {} break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    if (exists) {
        if (force) {
            LogWarning("Overwriting existing user '%1'", username);
        } else {
            LogError("User '%1' already exists", username);
            return false;
        }
    }

    DeleteRaw(filename);

    if (!WriteKeys(filename, pwd, role, *keyset))
        return false;

    return true;
}

bool rk_Disk::DeleteUser(const char *username)
{
    RG_ASSERT(url);

    BlockAllocator temp_alloc;

    if (!CheckUserName(username))
        return false;

    const char *filename = Fmt(&temp_alloc, "keys/%1", username).ptr;

    switch (TestRaw(filename)) {
        case StatResult::Success: {} break;
        case StatResult::MissingPath: {
            LogError("User '%1' does not exist", username);
            return false;
        } break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    if (!DeleteRaw(filename))
        return false;

    return true;
}

static bool DecodeRole(int value, rk_UserRole *out_role)
{
    switch (value) {
        case (int)rk_UserRole::ReadWrite:
        case (int)rk_UserRole::WriteOnly: break;

        default: {
            LogError("Invalid user role %1", value);
            return false;
        } break;
    }

    *out_role = (rk_UserRole)value;
    return true;
}

bool rk_Disk::ListUsers(Allocator *alloc, bool verify, HeapArray<rk_UserInfo> *out_users)
{
    Size prev_len = out_users->len;
    RG_DEFER_N(out_guard) { out_users->RemoveFrom(prev_len); };

    bool success = ListRaw("keys", [&](const char *path) {
        rk_UserInfo user = {};

        Span<const char> remain = path;

        if (!StartsWith(remain, "keys/"))
            return true;

        Span<const char> username = remain.Take(5, remain.len - 5);

        if (!IsUserName(username))
            return true;

        user.username = DuplicateString(username, alloc).ptr;

        KeyData data = {};
        RG_DEFER { ZeroSafe(&data, RG_SIZE(data)); };

        // Read file data
        {
            Span<uint8_t> buf = MakeSpan((uint8_t *)&data, RG_SIZE(KeyData));
            Size len = ReadRaw(path, buf);

            if (len != buf.len) {
                if (len >= 0) {
                    LogError("Truncated keys in '%1'", path);
                }

                return true;
            }
        }

        if (verify && crypto_sign_verify_detached(data.sig, (const uint8_t *)&data, offsetof(KeyData, sig), keyset->vkey)) {
            LogError("Invalid signature for user '%1'", user.username);
            return true;
        }

        if (!DecodeRole(data.role, &user.role))
            return true;

        out_users->Append(user);
        return true;
    });
    if (!success)
        return false;

    std::sort(out_users->begin() + prev_len, out_users->end(),
              [](const rk_UserInfo &user1, const rk_UserInfo &user2) {
        return CmpStr(user1.username, user2.username) < 0;
    });

    out_guard.Disable();
    return true;
}

static inline FmtArg GetBlobPrefix(const rk_Hash &hash)
{
    uint16_t prefix = hash.hash[0];
    return FmtHex(prefix).Pad0(-2);
}

static int64_t PadMe(int64_t len)
{
    RG_ASSERT(len > 0);

    uint64_t e = 63 - CountLeadingZeros((uint64_t)len);
    uint64_t s = 63 - CountLeadingZeros(e) + 1;
    uint64_t mask = (1ull << (e - s)) - 1;

    uint64_t padded = (len + mask) & ~mask;
    uint64_t padding = padded - len;

    return (int64_t)padding;
}

bool rk_Disk::ReadBlob(const rk_Hash &hash, rk_BlobType *out_type, HeapArray<uint8_t> *out_blob)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Full);

    Size prev_len = out_blob->len;
    RG_DEFER_N(err_guard) { out_blob->RemoveFrom(prev_len); };

    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "blobs/%1/%2", GetBlobPrefix(hash), hash).len;

    HeapArray<uint8_t> raw;
    if (ReadRaw(path.data, &raw) < 0)
        return false;
    Span<const uint8_t> remain = raw;

    // Init blob decryption
    crypto_secretstream_xchacha20poly1305_state state;
    int version;
    rk_BlobType type;
    {
        BlobIntro intro;
        if (remain.len < RG_SIZE(intro)) {
            LogError("Truncated blob");
            return false;
        }
        MemCpy(&intro, remain.ptr, RG_SIZE(intro));

        if (intro.version != BlobVersion) {
            LogError("Unexpected blob version %1 (expected %2)", intro.version, BlobVersion);
            return false;
        }
        if (intro.type < 0 || intro.type >= RG_LEN(rk_BlobTypeNames)) {
            LogError("Invalid blob type 0x%1", FmtHex(intro.type));
            return false;
        }

        version = intro.version;
        type = (rk_BlobType)intro.type;

        uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
        if (crypto_box_seal_open(key, intro.ekey, RG_SIZE(intro.ekey), keyset->wkey, keyset->dkey) != 0) {
            LogError("Failed to unseal blob (wrong key?)");
            return false;
        }

        if (crypto_secretstream_xchacha20poly1305_init_pull(&state, intro.header, key) != 0) {
            LogError("Failed to initialize symmetric decryption (corrupt blob?)");
            return false;
        }

        remain.ptr += RG_SIZE(BlobIntro);
        remain.len -= RG_SIZE(BlobIntro);
    }

    if (version < 7) {
        LogError("Unsupported old blob format version %1", version);
        return false;
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
                LogError("Failed during symmetric decryption (corrupt blob?)");
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
            LogError("Truncated blob");
            return false;
        }
    }

    *out_type = type;
    err_guard.Disable();
    return true;
}

Size rk_Disk::WriteBlob(const rk_Hash &hash, rk_BlobType type, Span<const uint8_t> blob)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::WriteOnly || mode == rk_DiskMode::Full);

    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "blobs/%1/%2", GetBlobPrefix(hash), hash).len;

    switch (TestFast(path.data)) {
        case StatResult::Success: return 0;
        case StatResult::MissingPath: {} break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return -1;
    }

    Size written = WriteRaw(path.data, [&](FunctionRef<bool(Span<const uint8_t>)> func) {
        // Write blob intro
        crypto_secretstream_xchacha20poly1305_state state;
        {
            BlobIntro intro = {};

            intro.version = BlobVersion;
            intro.type = (int8_t)type;

            uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
            crypto_secretstream_xchacha20poly1305_keygen(key);
            if (crypto_secretstream_xchacha20poly1305_init_push(&state, intro.header, key) != 0) {
                LogError("Failed to initialize symmetric encryption");
                return false;
            }
            if (crypto_box_seal(intro.ekey, key, RG_SIZE(key), keyset->wkey) != 0) {
                LogError("Failed to seal symmetric key");
                return false;
            }

            Span<const uint8_t> buf = MakeSpan((const uint8_t *)&intro, RG_SIZE(intro));

            if (!func(buf))
                return false;
        }

        // Initialize compression
        EncodeLZ4 lz4;
        if (!lz4.Start(compression_level))
            return false;

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
                    return false;

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

                        if (!func(final))
                            return (Size)-1;
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

                        Size pad = (Size)std::min(padding, (int64_t)RG_SIZE(expand.data) - buf.len);

                        MemCpy(expand.data, buf.ptr, buf.len);
                        MemSet(expand.data + buf.len, 0, pad);
                        expand.len = buf.len + pad;

                        padding -= pad;

                        uint8_t cypher[BlobSplit + crypto_secretstream_xchacha20poly1305_ABYTES];
                        unsigned char tag = !padding ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
                        unsigned long long cypher_len;
                        crypto_secretstream_xchacha20poly1305_push(&state, cypher, &cypher_len, expand.data, expand.len, nullptr, 0, tag);

                        Span<const uint8_t> final = MakeSpan(cypher, (Size)cypher_len);

                        if (!func(final))
                            return (Size)-1;
                    }

                    // Finalize padding
                    while (padding) {
                        static const uint8_t padder[BlobSplit] = {};

                        Size pad = (Size)std::min(padding, (int64_t)RG_SIZE(padder));

                        padding -= pad;

                        uint8_t cypher[BlobSplit + crypto_secretstream_xchacha20poly1305_ABYTES];
                        unsigned char tag = !padding ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
                        unsigned long long cypher_len;
                        crypto_secretstream_xchacha20poly1305_push(&state, cypher, &cypher_len, padder, pad, nullptr, 0, tag);

                        Span<const uint8_t> final = MakeSpan(cypher, (Size)cypher_len);

                        if (!func(final))
                            return (Size)-1;
                    }

                    return processed;
                });
                if (!success)
                    return false;
            } while (!complete);
        }

        return true;
    });

    return written;
}

[[maybe_unused]] static Size ComputeMaxTagPayload()
{
    const Size MaxNameLen = 255;

    Size extra = RG_SIZE(TagIntro) + crypto_box_SEALBYTES;
    Size available = MaxNameLen / 4 * 3 - extra;

    return available;
}

Size rk_Disk::WriteTag(const rk_Hash &hash, Span<const uint8_t> payload)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::WriteOnly || mode == rk_DiskMode::Full);
    RG_ASSERT(payload.len <= ComputeMaxTagPayload());

    TagIntro intro = {};

    intro.version = TagVersion;
    intro.hash = hash;

    HeapArray<uint8_t> src;
    src.Append(MakeSpan((const uint8_t *)&intro, RG_SIZE(intro)));
    src.Append(payload);

    // Reuse for performance
    HeapArray<uint8_t> cypher;

    for (int i = 0; i < 100; i++) {
        Size cypher_len = crypto_box_SEALBYTES + src.len;

        cypher.RemoveFrom(0);
        cypher.Reserve(cypher_len);
        if (crypto_box_seal(cypher.end(), src.ptr, src.len, keyset->tkey) != 0) {
            LogError("Failed to seal tag payload");
            return -1;
        }
        cypher.len = cypher_len;

        LocalArray<char, 2048> path;
        path.len = Fmt(path.data, "tags/").len;

        RG_ASSERT(sodium_base64_ENCODED_LEN(cypher.len, sodium_base64_VARIANT_URLSAFE_NO_PADDING) < path.Available());
        sodium_bin2base64(path.end(), path.Available(), cypher.ptr, cypher.len, sodium_base64_VARIANT_URLSAFE_NO_PADDING);

        Size written = WriteDirect(path.data, cypher, false);

        if (written > 0)
            return written;
        if (written < 0)
            return -1;
    }

    // We really really should never reach this...
    LogError("Failed to create tag for '%1'", hash);
    return -1;
}

bool rk_Disk::ListTags(Allocator *alloc, HeapArray<rk_TagInfo> *out_tags)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Full);

    Size start_len = out_tags->len;
    RG_DEFER_N(out_guard) { out_tags->RemoveFrom(start_len); };

    HeapArray<const char *> filenames;
    {
        bool success = ListRaw("tags", [&](const char *path) {
            const char *filename = DuplicateString(path, alloc).ptr;
            filenames.Append(filename);

            return true;
        });
        if (!success)
            return false;
    }

    HeapArray<bool> ready;
    out_tags->AppendDefault(filenames.len);
    ready.AppendDefault(filenames.len);

    Async async(GetAsync());

    // List snapshots
    for (Size i = 0; i < filenames.len; i++) {
        const char *filename = filenames[i];

        async.Run([=, &ready, this]() {
            rk_TagInfo tag = {};

            Span<const char> basename = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);

            HeapArray<uint8_t> cypher;
            {
                cypher.Reserve(basename.len);

                size_t len = 0;
                if (sodium_base642bin(cypher.ptr, cypher.capacity, basename.ptr, basename.len, nullptr,
                                      &len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING) < 0) {
                    LogError("Invalid base64 string in tag");
                    return true;
                }
                cypher.len = (Size)len;

                if (cypher.len < (Size)crypto_box_SEALBYTES + RG_SIZE(TagIntro)) {
                    LogError("Truncated cypher in tag");
                    return true;
                }
            }

            Size data_len = cypher.len - crypto_box_SEALBYTES;
            Span<uint8_t> data = AllocateSpan<uint8_t>(alloc, data_len);

            if (crypto_box_seal_open(data.ptr, cypher.ptr, cypher.len, keyset->tkey, keyset->lkey) != 0) {
                LogError("Failed to unseal tag data from '%1'", basename);
                return true;
            }

            TagIntro intro = {};
            MemCpy(&intro, data.ptr, RG_SIZE(intro));

            if (intro.version != TagVersion) {
                LogError("Unexpected tag version %1 (expected %2) in '%3'", intro.version, TagVersion, basename);
                return true;
            }

            tag.path = basename.ptr;
            tag.hash = intro.hash;
            tag.payload = data.Take(RG_SIZE(intro), data.len - RG_SIZE(intro));

            out_tags->ptr[start_len + i] = tag;
            ready[i] = true;

            return true;
        });
    }

    if (!async.Sync())
        return false;

    Size j = 0;
    for (Size i = 0; i < filenames.len; i++) {
        out_tags->ptr[start_len + j] = out_tags->ptr[start_len + i];
        j += ready[i];
    }
    out_tags->len = start_len + j;

    out_guard.Disable();
    return true;
}

bool rk_Disk::InitDefault(Span<const uint8_t> mkey, const char *full_pwd, const char *write_pwd)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);
    RG_ASSERT(!keyset);

    if (mkey.len != rk_MasterKeySize) {
        LogError("Malformed master key");
        return false;
    }

    // Drop created files if anything fails
    HeapArray<const char *> names;
    RG_DEFER_N(err_guard) {
        Lock();

        for (const char *name: names) {
            DeleteRaw(name);
        }
    };

    switch (TestRaw("rekkord")) {
        case StatResult::Success: {
            LogError("Repository '%1' looks already initialized", url);
            return false;
        } break;
        case StatResult::MissingPath: {} break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    keyset = (KeySet *)AllocateSafe(RG_SIZE(KeySet));
    mode = rk_DiskMode::Full;

    // Derive encryption keys
    crypto_kdf_blake2b_derive_from_key(keyset->ckey, RG_SIZE(keyset->ckey), (int)MasterDerivation::ConfigKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->dkey, RG_SIZE(keyset->dkey), (int)MasterDerivation::DataKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->lkey, RG_SIZE(keyset->lkey), (int)MasterDerivation::LogKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->ukey, RG_SIZE(keyset->ukey), (int)MasterDerivation::UserKey, DerivationContext, mkey.ptr);
    SeedSigningPair(keyset->ckey, keyset->akey);
    crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
    crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
    SeedSigningPair(keyset->ukey, keyset->vkey);

    // Generate random ID for local cache
    randombytes_buf(id, RG_SIZE(id));
    if (!WriteConfig("rekkord", id, false))
        return false;
    names.Append("rekkord");

    // Write key files
    if (full_pwd) {
        if (!WriteKeys("keys/full", full_pwd, rk_UserRole::ReadWrite, *keyset))
            return false;
        names.Append("keys/full");
    }
    if (write_pwd) {
        if (!WriteKeys("keys/write", write_pwd, rk_UserRole::WriteOnly, *keyset))
            return false;
        names.Append("keys/write");
    }

    err_guard.Disable();
    return true;
}

bool rk_Disk::PutCache(const char *key)
{
    if (!cache_db.IsValid())
        return true;

    bool success = cache_db.Run(R"(INSERT INTO objects (key) VALUES (?1)
                                   ON CONFLICT DO NOTHING)", key);
    return success;
}

StatResult rk_Disk::TestFast(const char *path)
{
    if (!cache_db.IsValid())
        return TestRaw(path);

    bool should_exist;
    {
        sq_Statement stmt;
        if (!cache_db.Prepare("SELECT rowid FROM objects WHERE key = ?1", &stmt))
            return StatResult::OtherError;
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

        should_exist = stmt.Step();
    }

    // Probabilistic check
    if (GetRandomInt(0, 100) < 2) {
        bool really_exists = false;

        switch (TestRaw(path)) {
            case StatResult::Success: { really_exists = true; } break;
            case StatResult::MissingPath: { really_exists = false; } break;
            case StatResult::AccessDenied: return StatResult::AccessDenied;
            case StatResult::OtherError: return StatResult::OtherError;
        }

        if (really_exists && !should_exist) {
            std::unique_lock<std::mutex> lock(cache_mutex, std::try_to_lock);

            if (lock.owns_lock() && ++cache_misses >= 4) {
                RebuildCache();
                cache_misses = 0;
            }

            return really_exists ? StatResult::Success : StatResult::MissingPath;
        } else if (should_exist && !really_exists) {
            ClearCache();

            LogError("The local cache database was mismatched and could have resulted in missing data in the backup.");
            LogError("You must start over to fix this situation.");

            return StatResult::OtherError;
        }
    }

    return should_exist ? StatResult::Success : StatResult::MissingPath;
}

static bool DeriveFromPassword(const char *pwd, const uint8_t salt[16], uint8_t out_key[32])
{
    static_assert(crypto_pwhash_SALTBYTES == 16);

    if (crypto_pwhash(out_key, 32, pwd, strlen(pwd), salt, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_ARGON2ID13) != 0) {
        LogError("Failed to derive key from password (exhausted resource?)");
        return false;
    }

    return true;
}

bool rk_Disk::WriteKeys(const char *path, const char *pwd, rk_UserRole role, const KeySet &keys)
{
    const Size PayloadSize = RG_SIZE(KeyData::cypher) - 16;
    static_assert(RG_SIZE(keys) <= PayloadSize);

    KeyData data = {};
    uint8_t *payload = (uint8_t *)AllocateSafe(PayloadSize);
    RG_DEFER {
        ZeroSafe(&data, RG_SIZE(data));
        ReleaseSafe(payload, PayloadSize);
    };

    randombytes_buf(data.salt, RG_SIZE(data.salt));
    randombytes_buf(data.nonce, RG_SIZE(data.nonce));
    data.role = (int8_t)role;

    // Pick relevant subset of keys
    switch (role) {
        case rk_UserRole::ReadWrite: {
            MemCpy(payload + offsetof(KeySet, ckey), keys.ckey, RG_SIZE(keys.ckey));
            MemCpy(payload + offsetof(KeySet, dkey), keys.dkey, RG_SIZE(keys.dkey));
            MemCpy(payload + offsetof(KeySet, lkey), keys.lkey, RG_SIZE(keys.lkey));
            MemCpy(payload + offsetof(KeySet, ukey), keys.ukey, RG_SIZE(keys.ukey));
        } break;

        case rk_UserRole::WriteOnly: {
            MemCpy(payload + offsetof(KeySet, akey), keys.akey, RG_SIZE(keys.akey));
            MemCpy(payload + offsetof(KeySet, wkey), keys.wkey, RG_SIZE(keys.wkey));
            MemCpy(payload + offsetof(KeySet, tkey), keys.tkey, RG_SIZE(keys.tkey));
            MemCpy(payload + offsetof(KeySet, vkey), keys.vkey, RG_SIZE(keys.vkey));
        } break;
    }

    // Encrypt payload
    {
        uint8_t key[32];
        if (!DeriveFromPassword(pwd, data.salt, key))
            return false;
        crypto_secretbox_easy(data.cypher, payload, PayloadSize, data.nonce, key);
    }

    // Sign serialized keyset to detect tampering
    crypto_sign_ed25519_detached(data.sig, nullptr, (const uint8_t *)&data, offsetof(KeyData, sig), keyset->ukey);

    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&data, RG_SIZE(data));
    Size written = WriteDirect(path, buf, false);

    if (written < 0)
        return false;
    if (!written) {
        LogError("Key file '%1' already exists", path);
        return false;
    }

    return true;
}

bool rk_Disk::ReadKeys(const char *path, const char *pwd, rk_UserRole *out_role, KeySet *out_keys)
{
    const Size PayloadSize = RG_SIZE(KeyData::cypher) - 16;
    static_assert(RG_SIZE(*out_keys) <= PayloadSize);

    KeyData data = {};
    uint8_t *payload = (uint8_t *)AllocateSafe(PayloadSize);
    RG_DEFER {
        ZeroSafe(&data, RG_SIZE(data));
        ReleaseSafe(payload, PayloadSize);
    };

    // Read file data
    {
        Span<uint8_t> buf = MakeSpan((uint8_t *)&data, RG_SIZE(data));
        Size len = ReadRaw(path, buf);

        if (len != buf.len) {
            if (len >= 0) {
                LogError("Truncated keys in '%1'", path);
            }
            return false;
        }
    }

    // Decrypt payload
    {
        uint8_t key[32];
        if (!DeriveFromPassword(pwd, data.salt, key))
            return false;

        if (crypto_secretbox_open_easy(payload, data.cypher, RG_SIZE(data.cypher), data.nonce, key)) {
            LogError("Failed to open repository (wrong password?)");
            return false;
        }
    }

    if (!DecodeRole(data.role, out_role))
        return false;
    MemCpy(out_keys, payload, RG_SIZE(*out_keys));

    return true;
}

bool rk_Disk::WriteConfig(const char *path, Span<const uint8_t> data, bool overwrite)
{
    RG_ASSERT(data.len + crypto_sign_ed25519_BYTES <= RG_SIZE(ConfigData::cypher));

    ConfigData config = {};

    config.version = ConfigVersion;

    // Sign config to detect tampering
    crypto_sign_ed25519(config.cypher, nullptr, data.ptr, (size_t)data.len, keyset->ckey);

    Size len = offsetof(ConfigData, cypher) + crypto_sign_ed25519_BYTES + data.len;
    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&config, len);
    Size written = WriteDirect(path, buf, overwrite);

    if (written < 0)
        return false;
    if (!written) {
        LogError("Config file '%1' already exists", path);
        return false;
    }

    return true;
}

bool rk_Disk::ReadConfig(const char *path, Span<uint8_t> out_buf)
{
    ConfigData config = {};

    Span<uint8_t> buf = MakeSpan((uint8_t *)&config, RG_SIZE(config));
    Size len = ReadRaw(path, buf);

    if (len < 0)
        return false;
    if (len < (Size)offsetof(ConfigData, cypher)) {
        LogError("Malformed config file '%1'", path);
        return false;
    }
    if (config.version != ConfigVersion) {
        LogError("Unexpected config version %1 (expected %2)", config.version, ConfigVersion);
        return false;
    }

    len -= offsetof(ConfigData, cypher);
    len = std::min(len, out_buf.len + (Size)crypto_sign_ed25519_BYTES);

    if (crypto_sign_ed25519_open(out_buf.ptr, nullptr, config.cypher, len, keyset->akey)) {
        LogError("Failed to decrypt config '%1'", path);
        return false;
    }

    return true;
}

Size rk_Disk::WriteDirect(const char *path, Span<const uint8_t> buf, bool overwrite)
{
    if (!overwrite) {
        switch (TestRaw(path)) {
            case StatResult::Success: return 0;
            case StatResult::MissingPath: {} break;
            case StatResult::AccessDenied:
            case StatResult::OtherError: return -1;
        }
    }

    Size written = WriteRaw(path, [&](FunctionRef<bool(Span<const uint8_t>)> func) { return func(buf); });
    return written;
}

bool rk_Disk::CheckRepository()
{
    switch (TestRaw("rekkord")) {
        case StatResult::Success: return true;
        case StatResult::MissingPath: {
            LogError("Repository '%1' is not initialized or not valid", url);
            return false;
        } break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    RG_UNREACHABLE();
}

void rk_Disk::ClearCache()
{
    if (!cache_db.IsValid())
        return;

    cache_db.Transaction([&]() {
        if (!cache_db.Run("DELETE FROM objects"))
            return false;
        if (!cache_db.Run("DELETE FROM stats"))
            return false;

        return true;
    });
}

std::unique_ptr<rk_Disk> rk_Open(const rk_Config &config, bool authenticate)
{
    if (!config.Validate(authenticate))
        return nullptr;

    const char *username = authenticate ? config.username : nullptr;
    const char *password = authenticate ? config.password : nullptr;

    rk_OpenSettings settings = {
        .threads = config.threads,
        .compression_level = config.compression_level
    };

    switch (config.type) {
        case rk_DiskType::Local: return rk_OpenLocalDisk(config.url, username, password, settings);
        case rk_DiskType::SFTP: return rk_OpenSftpDisk(config.ssh, username, password, settings);
        case rk_DiskType::S3: return rk_OpenS3Disk(config.s3, username, password, settings);
    }

    RG_UNREACHABLE();
}

}
