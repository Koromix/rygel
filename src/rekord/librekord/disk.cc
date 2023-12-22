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
#include "config.hh"
#include "disk.hh"
#include "lz4.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static_assert(crypto_box_PUBLICKEYBYTES == 32);
static_assert(crypto_box_SECRETKEYBYTES == 32);
static_assert(crypto_secretbox_KEYBYTES == 32);
static_assert(crypto_secretstream_xchacha20poly1305_KEYBYTES == 32);

#pragma pack(push, 1)
struct KeyData {
    uint8_t salt[16];
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    uint8_t cypher[crypto_secretbox_MACBYTES + 32];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct SecretData {
    int8_t version;
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    uint8_t cypher[crypto_secretbox_MACBYTES + 2048];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BlobIntro {
    int8_t version;
    int8_t type;
    uint8_t ekey[crypto_secretstream_xchacha20poly1305_KEYBYTES + crypto_box_SEALBYTES];
    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
};
#pragma pack(pop)

static const int SecretVersion = 1;
static const int CacheVersion = 2;
static const int BlobVersion = 7;
static const Size BlobSplit = Kibibytes(32);

bool rk_Disk::Authenticate(const char *username, const char *pwd)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);

    RG_DEFER_N(err_guard) { Lock(); };

    const char *full_filename = Fmt(&str_alloc, "keys/%1/full", username).ptr;
    const char *write_filename = Fmt(&str_alloc, "keys/%1/write", username).ptr;

    // Open disk and determine mode
    {
        bool error = false;

        if (ReadKey(write_filename, pwd, pkey, &error)) {
            mode = rk_DiskMode::WriteOnly;
            memset(skey, 0, RG_SIZE(skey));
        } else if (ReadKey(full_filename, pwd, skey, &error)) {
            mode = rk_DiskMode::ReadWrite;
            crypto_scalarmult_base(pkey, skey);
        } else {
            if (!error) {
                LogError("Failed to open repository (wrong password?)");
            }
            return false;
        }
    }

    // Read repository ID
    if (!ReadSecret("rekord", id))
        return false;

    if (!OpenCache())
        return false;

    err_guard.Disable();
    return true;
}

void rk_Disk::Lock()
{
    mode = rk_DiskMode::Secure;

    ZeroMemorySafe(id, RG_SIZE(id));
    ZeroMemorySafe(pkey, RG_SIZE(pkey));
    ZeroMemorySafe(skey, RG_SIZE(skey));

    cache_db.Close();
}

static inline FmtArg GetPrefix3(const rk_ID &id)
{
    uint64_t prefix = ((uint64_t)id.hash[0] << 4) | ((uint64_t)id.hash[1] >> 4);
    return FmtHex(prefix).Pad0(-3);
}

bool rk_Disk::ReadBlob(const rk_ID &id, rk_BlobType *out_type, HeapArray<uint8_t> *out_blob)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::ReadWrite);

    Size prev_len = out_blob->len;
    RG_DEFER_N(err_guard) { out_blob->RemoveFrom(prev_len); };

    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "blobs/%1/%2", GetPrefix3(id), id).len;

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
        memcpy(&intro, remain.ptr, RG_SIZE(intro));

        if (intro.version > BlobVersion) {
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
        if (crypto_box_seal_open(key, intro.ekey, RG_SIZE(intro.ekey), pkey, skey) != 0) {
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

        while (remain.len) {
            Size in_len = std::min(remain.len, BlobSplit + crypto_secretstream_xchacha20poly1305_ABYTES);
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

            bool eof = !remain.len;
            bool success = lz4.Flush(eof, [&](Span<const uint8_t> buf) {
                out_blob->Append(buf);
                return true;
            });
            if (!success)
                return false;

            if (eof) {
                if (tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
                    LogError("Truncated blob");
                    return false;
                }
                break;
            }
        }
    }

    *out_type = type;
    err_guard.Disable();
    return true;
}

