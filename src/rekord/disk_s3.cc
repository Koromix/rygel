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
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

class S3Disk: public rk_Disk {
    s3_Session s3;

    std::atomic_int cache_hits {0};
    int cache_misses {0};
    std::mutex cache_mutex;

public:
    S3Disk(const s3_Config &config);
    ~S3Disk() override;

    bool Init(const char *full_pwd, const char *write_pwd) override;

    Size ReadRaw(const char *path, Span<uint8_t> out_buf) override;
    bool ReadRaw(const char *path, HeapArray<uint8_t> *out_obj) override;

    Size WriteRaw(const char *path, Size len, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
    bool DeleteRaw(const char *path) override;

    bool ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths) override;
    bool TestRaw(const char *path) override;
};

S3Disk::S3Disk(const s3_Config &config)
{
    if (!s3.Open(config))
        return;

    // We're good!
    url = s3.GetURL();
}

S3Disk::~S3Disk()
{
}

bool S3Disk::Init(const char *full_pwd, const char *write_pwd)
{
    RG_ASSERT(mode == rk_DiskMode::Secure);
    return InitKeys(full_pwd, write_pwd);
}

Size S3Disk::ReadRaw(const char *path, Span<uint8_t> out_buf)
{
    return s3.GetObject(path, out_buf);
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

bool S3Disk::DeleteRaw(const char *path)
{
    return s3.DeleteObject(path);
}

bool S3Disk::ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths)
{
    if (!EndsWith(path, "/")) {
        path = Fmt(alloc, "%1/", path).ptr;
    }

    return s3.ListObjects(path, alloc, out_paths);
}

bool S3Disk::TestRaw(const char *path)
{
    sq_Statement stmt;
    if (!cache_db.Prepare("SELECT rowid FROM objects WHERE key = ?1", &stmt))
        return -1;
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

    return stmt.Step();
}

std::unique_ptr<rk_Disk> rk_OpenS3Disk(const s3_Config &config, const char *pwd)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<S3Disk>(config);

    if (!disk->GetURL())
        return nullptr;
    if (pwd && !disk->Open(pwd))
        return nullptr;

    return disk;
}

}
