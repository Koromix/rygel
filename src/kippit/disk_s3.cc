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
#include "disk.hh"
#include "src/core/libsqlite/libsqlite.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static const int CacheVersion = 1;

#pragma pack(push, 1)
struct KeyData {
    uint8_t salt[16];
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    uint8_t cypher[crypto_secretbox_MACBYTES + 32];
};
#pragma pack(pop)

class S3Disk: public kt_Disk {
    s3_Session s3;

    sq_Database cache_db;
    std::atomic_int cache_hits {0};
    int cache_misses {0};
    std::mutex cache_mutex;

public:
    S3Disk(const s3_Config &config, const char *pwd);
    ~S3Disk() override;

    bool ReadRaw(const char *path, HeapArray<uint8_t> *out_obj) override;
    Size WriteRaw(const char *path, Size len, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
    bool ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths) override;
};

static bool DeriveKey(const char *pwd, const uint8_t salt[16], uint8_t out_key[32])
{
    RG_STATIC_ASSERT(crypto_pwhash_SALTBYTES == 16);

    if (crypto_pwhash(out_key, 32, pwd, strlen(pwd), salt, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_ARGON2ID13) != 0) {
        LogError("Failed to derive key from password (exhausted resource?)");
        return false;
    }

    return true;
}

static bool WriteKey(s3_Session *s3, const char *path, const char *pwd, const uint8_t payload[32])
{
    KeyData data;

    randombytes_buf(data.salt, RG_SIZE(data.salt));
    randombytes_buf(data.nonce, RG_SIZE(data.nonce));

    uint8_t key[32];
    if (!DeriveKey(pwd, data.salt, key))
        return false;

    crypto_secretbox_easy(data.cypher, payload, 32, data.nonce, key);

    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&data, RG_SIZE(data));
    return s3->PutObject(path, buf);
}

static bool ReadKey(s3_Session *s3, const char *path, const char *pwd, uint8_t *out_payload, bool *out_error)
{
    KeyData data;

    // Read file data
    {
        Span<uint8_t> buf = MakeSpan((uint8_t *)&data, RG_SIZE(data));
        Size len = s3->GetObject(path, buf);

        if (len != RG_SIZE(data)) {
            if (len >= 0) {
                LogError("Truncated key object '%1'", path);
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

S3Disk::S3Disk(const s3_Config &config, const char *pwd)
{
    if (!s3.Open(config))
        return;

    // Open disk and determine mode
    {
        bool error = false;

        if (ReadKey(&s3, "keys/write", pwd, pkey, &error)) {
            mode = kt_DiskMode::WriteOnly;
            memset(skey, 0, RG_SIZE(skey));
        } else if (ReadKey(&s3, "keys/full", pwd, skey, &error)) {
            mode = kt_DiskMode::ReadWrite;
            crypto_scalarmult_base(pkey, skey);
        } else {
            if (!error) {
                LogError("Failed to open repository (wrong password?)");
            }
            return;
        }
    }

    // Open cache database
    {
        const char *cache_dir = GetUserCachePath("kippit", &str_alloc);
        if (!MakeDirectory(cache_dir, false))
            return;

        const char *cache_filename = Fmt(&str_alloc, "%1%/%2.db", cache_dir, FmtSpan(pkey, FmtType::SmallHex, "").Pad0(-2)).ptr;
        LogDebug("Cache file: %1", cache_filename);

        if (!cache_db.Open(cache_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return;
        if (!cache_db.SetWAL(true))
            return;

        int version;
        if (!cache_db.GetUserVersion(&version))
            return;

        if (version > CacheVersion) {
            LogError("Cache schema is too recent (%1, expected %2)", version, CacheVersion);
            return;
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
                    } break;

                    RG_STATIC_ASSERT(CacheVersion == 1);
                }

                if (!cache_db.SetUserVersion(CacheVersion))
                    return false;

                return true;
            });
            if (!success)
                return;
        }
    }

    // We're good!
    url = s3.GetURL();
}

S3Disk::~S3Disk()
{
}

bool S3Disk::ReadRaw(const char *path, HeapArray<uint8_t> *out_obj)
{
    return s3.GetObject(path, Mebibytes(256), out_obj);
}

Size S3Disk::WriteRaw(const char *path, Size total_len, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func)
{
    // Fast detection of known objects
    {
        sq_Statement stmt;
        if (!cache_db.Prepare("SELECT rowid FROM objects WHERE key = ?1", &stmt))
            return -1;
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

        if (stmt.Step()) {
            return 0;
        } else if (!stmt.IsValid()) {
            return -1;
        }
    }

    // Probabilistic detection and rebuild of outdated cache
    if (GetRandomIntSafe(0, 20) <= 1) {
        int hits = ++cache_hits;
        bool miss = s3.HasObject(path);

        if (miss) {
            std::lock_guard<std::mutex> lock(cache_mutex);

            cache_misses++;

            if (hits >= 20 && cache_misses >= hits / 5) {
                BlockAllocator temp_alloc;

                HeapArray<const char *> keys;
                if (!s3.ListObjects(nullptr, &temp_alloc, &keys))
                    return -1;

                for (const char *key: keys) {
                    if (!cache_db.Run(R"(INSERT INTO objects (key) VALUES (?1)
                                         ON CONFLICT (key) DO NOTHING)", key))
                        return -1;
                }

                cache_hits = 0;
                cache_misses = 0;
            }

            if (!cache_db.Run(R"(INSERT INTO objects (key) VALUES (?1)
                                 ON CONFLICT (key) DO NOTHING)", path))
                return -1;

            return 0;
        }
    }

    HeapArray<uint8_t> obj;
    obj.Reserve(total_len);
    if (!func([&](Span<const uint8_t> buf) { obj.Append(buf); return true; }))
        return -1;
    RG_ASSERT(obj.len == total_len);

    if (!s3.PutObject(path, obj))
        return -1;
    if (!cache_db.Run(R"(INSERT INTO objects (key) VALUES (?1)
                         ON CONFLICT (key) DO NOTHING)", path))
        return -1;

    return total_len;
}

bool S3Disk::ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths)
{
    if (!EndsWith(path, "/")) {
        path = Fmt(alloc, "%1/", path).ptr;
    }

    return s3.ListObjects(path, alloc, out_paths);
}

kt_Disk *kt_CreateS3Disk(const s3_Config &config, const char *full_pwd, const char *write_pwd)
{
    s3_Session s3;

    if (!s3.Open(config))
        return nullptr;

    // Drop created keys if anything fails
    HeapArray<const char *> keys;
    RG_DEFER_N(root_guard) {
        for (const char *key: keys) {
            s3.DeleteObject(key);
        }
    };

    if (s3.HasObject("keys/full")) {
        LogError("S3 repository '%1' looks already initialized", s3.GetURL());
        return nullptr;
    }

    // Generate master keys
    uint8_t skey[32];
    uint8_t pkey[32];
    crypto_box_keypair(pkey, skey);

    // Write control files
    if (!WriteKey(&s3, "keys/full", full_pwd, skey))
        return nullptr;
    keys.Append("keys/full");
    if (!WriteKey(&s3, "keys/write", write_pwd, pkey))
        return nullptr;
    keys.Append("keys/write");

    kt_Disk *disk = kt_OpenS3Disk(config, full_pwd);
    if (!disk)
        return nullptr;

    root_guard.Disable();
    return disk;
}

kt_Disk *kt_OpenS3Disk(const s3_Config &config, const char *pwd)
{
    kt_Disk *disk = new S3Disk(config, pwd);

    if (!disk->GetURL()) {
        delete disk;
        return nullptr;
    }

    return disk;
}

}