Size rk_Disk::WriteBlob(const rk_ID &id, rk_BlobType type, Span<const uint8_t> blob)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::WriteOnly || mode == rk_DiskMode::ReadWrite);

    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "blobs/%1/%2", GetPrefix3(id), id).len;

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
            if (crypto_box_seal(intro.ekey, key, RG_SIZE(key), pkey) != 0) {
                LogError("Failed to seal symmetric key");
                return false;
            }

            Span<const uint8_t> buf = MakeSpan((const uint8_t *)&intro, RG_SIZE(intro));

            if (!func(buf))
                return false;
        }

        // Initialize compression
        EncodeLZ4 lz4;
        if (!lz4.Start())
            return false;

        // Encrypt blob data
        {
            bool complete = false;

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

                    Size treshold = complete ? 1 : BlobSplit;
                    Size processed = 0;

                    while (buf.len >= treshold) {
                        Size piece_len = std::min(BlobSplit, buf.len);
                        Span<const uint8_t> piece = buf.Take(0, piece_len);

                        buf.ptr += piece.len;
                        buf.len -= piece.len;
                        processed += piece.len;

                        uint8_t cypher[BlobSplit + crypto_secretstream_xchacha20poly1305_ABYTES];
                        unsigned char tag = (complete && !buf.len) ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
                        unsigned long long cypher_len;
                        crypto_secretstream_xchacha20poly1305_push(&state, cypher, &cypher_len, piece.ptr, piece.len, nullptr, 0, tag);

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

Size rk_Disk::WriteTag(const rk_ID &id)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::WriteOnly || mode == rk_DiskMode::ReadWrite);

    // Prepare sealed ID
    uint8_t cypher[crypto_box_SEALBYTES + 32];
    if (crypto_box_seal(cypher, id.hash, RG_SIZE(id.hash), pkey) != 0) {
        LogError("Failed to seal ID");
        return -1;
    }

    // Write tag file with random name, retry if name is already used
    for (int i = 0; i < 1000; i++) {
        LocalArray<char, 256> path;
        path.len = Fmt(path.data, "tags/%1", FmtRandom(8)).len;

        Size written = WriteDirect(path.data, cypher);

        if (written > 0)
            return written;
        if (written < 0)
            return -1;
    }

    // We really really should never reach this...
    LogError("Failed to create tag for '%1'", id);
    return -1;
}

bool rk_Disk::ListTags(HeapArray<rk_ID> *out_ids)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::ReadWrite);

    BlockAllocator temp_alloc;

    Size start_len = out_ids->len;
    RG_DEFER_N(out_guard) { out_ids->RemoveFrom(start_len); };

    HeapArray<const char *> filenames;
    {
        bool success = ListRaw("tags", [&](const char *filename) {
            filename = DuplicateString(filename, &temp_alloc).ptr;
            filenames.Append(filename);

            return true;
        });
        if (!success)
            return false;
    }

    HeapArray<bool> ready;
    out_ids->AppendDefault(filenames.len);
    ready.AppendDefault(filenames.len);

    Async async(GetThreads());

    // List snapshots
    for (Size i = 0; i < filenames.len; i++) {
        const char *filename = filenames[i];

        async.Run([=, &ready, this]() {
            uint8_t blob[crypto_box_SEALBYTES + 32];
            Size len = ReadRaw(filename, blob);

            if (len != crypto_box_SEALBYTES + 32) {
                if (len >= 0) {
                    LogError("Malformed tag file '%1' (ignoring)", filename);
                }
                return true;
            }

            rk_ID id = {};
            if (crypto_box_seal_open(id.hash, blob, RG_SIZE(blob), pkey, skey) != 0) {
                LogError("Failed to unseal tag (ignoring)");
                return true;
            }

            out_ids->ptr[start_len + i] = id;
            ready[i] = true;

            return true;
        });
    }

    if (!async.Sync())
        return false;

    Size j = 0;
    for (Size i = 0; i < filenames.len; i++) {
        out_ids->ptr[start_len + j] = out_ids->ptr[start_len + i];
        j += ready[i];
    }
    out_ids->len = start_len + j;

    out_guard.Disable();
    return true;
}

