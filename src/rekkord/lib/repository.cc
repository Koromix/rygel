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
#include "lz4.hh"
#include "repository.hh"
#include "priv_repository.hh"
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

static void SeedSigningPair(const uint8_t sk[32], uint8_t pk[32])
{
    uint8_t hash[64];
    RG_DEFER { ZeroSafe(hash, RG_SIZE(hash)); };

    crypto_hash_sha512(hash, sk, 32);
    crypto_scalarmult_ed25519_base(pk, hash);
}

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
    RG_ASSERT(!keyset);

    BlockAllocator temp_alloc;

    if (mkey.len != rk_MasterKeySize) {
        LogError("Malformed master key");
        return false;
    }

    HeapArray<const char *> directories;
    HeapArray<const char *> files;

    RG_DEFER_N(err_guard) {
        Lock();

        for (const char *filename: files) {
            disk->DeleteFile(filename);
        }
        for (Size i = directories.len - 1; i >= 0; i--) {
            const char *dirname = directories[i];
            disk->DeleteDirectory(dirname);
        }
    };

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

    keyset = (rk_KeySet *)AllocateSafe(RG_SIZE(rk_KeySet));
    modes = UINT_MAX;
    role = "Master";

    // Derive encryption keys
    crypto_kdf_blake2b_derive_from_key(keyset->ckey, RG_SIZE(keyset->ckey), (int)MasterDerivation::ConfigKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->dkey, RG_SIZE(keyset->dkey), (int)MasterDerivation::DataKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->lkey, RG_SIZE(keyset->lkey), (int)MasterDerivation::LogKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->nkey, RG_SIZE(keyset->nkey), (int)MasterDerivation::NeutralKey, DerivationContext, mkey.ptr);
    SeedSigningPair(keyset->ckey, keyset->akey);
    crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
    crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
    MemCpy(keyset->skey, keyset->nkey, RG_SIZE(keyset->skey));
    SeedSigningPair(keyset->skey, keyset->pkey);

    // Generate unique repository IDs
    {
        randombytes_buf(ids.rid, RG_SIZE(ids.rid));
        randombytes_buf(ids.cid, RG_SIZE(ids.cid));

        uint8_t buf[RG_SIZE(ids)];
        MemCpy(buf, &ids, RG_SIZE(ids));

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

bool rk_Repository::Authenticate(const char *username, const char *pwd)
{
    RG_ASSERT(!keyset);

    RG_DEFER_N(err_guard) { Lock(); };

    const char *filename = Fmt(&str_alloc, "keys/%1", username).ptr;

    if (!CheckRepository())
        return false;

    // Does user exist?
    switch (disk->TestFile(filename)) {
        case StatResult::Success: {} break;
        case StatResult::MissingPath: {
            LogError("User '%1' does not exist", username);
            return false;
        } break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    keyset = (rk_KeySet *)AllocateSafe(RG_SIZE(rk_KeySet));

    // Open disk and determine mode
    {
        rk_UserRole role;

        if (!ReadKeys(filename, pwd, &role, keyset))
            return false;

        switch (role) {
            case rk_UserRole::Admin: { modes = (int)rk_AccessMode::Config |
                                               (int)rk_AccessMode::Read |
                                               (int)rk_AccessMode::Write |
                                               (int)rk_AccessMode::Log; } break;
            case rk_UserRole::WriteOnly: { modes = (int)rk_AccessMode::Write; } break;
            case rk_UserRole::ReadWrite: { modes = (int)rk_AccessMode::Read |
                                                   (int)rk_AccessMode::Write |
                                                   (int)rk_AccessMode::Log; } break;
            case rk_UserRole::LogOnly: { modes = (int)rk_AccessMode::Log; } break;
        }

        this->role = rk_UserRoleNames[(int)role];
        this->user = DuplicateString(username, &str_alloc).ptr;
    }

    // Read unique identifiers
    {
        uint8_t buf[RG_SIZE(ids)];
        if (!ReadConfig("rekkord", buf))
            return false;
        MemCpy(&ids, buf, RG_SIZE(ids));
    }

    err_guard.Disable();
    return true;
}

bool rk_Repository::Authenticate(Span<const uint8_t> mkey)
{
    RG_ASSERT(!keyset);

    RG_DEFER_N(err_guard) { Lock(); };

    if (mkey.len != rk_MasterKeySize) {
        LogError("Malformed master key");
        return false;
    }

    if (!CheckRepository())
        return false;

    keyset = (rk_KeySet *)AllocateSafe(RG_SIZE(rk_KeySet));
    modes = UINT_MAX;
    role = "Master";

    // Derive encryption keys
    crypto_kdf_blake2b_derive_from_key(keyset->ckey, RG_SIZE(keyset->ckey), (int)MasterDerivation::ConfigKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->dkey, RG_SIZE(keyset->dkey), (int)MasterDerivation::DataKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->lkey, RG_SIZE(keyset->lkey), (int)MasterDerivation::LogKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->nkey, RG_SIZE(keyset->nkey), (int)MasterDerivation::NeutralKey, DerivationContext, mkey.ptr);
    SeedSigningPair(keyset->ckey, keyset->akey);
    crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
    crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
    MemCpy(keyset->skey, keyset->nkey, RG_SIZE(keyset->skey));
    SeedSigningPair(keyset->skey, keyset->pkey);

    user = nullptr;

    // Read unique identifiers
    {
        uint8_t buf[RG_SIZE(ids)];
        if (!ReadConfig("rekkord", buf))
            return false;
        MemCpy(&ids, buf, RG_SIZE(ids));
    }

    err_guard.Disable();
    return true;
}

void rk_Repository::Lock()
{
    modes = 0;
    user = nullptr;
    role = "Secure";

    ZeroSafe(&ids, RG_SIZE(ids));
    ReleaseSafe(keyset, RG_SIZE(*keyset));
    keyset = nullptr;
    str_alloc.ReleaseAll();
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

void rk_Repository::MakeID(Span<uint8_t> out_id) const
{
    RG_ASSERT(out_id.len >= 16 && out_id.len <= 64);

    Span<const char> url = disk->GetURL();

    uint8_t sha512[64];
    {
        crypto_hash_sha512_state state;
        crypto_hash_sha512_init(&state);

        crypto_hash_sha512_update(&state, ids.rid, RG_SIZE(ids.rid));
        crypto_hash_sha512_update(&state, (const uint8_t *)url.ptr, (size_t)url.len);
        crypto_hash_sha512_update(&state, (const uint8_t *)&out_id.len, RG_SIZE(out_id.len));

        crypto_hash_sha512_final(&state, sha512);
    }

    MemCpy(out_id.ptr, sha512, out_id.len);
}

void rk_Repository::MakeSalt(rk_SaltKind kind, Span<uint8_t> out_buf) const
{
    RG_ASSERT(HasMode(rk_AccessMode::Write));
    RG_ASSERT(out_buf.len >= 8);
    RG_ASSERT(out_buf.len <= 32);

    RG_ASSERT(strlen(DerivationContext) == 8);
    uint64_t subkey = (uint64_t)kind;

    uint8_t buf[32] = {};
    crypto_kdf_blake2b_derive_from_key(buf, RG_SIZE(buf), subkey, DerivationContext, keyset->wkey);

    MemCpy(out_buf.ptr, buf, out_buf.len);
}

bool rk_Repository::ChangeCID()
{
    RG_ASSERT(HasMode(rk_AccessMode::Config));

    IdSet new_ids = ids;
    randombytes_buf(new_ids.cid, RG_SIZE(new_ids.cid));

    // Write new IDs
    {
        uint8_t buf[RG_SIZE(new_ids)];
        MemCpy(buf, &new_ids, RG_SIZE(new_ids));

        if (!WriteConfig("rekkord", buf, true))
            return false;
    }

    MemCpy(ids.cid, new_ids.cid, RG_SIZE(new_ids.cid));

    return true;
}

bool rk_Repository::InitUser(const char *username, rk_UserRole role, const char *pwd)
{
    RG_ASSERT(HasMode(rk_AccessMode::Config));
    RG_ASSERT(pwd);

    BlockAllocator temp_alloc;

    if (!CheckUserName(username))
        return false;

    const char *filename = Fmt(&temp_alloc, "keys/%1", username).ptr;
    bool exists = false;

    switch (disk->TestFile(filename)) {
        case StatResult::Success: { exists = true; } break;
        case StatResult::MissingPath: {} break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    if (exists) {
        LogError("User '%1' already exists", username);
        return false;
    }

    rk_KeySet keys = *keyset;
    randombytes_buf(keys.skey, RG_SIZE(keys.skey));

    if (!WriteKeys(filename, pwd, role, keys))
        return false;

    return true;
}

bool rk_Repository::DeleteUser(const char *username)
{
    BlockAllocator temp_alloc;

    if (!CheckUserName(username))
        return false;

    const char *filename = Fmt(&temp_alloc, "keys/%1", username).ptr;

    switch (disk->TestFile(filename)) {
        case StatResult::Success: {} break;
        case StatResult::MissingPath: {
            LogError("User '%1' does not exist", username);
            return false;
        } break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    if (!disk->DeleteFile(filename))
        return false;

    return true;
}

static bool DecodeRole(int value, rk_UserRole *out_role)
{
    if (value < 0 || value >= RG_LEN(rk_UserRoleNames)) {
        LogError("Invalid user role %1", value);
        return false;
    }

    *out_role = (rk_UserRole)value;
    return true;
}

bool rk_Repository::ListUsers(Allocator *alloc, bool verify, HeapArray<rk_UserInfo> *out_users)
{
    Size prev_len = out_users->len;
    RG_DEFER_N(out_guard) { out_users->RemoveFrom(prev_len); };

    bool success = disk->ListFiles("keys", [&](const char *path, int64_t) {
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
            Size len = disk->ReadFile(path, buf);

            if (len != buf.len) {
                if (len >= 0) {
                    LogError("Malformed keys in '%1'", path);
                }

                return true;
            }
        }

        if (verify && crypto_sign_ed25519_verify_detached(data.sig, (const uint8_t *)&data, offsetof(KeyData, sig), keyset->akey)) {
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
    uint16_t prefix = hash.raw[0];
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

bool rk_Repository::ReadBlob(const rk_ObjectID &oid, int *out_type, HeapArray<uint8_t> *out_blob, int64_t *out_size)
{
    RG_ASSERT(HasMode(rk_AccessMode::Read));

    char path[256];
    Fmt(path, "blobs/%1/%2/%3", rk_BlobCatalogNames[(int)oid.catalog], GetBlobPrefix(oid.hash), oid.hash);

    Size prev_len = out_blob->len;
    RG_DEFER_N(err_guard) { out_blob->RemoveFrom(prev_len); };

    HeapArray<uint8_t> raw;
    if (disk->ReadFile(path, &raw) < 0)
        return false;
    Span<const uint8_t> remain = raw;

    // Init blob decryption
    crypto_secretstream_xchacha20poly1305_state state;
    int8_t type;
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
        if (intro.type < 0) {
            LogError("Invalid blob type %1", intro.type);
            return false;
        }

        type = intro.type;

        uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
        if (crypto_box_seal_open(key, intro.ekey, RG_SIZE(intro.ekey), keyset->wkey, keyset->dkey) != 0) {
            LogError("Failed to unseal blob '%1'", path);
            return false;
        }

        if (crypto_secretstream_xchacha20poly1305_init_pull(&state, intro.header, key) != 0) {
            LogError("Failed to initialize symmetric decryption of '%1'", path);
            return false;
        }

        remain.ptr += RG_SIZE(BlobIntro);
        remain.len -= RG_SIZE(BlobIntro);
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
            LogError("Truncated blob '%1'", path);
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
    RG_ASSERT(HasMode(rk_AccessMode::Write));
    RG_ASSERT(type >= 0 && type < INT8_MAX);

    char path[256];
    Fmt(path, "blobs/%1/%2/%3", rk_BlobCatalogNames[(int)oid.catalog], GetBlobPrefix(oid.hash), oid.hash);

    HeapArray<uint8_t> raw;
    crypto_secretstream_xchacha20poly1305_state state;

    // Write blob intro
    {
        BlobIntro intro = {};

        intro.version = BlobVersion;
        intro.type = type;

        uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
        crypto_secretstream_xchacha20poly1305_keygen(key);
        if (crypto_secretstream_xchacha20poly1305_init_push(&state, intro.header, key) != 0) {
            LogError("Failed to initialize symmetric encryption");
            return rk_WriteResult::OtherError;
        }
        if (crypto_box_seal(intro.ekey, key, RG_SIZE(key), keyset->wkey) != 0) {
            LogError("Failed to seal symmetric key");
            return rk_WriteResult::OtherError;
        }

        Span<const uint8_t> buf = MakeSpan((const uint8_t *)&intro, RG_SIZE(intro));
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
                    raw.Append(final);
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
                    raw.Append(final);
                }

                return processed;
            });
            if (!success)
                return rk_WriteResult::OtherError;
        } while (!complete);
    }

    rk_WriteSettings settings = { .conditional = HasConditionalWrites(), .retain = retain };
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
    const Size MaxFragmentSize = 160;

    RG_ASSERT(HasMode(rk_AccessMode::Write));

    BlockAllocator temp_alloc;

    Size fragments = (payload.len + MaxFragmentSize - 1) / MaxFragmentSize;

    // Sanity check
    if (fragments >= 5) {
        LogError("Excessive tag data size");
        return false;
    }

    char prefix[RG_SIZE(TagIntro::prefix) + 1];
    uint8_t key[RG_SIZE(TagIntro::key)];
    Fmt(prefix, "%1", FmtRandom(RG_SIZE(prefix) - 1));
    randombytes_buf(key, RG_SIZE(key));

    HeapArray<const char *> paths;

    // Create main tag path
    {
        TagIntro intro = {};

        intro.version = TagVersion;
        intro.oid = oid;
        MemCpy(intro.prefix, prefix, RG_SIZE(intro.prefix));
        MemCpy(intro.key, key, RG_SIZE(key));
        intro.count = (int8_t)fragments;

        uint8_t cypher[RG_SIZE(intro) + crypto_box_SEALBYTES + crypto_sign_BYTES];

        if (crypto_box_seal(cypher, (const uint8_t *)&intro, RG_SIZE(intro), keyset->tkey) != 0) {
            LogError("Failed to seal tag payload");
            return false;
        }

        // Sign it to avoid tampering by other users
        crypto_sign_ed25519_detached(std::end(cypher) - crypto_sign_BYTES, nullptr, cypher,
                                     RG_SIZE(cypher) - crypto_sign_BYTES, keyset->skey);

        HeapArray<char> buf(&temp_alloc);
        buf.Reserve(512);

        Fmt(&buf, "tags/M/");
        sodium_bin2base64(buf.end(), buf.Available(), cypher, RG_SIZE(cypher), sodium_base64_VARIANT_URLSAFE_NO_PADDING);
        buf.len += sodium_base64_encoded_len(RG_SIZE(cypher), sodium_base64_VARIANT_URLSAFE_NO_PADDING) - 1;

        Span<const char> path = buf.Leak();
        paths.Append(path.ptr);
    }

    // Encrypt payload
    for (uint8_t i = 0; payload.len; i++) {
        Size frag_len = std::min(payload.len, MaxFragmentSize);

        uint8_t cypher[MaxFragmentSize + crypto_secretbox_MACBYTES];
        uint8_t nonce[24] = {};

        nonce[23] = i;

        if (crypto_secretbox_easy(cypher, payload.ptr, frag_len, nonce, key) != 0) {
            LogError("Failed to encrypt tag payload");
            return false;
        }

        HeapArray<char> buf(&temp_alloc);
        buf.Reserve(512);

        Fmt(&buf, "tags/P/%1_%2_", prefix, FmtArg(i).Pad0(-2));
        sodium_bin2base64(buf.end(), buf.Available(), cypher, frag_len + crypto_secretbox_MACBYTES, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
        buf.len += sodium_base64_encoded_len(frag_len + crypto_secretbox_MACBYTES, sodium_base64_VARIANT_URLSAFE_NO_PADDING) - 1;

        Span<const char> path = buf.Leak();
        paths.Append(path.ptr);

        payload.ptr += frag_len;
        payload.len -= frag_len;
    }

    // Create tag files
    for (const char *path: paths) {
        rk_WriteSettings settings = { .conditional = HasConditionalWrites(), .retain = retain };
        rk_WriteResult ret = disk->WriteFile(path, {}, settings);

        if (ret != rk_WriteResult::Success)
            return false;
    }

    return true;
}

bool rk_Repository::ListTags(Allocator *alloc, HeapArray<rk_TagInfo> *out_tags)
{
    RG_ASSERT(HasMode(rk_AccessMode::Log));

    BlockAllocator temp_alloc;

    Size start_len = out_tags->len;
    RG_DEFER_N(err_guard) { out_tags->RemoveFrom(start_len); };

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

        LocalArray<uint8_t, 255> cypher;
        {
            size_t len = 0;
            sodium_base642bin(cypher.data, RG_SIZE(cypher.data), main.ptr, main.len, nullptr,
                              &len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING);

            if (len != RG_SIZE(TagIntro) + crypto_box_SEALBYTES + crypto_sign_BYTES) {
                LogError("Invalid tag cypher length");
                continue;
            }

            cypher.len = (Size)len;
        }

        TagIntro intro;
        if (crypto_box_seal_open((uint8_t *)&intro, cypher.data, cypher.len - crypto_sign_BYTES, keyset->tkey, keyset->lkey) != 0) {
            LogError("Failed to unseal tag data from '%1'", main);
            continue;
        }
        if (intro.version != TagVersion) {
            LogError("Unexpected tag version %1 (expected %2) in '%3'", intro.version, TagVersion, main);
            continue;
        }

        tag.name = main.ptr;
        tag.prefix = DuplicateString(MakeSpan(intro.prefix, RG_SIZE(intro.prefix)), alloc).ptr;
        tag.oid = intro.oid;

        // Stash key and count in payload to make it available for next step
        tag.payload = AllocateSpan<uint8_t>(&temp_alloc, RG_SIZE(intro.key));
        MemCpy((uint8_t *)tag.payload.ptr, intro.key, RG_SIZE(intro.key));
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
                    sodium_base642bin(cypher.data, RG_SIZE(cypher.data), frag.ptr + 14, frag.len - 14, nullptr,
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

bool rk_Repository::WriteKeys(const char *path, const char *pwd, rk_UserRole role, const rk_KeySet &keys)
{
    RG_ASSERT(HasMode(rk_AccessMode::Config));

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

    // Write public user key
    SeedSigningPair(keys.skey, payload);

    // Pick relevant subset of keys
    {
        uint8_t *dest = payload + 32;

        switch (role) {
            case rk_UserRole::Admin: {
                MemCpy(dest + offsetof(rk_KeySet, ckey), keys.ckey, RG_SIZE(keys.ckey));
                MemCpy(dest + offsetof(rk_KeySet, dkey), keys.dkey, RG_SIZE(keys.dkey));
                MemCpy(dest + offsetof(rk_KeySet, lkey), keys.lkey, RG_SIZE(keys.lkey));
            } break;

            case rk_UserRole::WriteOnly: {
                MemCpy(dest + offsetof(rk_KeySet, akey), keys.akey, RG_SIZE(keys.akey));
                MemCpy(dest + offsetof(rk_KeySet, wkey), keys.wkey, RG_SIZE(keys.wkey));
                MemCpy(dest + offsetof(rk_KeySet, tkey), keys.tkey, RG_SIZE(keys.tkey));
            } break;

            case rk_UserRole::ReadWrite: {
                MemCpy(dest + offsetof(rk_KeySet, akey), keys.akey, RG_SIZE(keys.akey));
                MemCpy(dest + offsetof(rk_KeySet, dkey), keys.dkey, RG_SIZE(keys.dkey));
                MemCpy(dest + offsetof(rk_KeySet, lkey), keys.lkey, RG_SIZE(keys.lkey));
            } break;

            case rk_UserRole::LogOnly: {
                MemCpy(dest + offsetof(rk_KeySet, akey), keys.akey, RG_SIZE(keys.akey));
                MemCpy(dest + offsetof(rk_KeySet, lkey), keys.lkey, RG_SIZE(keys.lkey));
            } break;
        }

        MemCpy(dest + offsetof(rk_KeySet, skey), keys.skey, RG_SIZE(keys.skey));
    }

    // Encrypt payload
    {
        uint8_t *key = (uint8_t *)AllocateSafe(32);
        RG_DEFER { ReleaseSafe(key, 32); };

        if (!DeriveFromPassword(pwd, data.salt, key))
            return false;
        crypto_secretbox_easy(data.cypher, payload, PayloadSize, data.nonce, key);
    }

    // Sign serialized keyset to detect tampering
    crypto_sign_ed25519_detached(data.sig, nullptr, (const uint8_t *)&data, offsetof(KeyData, sig), keyset->ckey);

    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&data, RG_SIZE(data));

    rk_WriteSettings settings = { .conditional = HasConditionalWrites() };
    rk_WriteResult ret = disk->WriteFile(path, buf, settings);

    switch (ret) {
        case rk_WriteResult::Success: return true;
        case rk_WriteResult::AlreadyExists: {
            LogError("Key file '%1' already exists", path);
            return false;
        } break;
        case rk_WriteResult::OtherError: return false;
    }

    RG_UNREACHABLE();
}

bool rk_Repository::ReadKeys(const char *path, const char *pwd, rk_UserRole *out_role, rk_KeySet *out_keys)
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
        Size len = disk->ReadFile(path, buf);

        if (len != buf.len) {
            if (len == 889) {
                LogError("This looks like a user made with Rekkord ≤ 0.74, use %!..+rekkord migrate_user -K master.key username%!0");
            } else if (len >= 0) {
                LogError("Malformed keys in '%1'", path);
            }
            return false;
        }
    }

    // Decrypt payload
    {
        uint8_t *key = (uint8_t *)AllocateSafe(32);
        RG_DEFER { ReleaseSafe(key, 32); };

        if (!DeriveFromPassword(pwd, data.salt, key))
            return false;

        if (crypto_secretbox_open_easy(payload, data.cypher, RG_SIZE(data.cypher), data.nonce, key)) {
            LogError("Failed to open repository (wrong password?)");
            return false;
        }
    }

    rk_UserRole role;
    if (!DecodeRole(data.role, &role))
        return false;

    *out_role = role;
    MemCpy(out_keys, payload + 32, RG_SIZE(*out_keys));

    switch (role) {
        case rk_UserRole::Admin: {
            SeedSigningPair(keyset->ckey, keyset->akey);
            crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
            crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
        } break;

        case rk_UserRole::WriteOnly: {
            ZeroSafe(keyset->ckey, RG_SIZE(keyset->ckey));
            ZeroSafe(keyset->dkey, RG_SIZE(keyset->dkey));
            ZeroSafe(keyset->lkey, RG_SIZE(keyset->lkey));
        } break;

        case rk_UserRole::ReadWrite: {
            ZeroSafe(keyset->ckey, RG_SIZE(keyset->ckey));
            crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
            crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
        } break;

        case rk_UserRole::LogOnly: {
            ZeroSafe(keyset->ckey, RG_SIZE(keyset->ckey));
            ZeroSafe(keyset->dkey, RG_SIZE(keyset->dkey));
            ZeroSafe(keyset->wkey, RG_SIZE(keyset->wkey));
            crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
        } break;
    }

    SeedSigningPair(keyset->skey, keyset->pkey);

#if 0
    PrintLn("ckey = %1", FmtSpan(out_keys->ckey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("akey = %1", FmtSpan(out_keys->akey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("dkey = %1", FmtSpan(out_keys->dkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("wkey = %1", FmtSpan(out_keys->wkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("lkey = %1", FmtSpan(out_keys->lkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("tkey = %1", FmtSpan(out_keys->tkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("nkey = %1", FmtSpan(out_keys->nkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("skey = %1", FmtSpan(out_keys->skey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("pkey = %1", FmtSpan(out_keys->pkey, FmtType::BigHex, "").Pad0(-2));

    Span<const uint8_t> pub = MakeSpan(payload, 32);
    PrintLn("public = %1", FmtSpan(pub, FmtType::BigHex, "").Pad0(-2));
#endif

    return true;
}

bool rk_Repository::WriteConfig(const char *path, Span<const uint8_t> data, bool overwrite)
{
    RG_ASSERT(HasMode(rk_AccessMode::Config));
    RG_ASSERT(data.len + crypto_sign_ed25519_BYTES <= RG_SIZE(ConfigData::cypher));

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

    // Sign config to detect tampering
    crypto_sign_ed25519(config.cypher, nullptr, data.ptr, (size_t)data.len, keyset->ckey);

    Size len = offsetof(ConfigData, cypher) + crypto_sign_ed25519_BYTES + data.len;
    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&config, len);

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

    RG_UNREACHABLE();
}

bool rk_Repository::ReadConfig(const char *path, Span<uint8_t> out_buf)
{
    ConfigData config = {};

    Span<uint8_t> buf = MakeSpan((uint8_t *)&config, RG_SIZE(config));
    Size len = disk->ReadFile(path, buf);

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

bool rk_Repository::CheckRepository()
{
    switch (disk->TestFile("rekkord")) {
        case StatResult::Success: return true;
        case StatResult::MissingPath: {
            LogError("Repository '%1' is not initialized or not valid", disk->GetURL());
            return false;
        } break;
        case StatResult::AccessDenied:
        case StatResult::OtherError: return false;
    }

    RG_UNREACHABLE();
}

bool rk_Repository::HasConditionalWrites()
{
    bool cw = false;
    TestConditionalWrites(&cw);
    return cw;
}

std::unique_ptr<rk_Repository> rk_OpenRepository(rk_Disk *disk, const rk_Config &config, bool authenticate)
{
    if (!disk)
        return nullptr;

    if (!config.Validate(authenticate))
        return nullptr;

    std::unique_ptr<rk_Repository> repo = std::make_unique<rk_Repository>(disk, config);

    if (authenticate) {
        if (config.key_filename) {
            Span<uint8_t> mkey = MakeSpan((uint8_t *)AllocateSafe(rk_MasterKeySize), rk_MasterKeySize);
            RG_DEFER_C(len = mkey.len) { ReleaseSafe(mkey.ptr, len); };

            mkey.len = ReadFile(config.key_filename, mkey);
            if (mkey.len < 0)
                return nullptr;

            if (!repo->Authenticate(mkey))
                return nullptr;
        } else {
            if (!repo->Authenticate(config.username, config.password))
                return nullptr;
        }
    }

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

    // Decode prefix
    {
        const auto ptr = std::find(std::begin(rk_BlobCatalogNames), std::end(rk_BlobCatalogNames), str[0]);

        if (ptr == std::end(rk_BlobCatalogNames))
            return false;

        out_oid->catalog = (rk_BlobCatalog)(ptr - rk_BlobCatalogNames);
    }

    for (Size i = 2, j = 0; i < 66; i += 2, j++) {
        int high = ParseHexadecimalChar(str[i - 1]);
        int low = ParseHexadecimalChar(str[i]);

        if (high < 0 || low < 0)
            return false;

        out_oid->hash.raw[j] = (uint8_t)((high << 4) | low);
    }

    return true;
}

}
