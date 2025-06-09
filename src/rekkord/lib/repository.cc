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

static void SeedSigningPair(uint8_t sk[32], uint8_t pk[32])
{
    crypto_hash_sha512(pk, sk, 32);
    crypto_scalarmult_ed25519_base(pk, pk);
}

bool rk_Repository::IsRepository()
{
    StatResult ret = disk->TestFile("rekkord");
    return (ret == StatResult::Success);
}

bool rk_Repository::Init(Span<const uint8_t> mkey, Span<const rk_UserInfo> users)
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

    if (!disk->IsEmpty()) {
        if (disk->TestFile("rekkord") == StatResult::Success) {
            LogError("Repository '%1' looks already initialized", disk->GetURL());
            return false;
        } else {
            LogError("Directory '%1' exists and is not empty", disk->GetURL());
            return false;
        }
    }

    const auto make_directory = [&](const char *dirname) {
        if (!disk->CreateDirectory(dirname))
            return false;
        directories.Append(dirname);

        return true;
    };

    if (!make_directory(""))
        return false;
    if (!make_directory("keys"))
        return false;
    if (!make_directory("tags"))
        return false;
    if (!make_directory("blobs"))
        return false;
    if (!make_directory("tmp"))
        return false;

    for (char catalog: rk_BlobCatalogNames) {
        char parent[128];
        Fmt(parent, "blobs/%1", catalog);

        if (!make_directory(parent))
            return false;

        for (int i = 0; i < 256; i++) {
            char name[128];
            Fmt(name, "%1/%2", parent, FmtHex(i).Pad0(-2));

            if (!make_directory(name))
                return false;
        }
    }

    keyset = (rk_KeySet *)AllocateSafe(RG_SIZE(rk_KeySet));
    modes = UINT_MAX;
    role = "Admin";

    // Derive encryption keys
    crypto_kdf_blake2b_derive_from_key(keyset->ckey, RG_SIZE(keyset->ckey), (int)MasterDerivation::ConfigKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->dkey, RG_SIZE(keyset->dkey), (int)MasterDerivation::DataKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->lkey, RG_SIZE(keyset->lkey), (int)MasterDerivation::LogKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(keyset->skey, RG_SIZE(keyset->skey), (int)MasterDerivation::SignKey, DerivationContext, mkey.ptr);
    SeedSigningPair(keyset->ckey, keyset->akey);
    crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
    crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
    SeedSigningPair(keyset->skey, keyset->vkey);

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

    // Write key files
    for (const rk_UserInfo &user: users) {
        RG_ASSERT(user.pwd);

        const char *filename = Fmt(&temp_alloc, "keys/%1", user.username).ptr;
        if (!WriteKeys(filename, user.pwd, user.role, *keyset))
            return false;
        files.Append(filename);
    }

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
        LocalArray<uint8_t[32], MaxKeys> keys;

        if (!ReadKeys(filename, pwd, &role, keyset))
            return false;

        switch (role) {
            case rk_UserRole::Admin: {
                modes = (int)rk_AccessMode::Config |
                        (int)rk_AccessMode::Read |
                        (int)rk_AccessMode::Write |
                        (int)rk_AccessMode::Log;

                SeedSigningPair(keyset->ckey, keyset->akey);
                crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
                crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
                SeedSigningPair(keyset->skey, keyset->vkey);
            } break;

            case rk_UserRole::WriteOnly: {
                modes = (int)rk_AccessMode::Write;

                ZeroSafe(keyset->ckey, RG_SIZE(keyset->ckey));
                ZeroSafe(keyset->dkey, RG_SIZE(keyset->dkey));
                ZeroSafe(keyset->lkey, RG_SIZE(keyset->lkey));
                SeedSigningPair(keyset->skey, keyset->vkey);
            } break;

            case rk_UserRole::ReadWrite: {
                modes = (int)rk_AccessMode::Read |
                        (int)rk_AccessMode::Write |
                        (int)rk_AccessMode::Log;

                ZeroSafe(keyset->ckey, RG_SIZE(keyset->ckey));
                crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
                crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
                SeedSigningPair(keyset->skey, keyset->vkey);
            } break;

            case rk_UserRole::LogOnly: {
                modes = (int)rk_AccessMode::Log;

                ZeroSafe(keyset->ckey, RG_SIZE(keyset->ckey));
                ZeroSafe(keyset->dkey, RG_SIZE(keyset->dkey));
                ZeroSafe(keyset->wkey, RG_SIZE(keyset->wkey));
                crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
                ZeroSafe(keyset->skey, RG_SIZE(keyset->skey));
            } break;
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
    crypto_kdf_blake2b_derive_from_key(keyset->skey, RG_SIZE(keyset->skey), (int)MasterDerivation::SignKey, DerivationContext, mkey.ptr);
    SeedSigningPair(keyset->ckey, keyset->akey);
    crypto_scalarmult_curve25519_base(keyset->wkey, keyset->dkey);
    crypto_scalarmult_curve25519_base(keyset->tkey, keyset->lkey);
    SeedSigningPair(keyset->skey, keyset->vkey);

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

    CloseCache();
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

    CloseCache();
    MemCpy(ids.cid, new_ids.cid, RG_SIZE(new_ids.cid));

    return true;
}

sq_Database *rk_Repository::OpenCache(bool build)
{
    RG_ASSERT(keyset);

    CloseCache();

    RG_DEFER_N(err_guard) { CloseCache(); };

    uint8_t cache_id[32] = {};
    {
        // Combine repository URL and RID to create a secure ID

        Span<const char> url = disk->GetURL();

        static_assert(RG_SIZE(cache_id) == crypto_hash_sha256_BYTES);

        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        crypto_hash_sha256_update(&state, ids.rid, RG_SIZE(ids.rid));
        crypto_hash_sha256_update(&state, (const uint8_t *)url.ptr, (size_t)url.len);

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
                } [[fallthrough]];

                case 4: {
                    bool success = cache_db.RunMany(R"(
                        CREATE TABLE meta (
                            cid BLOB
                        );
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 5: {
                    bool success = cache_db.RunMany(R"(
                        DROP INDEX stats_p;

                        ALTER TABLE stats RENAME TO stats_BAK;

                        CREATE TABLE stats (
                            path TEXT NOT NULL,
                            mtime INTEGER NOT NULL,
                            ctime INTEGER NOT NULL,
                            btime INEGER NOT NULL,
                            mode INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            hash BLOB NOT NULL
                        );
                        CREATE UNIQUE INDEX stats_p ON stats (path);

                        INSERT INTO stats (path, mtime, ctime, btime, mode, size, hash)
                            SELECT path, mtime, btime, btime, mode, size, hash FROM stats_BAK;

                        DROP TABLE stats_BAK;
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 6: {
                    bool success = cache_db.RunMany(R"(
                        ALTER TABLE stats DROP COLUMN btime;
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 7: {
                    bool success = cache_db.RunMany(R"(
                        DROP INDEX objects_k;
                        DROP INDEX stats_p;

                        DROP TABLE objects;
                        DROP TABLE stats;

                        CREATE TABLE objects (
                            key TEXT NOT NULL,
                            size INTEGER NOT NULL
                        );
                        CREATE UNIQUE INDEX objects_k ON objects (key);

                        CREATE TABLE stats (
                            path TEXT NOT NULL,
                            mtime INTEGER NOT NULL,
                            ctime INTEGER NOT NULL,
                            mode INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            hash BLOB NOT NULL,
                            stored INTEGER NOT NULL
                        );
                        CREATE UNIQUE INDEX stats_p ON stats (path);

                        UPDATE meta SET cid = NULL;
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 8: {
                    bool success = cache_db.RunMany(R"(
                        ALTER TABLE objects ADD COLUMN checked INTEGER;
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 9: {
                    bool success = cache_db.RunMany(R"(
                        CREATE TABLE blobs (
                            key TEXT NOT NULL,
                            size INTEGER NOT NULL,
                            checked INTEGER,
                            missing INTEGER CHECK (missing IN (0, 1)) NOT NULL
                        );
                        CREATE UNIQUE INDEX blobs_k ON blobs (key);

                        INSERT INTO blobs (key, size, checked, missing)
                            SELECT key, size, checked, 0 FROM objects;

                        DROP INDEX objects_k;
                        DROP TABLE objects;
                    )");
                    if (!success)
                        return false;
                } // [[fallthrough]];

                static_assert(CacheVersion == 10);
            }

            if (!cache_db.SetUserVersion(CacheVersion))
                return false;

            return true;
        });

        if (!success) {
            cache_db.Close();
            return nullptr;
        }
    }

    // Check known CID against repository CID
    {
        sq_Statement stmt;
        if (!cache_db.Prepare("SELECT cid FROM meta", &stmt))
            return nullptr;

        if (stmt.Step()) {
            Span<const uint8_t> cid = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 0),
                                               sqlite3_column_bytes(stmt, 0));

            build &= (cid.len != RG_SIZE(ids.cid)) || memcmp(cid.ptr, ids.cid, RG_SIZE(ids.cid));
        } else if (stmt.IsValid()) {
            if (!cache_db.Run("INSERT INTO meta (cid) VALUES (NULL)"))
                return nullptr;
        } else {
            return nullptr;
        }
    }

    if (build) {
        LogInfo("Rebuilding cache...");

        if (!ResetCache(true))
            return nullptr;
    }

    if (!cache_db.Run("BEGIN IMMEDIATE TRANSACTION"))
        return nullptr;
    cache_commit = GetMonotonicTime();

    err_guard.Disable();
    return &cache_db;
}

bool rk_Repository::ResetCache(bool list)
{
    if (!cache_db.IsValid()) {
        LogInfo("Cannot reset closed cache");
        return false;
    }

    if (!cache_db.Run("DELETE FROM stats"))
        return false;

    if (!cache_db.Run("UPDATE blobs SET missing = 1"))
        return false;
    if (list) {
        bool success = disk->ListFiles("blobs", [&](const char *path, int64_t size) {
            // We never write empty blobs, something went wrong
            if (!size) [[unlikely]]
                return true;

            if (!cache_db.Run(R"(INSERT INTO blobs (key, size, missing)
                                 VALUES (?1, ?2, 0)
                                 ON CONFLICT (key) DO UPDATE SET size = excluded.size,
                                                                 missing = 0)",
                              path, size))
                return false;

            return true;
        });
        if (!success)
            return false;
    }
    if (!cache_db.Run("DELETE FROM blobs WHERE missing = 1"))
        return false;

    Span<const uint8_t> cid = ids.cid;

    if (!cache_db.Run("UPDATE meta SET cid = ?1", cid))
        return false;

    if (!CommitCache(true))
        return false;

    return true;
}

bool rk_Repository::InitUser(const char *username, rk_UserRole role, const char *pwd, bool force)
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
        if (force) {
            LogWarning("Overwriting existing user '%1'", username);
        } else {
            LogError("User '%1' already exists", username);
            return false;
        }
    }

    disk->DeleteFile(filename);

    if (!WriteKeys(filename, pwd, role, *keyset))
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
                    LogError("Truncated keys in '%1'", path);
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

bool rk_Repository::ReadBlob(const rk_ObjectID &oid, int *out_type, HeapArray<uint8_t> *out_blob)
{
    char path[256];
    Fmt(path, "blobs/%1/%2/%3", rk_BlobCatalogNames[(int)oid.catalog], GetBlobPrefix(oid.hash), oid.hash);

    return ReadBlob(path, out_type, out_blob);
}

bool rk_Repository::ReadBlob(const char *path, int *out_type, HeapArray<uint8_t> *out_blob)
{
    RG_ASSERT(HasMode(rk_AccessMode::Read));

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
    err_guard.Disable();
    return true;
}

Size rk_Repository::WriteBlob(const rk_ObjectID &oid, int type, Span<const uint8_t> blob)
{
    char path[256];
    Fmt(path, "blobs/%1/%2/%3", rk_BlobCatalogNames[(int)oid.catalog], GetBlobPrefix(oid.hash), oid.hash);

    return WriteBlob(path, type, blob);
}

Size rk_Repository::WriteBlob(const char *path, int type, Span<const uint8_t> blob)
{
    RG_ASSERT(HasMode(rk_AccessMode::Write));
    RG_ASSERT(type >= 0 && type < INT8_MAX);

    // Skip objects that already exist
    {
        int64_t written = 0;

        switch (TestFast(path, &written)) {
            case StatResult::Success: return (Size)written;
            case StatResult::MissingPath: {} break;

            case StatResult::AccessDenied:
            case StatResult::OtherError: return -1;
        }
    }

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
            return -1;
        }
        if (crypto_box_seal(intro.ekey, key, RG_SIZE(key), keyset->wkey) != 0) {
            LogError("Failed to seal symmetric key");
            return -1;
        }

        Span<const uint8_t> buf = MakeSpan((const uint8_t *)&intro, RG_SIZE(intro));
        raw.Append(buf);
    }

    // Initialize compression
    EncodeLZ4 lz4;
    if (!lz4.Start(compression_level))
        return -1;

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
                return -1;

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
                return -1;
        } while (!complete);
    }

    // Write the damn thing
    {
        rk_WriteResult ret = disk->WriteFile(path, raw, (int)rk_WriteFlag::Lockable);
        bool overwrite = (ret == rk_WriteResult::Success);

        switch (ret) {
            case rk_WriteResult::Success:
            case rk_WriteResult::AlreadyExists: {} break;

            case rk_WriteResult::OtherError: return -1;
        }

        if (!PutCache(path, raw.len, overwrite))
            return -1;
    }

    return raw.len;
}