bool rk_Disk::InitDefault(const char *full_pwd, const char *write_pwd)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);

    // Drop created files if anything fails
    HeapArray<const char *> names;
    RG_DEFER_N(err_guard) {
        Lock();

        DeleteRaw("rekord");
        DeleteRaw("keys/default/full");
        DeleteRaw("keys/default/write");
    };

    if (TestSlow("rekord")) {
        LogError("Repository '%1' looks already initialized", url);
        return false;
    }

    // Generate random ID and keys
    randombytes_buf(id, RG_SIZE(id));
    crypto_box_keypair(pkey, skey);

    if (!WriteSecret("rekord", id))
        return false;
    names.Append("rekord");

    // Write key files
    if (!WriteKey("keys/default/full", full_pwd, skey))
        return false;
    names.Append("keys/default/full");
    if (!WriteKey("keys/default/write", write_pwd, pkey))
        return false;
    names.Append("keys/default/write");

    // Success!
    mode = rk_DiskMode::ReadWrite;

    err_guard.Disable();
    return true;
}

rk_Disk::TestResult rk_Disk::TestFast(const char *path)
{
    if (!cache_db.IsValid())
        return TestSlow(path) ? TestResult::Exists : TestResult::Missing;

    bool should_exist;
    {
        sq_Statement stmt;
        if (!cache_db.Prepare("SELECT rowid FROM objects WHERE key = ?1", &stmt))
            return TestResult::FatalError;
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

        should_exist = stmt.Step();
    }

    // Probabilistic check
    if (GetRandomIntSafe(0, 100) < 2) {
        bool really_exists = TestSlow(path);

        if (really_exists && !should_exist) {
            std::unique_lock<std::mutex> lock(cache_mutex, std::try_to_lock);

            if (lock.owns_lock() && ++cache_misses >= 4) {
                RebuildCache();
                cache_misses = 0;
            }

            return really_exists ? TestResult::Exists : TestResult::Missing;
        } else if (should_exist && !really_exists) {
            ClearCache();

            LogError("The local cache database was mismatched and could have resulted in missing data in the backup.");
            LogError("You must start over to fix this situation.");

            return TestResult::FatalError;
        }
    }

    return should_exist ? TestResult::Exists : TestResult::Missing;
}

bool rk_Disk::PutCache(const char *key)
{
    if (!cache_db.IsValid())
        return true;

    bool success = cache_db.Run(R"(INSERT INTO objects (key) VALUES (?1)
                                   ON CONFLICT DO NOTHING)", key);
    return success;
}

static bool DeriveKey(const char *pwd, const uint8_t salt[16], uint8_t out_key[32])
{
    static_assert(crypto_pwhash_SALTBYTES == 16);

    if (crypto_pwhash(out_key, 32, pwd, strlen(pwd), salt, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_ARGON2ID13) != 0) {
        LogError("Failed to derive key from password (exhausted resource?)");
        return false;
    }

    return true;
}

bool rk_Disk::WriteKey(const char *path, const char *pwd, const uint8_t payload[32])
{
    KeyData data;

    randombytes_buf(data.salt, RG_SIZE(data.salt));
    randombytes_buf(data.nonce, RG_SIZE(data.nonce));

    uint8_t key[32];
    if (!DeriveKey(pwd, data.salt, key))
        return false;

    crypto_secretbox_easy(data.cypher, payload, 32, data.nonce, key);

    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&data, RG_SIZE(data));
    Size written = WriteDirect(path, buf);

    if (written < 0)
        return false;
    if (!written) {
        LogError("Key file '%1' already exists", path);
        return false;
    }

    return true;
}

bool rk_Disk::ReadKey(const char *path, const char *pwd, uint8_t *out_payload, bool *out_error)
{
    KeyData data;

    // Read file data
    {
        Span<uint8_t> buf = MakeSpan((uint8_t *)&data, RG_SIZE(data));
        Size len = ReadRaw(path, buf);

        if (len != RG_SIZE(data)) {
            if (len >= 0) {
                LogError("Truncated key '%1'", path);
            }

            *out_error = true;
            return false;
        }
    }

    uint8_t key[32];
    if (!DeriveKey(pwd, data.salt, key)) {
        *out_error = true;
        return false;
    }

    bool success = !crypto_secretbox_open_easy(out_payload, data.cypher, RG_SIZE(data.cypher), data.nonce, key);
    return success;
}

