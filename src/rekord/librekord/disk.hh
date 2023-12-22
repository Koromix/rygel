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

#pragma once

#include "src/core/libcc/libcc.hh"
#include "types.hh"
#include "src/core/libsqlite/libsqlite.hh"

namespace RG {

struct rk_Config;
struct s3_Config;
struct ssh_Config;

enum class rk_DiskMode {
    Secure,
    WriteOnly,
    ReadWrite
};
static const char *const rk_DiskModeNames[] = {
    "Secure",
    "WriteOnly",
    "ReadWrite"
};

enum class rk_BlobType: int8_t {
    Chunk = 0,
    File = 1,
    Directory = 2,
    Snapshot = 3,
    Link = 4
};
static const char *const rk_BlobTypeNames[] = {
    "Chunk",
    "File",
    "Directory",
    "Snapshot",
    "Link"
};

class rk_Disk {
protected:
    const char *url = nullptr;

    uint8_t id[32];

    rk_DiskMode mode = rk_DiskMode::Secure;
    uint8_t pkey[32] = {};
    uint8_t skey[32] = {};

    sq_Database cache_db;
    std::mutex cache_mutex;
    int cache_misses = 0;
    int threads = 1;

    BlockAllocator str_alloc;

public:
    enum class TestResult {
        Exists = 1,
        Missing = 0,
        FatalError = -1
    };

    virtual ~rk_Disk() = default;

    virtual bool Init(const char *full_pwd, const char *write_pwd) = 0;

    bool Authenticate(const char *username, const char *pwd);
    bool Authenticate(Span<const uint8_t> key);
    void Lock();

    bool InitUser(const char *username, const char *full_pwd, const char *write_pwd, bool force);
    bool DeleteUser(const char *username);

    const char *GetURL() const { return url; }
    Span<const uint8_t> GetID() const { return id; }
    Span<const uint8_t> GetSalt() const { return pkey; }
    rk_DiskMode GetMode() const { return mode; }

    Span<const uint8_t> GetFullKey() const
    {
        RG_ASSERT(mode == rk_DiskMode::ReadWrite);
        return skey;
    }
    Span<const uint8_t> GetWriteKey() const
    {
        RG_ASSERT(mode == rk_DiskMode::WriteOnly || mode == rk_DiskMode::ReadWrite);
        return pkey;
    }

    sq_Database *GetCache() { return &cache_db; }
    int GetThreads() const { return threads; }

    bool ReadBlob(const rk_ID &id, rk_BlobType *out_type, HeapArray<uint8_t> *out_blob);
    Size WriteBlob(const rk_ID &id, rk_BlobType type, Span<const uint8_t> blob);

    Size WriteTag(const rk_ID &id);
    bool ListTags(HeapArray<rk_ID> *out_ids);

protected:
    bool InitDefault(const char *full_pwd, const char *write_pwd);

    TestResult TestFast(const char *path);

    virtual Size ReadRaw(const char *path, Span<uint8_t> out_buf) = 0;
    virtual Size ReadRaw(const char *path, HeapArray<uint8_t> *out_blob) = 0;

    virtual Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) = 0;
    virtual bool DeleteRaw(const char *path) = 0;

    virtual bool CreateDirectory(const char *path) = 0;
    virtual bool DeleteDirectory(const char *path) = 0;

    bool PutCache(const char *key);

    virtual bool ListRaw(const char *path, FunctionRef<bool(const char *path)> func) = 0;

    virtual bool TestSlow(const char *path) = 0;

private:
    bool WriteKey(const char *path, const char *pwd, const uint8_t payload[32]);
    bool ReadKey(const char *path, const char *pwd, uint8_t *out_payload, bool *out_error);

    bool WriteSecret(const char *path, Span<const uint8_t> buf);
    bool ReadSecret(const char *path, Span<uint8_t> out_buf);

    Size WriteDirect(const char *path, Span<const uint8_t> buf);

    bool OpenCache();
    void ClearCache();
    bool RebuildCache();
};

std::unique_ptr<rk_Disk> rk_Open(const rk_Config &config, bool authenticate);

std::unique_ptr<rk_Disk> rk_OpenLocalDisk(const char *path, const char *username, const char *pwd, int threads = -1);
std::unique_ptr<rk_Disk> rk_OpenSftpDisk(const ssh_Config &config, const char *username, const char *pwd, int threads = -1);
std::unique_ptr<rk_Disk> rk_OpenS3Disk(const s3_Config &config, const char *username, const char *pwd, int threads = -1);

}
