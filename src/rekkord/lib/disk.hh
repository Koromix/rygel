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

#pragma once

#include "src/core/base/base.hh"
#include "src/core/sqlite/sqlite.hh"

namespace RG {

struct rk_Config;
struct s3_Config;
struct ssh_Config;

static const int rk_MasterKeySize = 32;

struct rk_Hash {
    uint8_t hash[32];

    operator FmtArg() const { return FmtSpan(hash, FmtType::BigHex, "").Pad0(-2); }

    int operator-(const rk_Hash &other) const
    {
        for (Size i = 0; i < RG_SIZE(hash); i++) {
            int delta = hash[i] - other.hash[i];

            if (delta)
                return delta;
        }

        return 0;
    }
};
static_assert(RG_SIZE(rk_Hash) == 32);

enum class rk_AccessMode {
    Config = 1 << 0,
    Read = 1 << 1,
    Write = 1 << 2,
    Log = 1 << 3
};
static const char *const rk_AccessModeNames[] = {
    "Config",
    "Read",
    "Write",
    "Log"
};

enum class rk_BlobType: int8_t {
    Chunk = 0,
    File = 1,
    Directory1 = 2,
    Snapshot1 = 3,
    Link = 4,
    Snapshot2 = 5,
    Directory2 = 6,
    Snapshot3 = 7
};
static const char *const rk_BlobTypeNames[] = {
    "Chunk",
    "File",
    "Directory1",
    "Snapshot1",
    "Link",
    "Snapshot2",
    "Directory2",
    "Snapshot3"
};

enum class rk_SaltKind {
    BlobHash = 0,
    SplitterSeed = 1
};

enum class rk_UserRole {
    Admin = 0,
    WriteOnly = 1,
    ReadWrite = 2,
    LogOnly = 3
};
static const char *const rk_UserRoleNames[] = {
    "Admin",
    "WriteOnly",
    "ReadWrite",
    "LogOnly"
};

struct rk_UserInfo {
    const char *username;
    rk_UserRole role;
    const char *pwd; // NULL unless used to create users
};

struct rk_TagInfo {
    const char *id;
    rk_Hash hash;
    Span<const uint8_t> payload;
};

struct rk_OpenSettings {
    int threads = -1;
    int compression_level = 4;
};

struct rk_KeySet {
    uint8_t ckey[32];
    uint8_t akey[32];
    uint8_t dkey[32];
    uint8_t wkey[32];
    uint8_t lkey[32];
    uint8_t tkey[32];
    uint8_t skey[32];
    uint8_t vkey[32];
};

class rk_Disk {
    struct IdSet {
        uint8_t rid[16];
        uint8_t cid[16];
    };

protected:
    const char *url = nullptr;
    IdSet ids;

    unsigned int modes = 0;
    const char *user = nullptr;
    const char *role = "Secure";
    rk_KeySet *keyset = nullptr;

    sq_Database cache_db;
    std::mutex cache_mutex;
    int cache_misses = 0;
    int compression_level = 0;

    Async tasks;

    BlockAllocator str_alloc;

public:
    rk_Disk(const rk_OpenSettings &settings, int default_threads)
        : compression_level(settings.compression_level),
          tasks(settings.threads > 0 ? settings.threads : default_threads) {}

    virtual ~rk_Disk();

    virtual bool Init(Span<const uint8_t> mkey, Span<const rk_UserInfo> users) = 0;

    bool Authenticate(const char *username, const char *pwd);
    bool Authenticate(Span<const uint8_t> mkey);
    void Lock();

    const char *GetURL() const { return url; }
    const char *GetUser() const { return user; } // Can be NULL
    const char *GetRole() const { return role; }
    Async *GetAsync() { return &tasks; }

    bool HasMode(rk_AccessMode mode) const { return modes & (int)mode; }

    void MakeSalt(rk_SaltKind kind, Span<uint8_t> out_buf) const;

    bool ChangeCID();

    sq_Database *OpenCache(bool build);
    bool RebuildCache();

    bool InitUser(const char *username, rk_UserRole role, const char *pwd, bool force);
    bool DeleteUser(const char *username);
    bool ListUsers(Allocator *alloc, bool verify, HeapArray<rk_UserInfo> *out_users);

    bool ReadBlob(const rk_Hash &hash, rk_BlobType *out_type, HeapArray<uint8_t> *out_blob);
    Size WriteBlob(const rk_Hash &hash, rk_BlobType type, Span<const uint8_t> blob);

    bool WriteTag(const rk_Hash &hash, Span<const uint8_t> payload);
    bool ListTags(Allocator *alloc, HeapArray<rk_TagInfo> *out_tags);

    virtual Size ReadRaw(const char *path, Span<uint8_t> out_buf) = 0;
    virtual Size ReadRaw(const char *path, HeapArray<uint8_t> *out_blob) = 0;

    virtual Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) = 0;
    virtual bool DeleteRaw(const char *path) = 0;

    virtual bool ListRaw(const char *path, FunctionRef<bool(const char *path)> func) = 0;
    virtual StatResult TestRaw(const char *path) = 0;

protected:
    virtual bool CreateDirectory(const char *path) = 0;
    virtual bool DeleteDirectory(const char *path) = 0;

    bool InitDefault(Span<const uint8_t> mkey, Span<const rk_UserInfo> users);

    bool PutCache(const char *key);

private:
    StatResult TestFast(const char *path);

    bool WriteKeys(const char *path, const char *pwd, rk_UserRole role, const rk_KeySet &keys);
    bool ReadKeys(const char *path, const char *pwd, rk_UserRole *out_role, rk_KeySet *out_keys);

    bool WriteConfig(const char *path, Span<const uint8_t> buf, bool overwrite);
    bool ReadConfig(const char *path, Span<uint8_t> out_buf);

    Size WriteDirect(const char *path, Span<const uint8_t> buf, bool overwrite);

    bool CheckRepository();

    void ClearCache();
};

std::unique_ptr<rk_Disk> rk_Open(const rk_Config &config, bool authenticate);

std::unique_ptr<rk_Disk> rk_OpenLocalDisk(const char *path, const char *username, const char *pwd, const rk_OpenSettings &settings);
std::unique_ptr<rk_Disk> rk_OpenSftpDisk(const ssh_Config &config, const char *username, const char *pwd, const rk_OpenSettings &settings);
std::unique_ptr<rk_Disk> rk_OpenS3Disk(const s3_Config &config, const char *username, const char *pwd, const rk_OpenSettings &settings);

}