bool rk_Disk::WriteSecret(const char *path, Span<const uint8_t> data)
{
    RG_ASSERT(data.len + crypto_secretbox_MACBYTES <= RG_SIZE(SecretData::cypher));

    SecretData secret = {};

    secret.version = SecretVersion;

    randombytes_buf(secret.nonce, RG_SIZE(secret.nonce));
    crypto_secretbox_easy(secret.cypher, data.ptr, (size_t)data.len, secret.nonce, pkey);

    Size len = RG_OFFSET_OF(SecretData, cypher) + crypto_secretbox_MACBYTES + data.len;
    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&secret, len);
    Size written = WriteDirect(path, buf);

    if (written < 0)
        return false;
    if (!written) {
        LogError("Secret file '%1' already exists", path);
        return false;
    }

    return true;
}

bool rk_Disk::ReadSecret(const char *path, Span<uint8_t> out_buf)
{
    SecretData secret;

    Span<uint8_t> buf = MakeSpan((uint8_t *)&secret, RG_SIZE(secret));
    Size len = ReadRaw(path, buf);

    if (len < 0)
        return false;
    if (len < RG_OFFSET_OF(SecretData, cypher)) {
        LogError("Malformed secret file '%1'", path);
        return false;
    }

    len -= RG_OFFSET_OF(SecretData, cypher);
    len = std::min(len, out_buf.len + crypto_secretbox_MACBYTES);

    if (crypto_secretbox_open_easy(out_buf.ptr, secret.cypher, len, secret.nonce, pkey)) {
        LogError("Failed to decrypt secret '%1'", path);
        return false;
    }

    return true;
}

Size rk_Disk::WriteDirect(const char *path, Span<const uint8_t> buf)
{
    if (TestSlow(path))
        return 0;

    Size written = WriteRaw(path, [&](FunctionRef<bool(Span<const uint8_t>)> func) { return func(buf); });
    return written;
}

bool rk_Disk::OpenCache()
{
    const char *cache_dir = GetUserCachePath("rekord", &str_alloc);
    if (!cache_dir) {
        LogError("Cannot find user cache path");
        return false;
    }
    if (!MakeDirectory(cache_dir, false))
        return false;

    const char *cache_filename = Fmt(&str_alloc, "%1%/%2.db", cache_dir, FmtSpan(id, FmtType::SmallHex, "").Pad0(-2)).ptr;
    LogDebug("Cache file: %1", cache_filename);

    if (!cache_db.Open(cache_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return false;
    if (!cache_db.SetWAL(true))
        return false;

    int version;
    if (!cache_db.GetUserVersion(&version))
        return false;

    if (version > CacheVersion) {
        LogError("Cache schema is too recent (%1, expected %2)", version, CacheVersion);
        return false;
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
                } // [[fallthrough]];

                static_assert(CacheVersion == 2);
            }

            if (!cache_db.SetUserVersion(CacheVersion))
                return false;

            return true;
        });
        if (!success)
            return false;
    }

    return true;
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

bool rk_Disk::RebuildCache()
{
    if (!cache_db.IsValid())
        return true;

    BlockAllocator temp_alloc;

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
    if (!success)
        return false;

    return true;
}

std::unique_ptr<rk_Disk> rk_Open(const rk_Config &config, bool authenticate)
{
    if (!config.Validate(authenticate))
        return nullptr;

    const char *username = authenticate ? config.username : nullptr;
    const char *password = authenticate ? config.password : nullptr;

    switch (config.type) {
        case rk_DiskType::Local: return rk_OpenLocalDisk(config.url, username, password, config.threads);
        case rk_DiskType::SFTP: return rk_OpenSftpDisk(config.ssh, username, password, config.threads);
        case rk_DiskType::S3: return rk_OpenS3Disk(config.s3, username, password, config.threads);
    }

    RG_UNREACHABLE();
}

}