bool rk_Repository::WriteTag(const rk_ObjectID &oid, Span<const uint8_t> payload)
{
    // Accounting for the ID and order value, and base64 encoding, each filename must fit into 255 characters
    const Size MaxFragmentSize = 160;

    RG_ASSERT(HasMode(rk_AccessMode::Write));

    BlockAllocator temp_alloc;

    uint8_t prefix[16] = {};
    TagIntro intro = {};

    // Prepare tag data
    randombytes_buf(prefix, RG_SIZE(prefix));
    intro.version = TagVersion;
    intro.oid = oid;

    // Encrypt tag
    HeapArray<uint8_t> cypher(&temp_alloc);
    {
        HeapArray<uint8_t> src(&temp_alloc);

        src.Append(MakeSpan((const uint8_t *)&intro, RG_SIZE(intro)));
        src.Append(payload);

        Size cypher_len = src.len + crypto_box_SEALBYTES;
        cypher.Reserve(cypher_len);

        if (crypto_box_seal(cypher.ptr, src.ptr, (size_t)src.len, keyset->tkey) != 0) {
            LogError("Failed to seal tag payload");
            return false;
        }

        cypher.len = src.len + crypto_box_SEALBYTES;
    }

    // Sign it to avoid tampering
    {
        cypher.Reserve(crypto_sign_BYTES);
        crypto_sign_ed25519_detached(cypher.end(), nullptr, cypher.ptr, cypher.len, keyset->skey);
        cypher.len += crypto_sign_BYTES;
    }

    // How many files do we need to generate?
    int fragments = (cypher.len + MaxFragmentSize - 1) / MaxFragmentSize;

    if (fragments >= 100) {
        LogError("Excessive tag payload size");
        return false;
    }

    for (int i = 0; i < fragments; i++) {
        Size offset = i * MaxFragmentSize;
        Size len = std::min(offset + MaxFragmentSize, cypher.len) - offset;

        LocalArray<char, 512> path;
        path.len = Fmt(path.data, "tags/%1_%2_", FmtSpan(prefix, FmtType::BigHex, "").Pad0(-2), FmtArg(i).Pad0(-2)).len;

        sodium_bin2base64(path.end(), path.Available(), cypher.ptr + offset, len, sodium_base64_VARIANT_URLSAFE_NO_PADDING);

        if (disk->WriteFile(path.data, {}, (int)rk_WriteFlag::Lockable) != rk_WriteResult::Success)
            return false;
    }

    return true;
}

bool rk_Repository::ListTags(Allocator *alloc, HeapArray<rk_TagInfo> *out_tags)
{
    RG_ASSERT(HasMode(rk_AccessMode::Log));

    Size start_len = out_tags->len;
    RG_DEFER_N(out_guard) { out_tags->RemoveFrom(start_len); };

    HeapArray<Span<const char>> filenames;
    {
        bool success = disk->ListFiles("tags", [&](const char *path, int64_t) {
            Span<const char> filename = DuplicateString(path, alloc);
            filenames.Append(filename);

            return true;
        });
        if (!success)
            return false;
    }

    std::sort(filenames.begin(), filenames.end(),
              [](Span<const char> filename1, Span<const char> filename2) {
        return CmpStr(filename1, filename2) < 0;
    });

    Size offset = 0;

    while (offset < filenames.len) {
        rk_TagInfo tag = {};

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

        if (cypher.len < (Size)crypto_box_SEALBYTES + RG_SIZE(TagIntro) + (Size)crypto_sign_BYTES) {
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
        Span<uint8_t> src = AllocateSpan<uint8_t>(alloc, src_len);

        // Get payload back at least
        if (crypto_box_seal_open(src.ptr, cypher.ptr, cypher.len - crypto_sign_BYTES, keyset->tkey, keyset->lkey) != 0) {
            LogError("Failed to unseal tag data from '%1'", prefix);
            continue;
        }

        TagIntro intro = {};
        MemCpy(&intro, src.ptr, RG_SIZE(intro));

        if (intro.version == TagVersion) {
            src = src.Take(RG_SIZE(intro), src.len - RG_SIZE(intro));
        } else if (intro.version == 2) {
            MemMove((uint8_t *)&intro.oid + 1, (const uint8_t *)&intro.oid, RG_SIZE(rk_Hash));
            intro.oid.catalog = rk_BlobCatalog::Meta;

            Span<uint8_t> copy = AllocateSpan<uint8_t>(alloc, src.len - RG_SIZE(TagIntro) + 1);
            MemCpy(copy.ptr, src.ptr + RG_SIZE(TagIntro) - 1, copy.len);

            src = copy;
        } else {
            LogError("Unexpected tag version %1 (expected %2) in '%3'", intro.version, TagVersion, prefix);
            continue;
        }

        if (!intro.oid.IsValid()) {
            LogError("Invalid tag OID in '%1'", prefix);
            continue;
        }

        tag.prefix = DuplicateString(prefix, alloc).ptr;
        tag.oid = intro.oid;
        tag.payload = src;

        out_tags->Append(tag);
    }

    out_guard.Disable();
    return true;
}

bool rk_Repository::PutCache(const char *key, int64_t size, bool overwrite)
{
    if (!cache_db.IsValid())
        return true;

    bool success = cache_db.Run(R"(INSERT INTO blobs (key, size, missing)
                                   VALUES (?1, ?2, 0)
                                   ON CONFLICT DO UPDATE SET checked = IF(?3 = 1, NULL, checked))",
                                key, size, 0 + overwrite);
    if (!success)
        return false;

    CommitCache(false);

    return success;
}

bool rk_Repository::CommitCache(bool force)
{
    int64_t commit = cache_commit.load(std::memory_order_relaxed);

    if (!cache_db.IsValid())
        return true;
    if (!commit)
        return true;

    int64_t now = GetMonotonicTime();

    if (!force) {
        int64_t delay = now - commit;
        force |= (delay >= CacheDelay);
    }

    if (force) {
        std::unique_lock lock(cache_mutex, std::try_to_lock);

        if (lock.owns_lock()) {
            if (!cache_db.RunMany("COMMIT; BEGIN IMMEDIATE TRANSACTION;"))
                return false;

            cache_commit = now;
        }
    }

    return true;
}

void rk_Repository::CloseCache()
{
    if (!cache_db.IsValid())
        return;

    CommitCache(true);

    cache_db.Close();
    cache_commit = 0;
}

StatResult rk_Repository::TestFast(const char *path, int64_t *out_size)
{
    if (!cache_db.IsValid())
        return disk->TestFile(path, out_size);

    int64_t known_size = -1;
    {
        sq_Statement stmt;
        if (!cache_db.Prepare("SELECT size FROM blobs WHERE key = ?1", &stmt))
            return StatResult::OtherError;
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

        if (stmt.Step()) {
            known_size = sqlite3_column_int64(stmt, 0);
        }
    }

    // Probabilistic check
    if (GetRandomInt(0, 100) < 2) {
        int64_t real_size = -1;

        switch (disk->TestFile(path, &real_size)) {
            case StatResult::Success:
            case StatResult::MissingPath: {} break;

            case StatResult::AccessDenied: return StatResult::AccessDenied;
            case StatResult::OtherError: return StatResult::OtherError;
        }

        if (known_size >= 0 && real_size < 0) {
            ResetCache(false);

            LogError("The local cache database was mismatched and could have resulted in missing data in the backup.");
            LogError("You must start over to fix this situation.");

            return StatResult::OtherError;
        }
    }

    if (known_size < 0)
        return StatResult::MissingPath;

    if (out_size) {
        *out_size = known_size;
    }
    return StatResult::Success;
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

    // Pick relevant subset of keys
    switch (role) {
        case rk_UserRole::Admin: {
            MemCpy(payload + offsetof(rk_KeySet, ckey), keys.ckey, RG_SIZE(keys.ckey));
            MemCpy(payload + offsetof(rk_KeySet, dkey), keys.dkey, RG_SIZE(keys.dkey));
            MemCpy(payload + offsetof(rk_KeySet, lkey), keys.lkey, RG_SIZE(keys.lkey));
            MemCpy(payload + offsetof(rk_KeySet, skey), keys.skey, RG_SIZE(keys.skey));
        } break;

        case rk_UserRole::WriteOnly: {
            MemCpy(payload + offsetof(rk_KeySet, akey), keys.akey, RG_SIZE(keys.akey));
            MemCpy(payload + offsetof(rk_KeySet, wkey), keys.wkey, RG_SIZE(keys.wkey));
            MemCpy(payload + offsetof(rk_KeySet, tkey), keys.tkey, RG_SIZE(keys.tkey));
            MemCpy(payload + offsetof(rk_KeySet, skey), keys.skey, RG_SIZE(keys.skey));
        } break;

        case rk_UserRole::ReadWrite: {
            MemCpy(payload + offsetof(rk_KeySet, akey), keys.akey, RG_SIZE(keys.akey));
            MemCpy(payload + offsetof(rk_KeySet, dkey), keys.dkey, RG_SIZE(keys.dkey));
            MemCpy(payload + offsetof(rk_KeySet, lkey), keys.lkey, RG_SIZE(keys.lkey));
            MemCpy(payload + offsetof(rk_KeySet, skey), keys.skey, RG_SIZE(keys.skey));
        } break;

        case rk_UserRole::LogOnly: {
            MemCpy(payload + offsetof(rk_KeySet, akey), keys.akey, RG_SIZE(keys.akey));
            MemCpy(payload + offsetof(rk_KeySet, lkey), keys.lkey, RG_SIZE(keys.lkey));
            MemCpy(payload + offsetof(rk_KeySet, vkey), keys.vkey, RG_SIZE(keys.vkey));
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
    crypto_sign_ed25519_detached(data.sig, nullptr, (const uint8_t *)&data, offsetof(KeyData, sig), keyset->ckey);

    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&data, RG_SIZE(data));
    rk_WriteResult ret = disk->WriteFile(path, buf, 0);

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

#if 0
    PrintLn("ckey = %1", FmtSpan(out_keys->ckey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("akey = %1", FmtSpan(out_keys->akey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("dkey = %1", FmtSpan(out_keys->dkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("wkey = %1", FmtSpan(out_keys->wkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("lkey = %1", FmtSpan(out_keys->lkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("tkey = %1", FmtSpan(out_keys->tkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("skey = %1", FmtSpan(out_keys->skey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("vkey = %1", FmtSpan(out_keys->vkey, FmtType::BigHex, "").Pad0(-2));
#endif

    return true;
}

bool rk_Repository::WriteConfig(const char *path, Span<const uint8_t> data, bool overwrite)
{
    RG_ASSERT(HasMode(rk_AccessMode::Config));
    RG_ASSERT(data.len + crypto_sign_ed25519_BYTES <= RG_SIZE(ConfigData::cypher));

    ConfigData config = {};

    config.version = ConfigVersion;

    // Sign config to detect tampering
    crypto_sign_ed25519(config.cypher, nullptr, data.ptr, (size_t)data.len, keyset->ckey);

    Size len = offsetof(ConfigData, cypher) + crypto_sign_ed25519_BYTES + data.len;
    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&config, len);
    unsigned int flags = overwrite ? (int)rk_WriteFlag::Overwrite : 0;

    rk_WriteResult ret = disk->WriteFile(path, buf, flags);

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

std::unique_ptr<rk_Repository> rk_OpenRepository(rk_Disk *disk, const rk_Config &config, bool authenticate)
{
    if (!disk)
        return nullptr;

    if (!config.Validate(authenticate))
        return nullptr;

    int threads = (config.threads >= 0) ? config.threads : disk->GetDefaultThreads();
    const char *username = authenticate ? config.username : nullptr;
    const char *password = authenticate ? config.password : nullptr;

    std::unique_ptr<rk_Repository> repo = std::make_unique<rk_Repository>(disk, threads, config.compression_level);

    if (authenticate && !repo->Authenticate(username, password))
        return nullptr;

    return repo;
}

}
