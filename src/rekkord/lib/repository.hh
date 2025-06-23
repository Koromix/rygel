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

#pragma once

#include "src/core/base/base.hh"

namespace RG {

struct rk_Config;
class rk_Disk;
enum class rk_WriteResult;

static const int rk_MasterKeySize = 32;

struct rk_Hash {
    uint8_t raw[32];

    bool operator==(const rk_Hash &other) const { return memcmp(raw, other.raw, RG_SIZE(raw)) == 0; }
    bool operator!=(const rk_Hash &other) const { return memcmp(raw, other.raw, RG_SIZE(raw)) != 0; }

    int operator-(const rk_Hash &other) const { return memcmp(raw, other.raw, RG_SIZE(raw)); }

    operator FmtArg() const { return FmtSpan(raw, FmtType::BigHex, "").Pad0(-2); }

    uint64_t Hash() const
    {
        uint64_t hash;
        MemCpy(&hash, raw, RG_SIZE(hash));
        return hash;
    }
};
static_assert(RG_SIZE(rk_Hash) == 32);

enum class rk_BlobCatalog: int8_t {
    Meta,
    Raw
};
static const char rk_BlobCatalogNames[] = {
    'M',
    'R'
};

struct rk_ObjectID {
    rk_BlobCatalog catalog;
    rk_Hash hash;

    bool IsValid() const
    {
        if ((int)catalog < 0)
            return false; 
        if ((int)catalog >= RG_LEN(rk_BlobCatalogNames))
            return false;

        return true;
    }

    bool operator==(const rk_ObjectID &other) const { return catalog == other.catalog && hash == other.hash; }
    bool operator!=(const rk_ObjectID &other) const { return catalog != other.catalog || hash != other.hash; }

    int operator-(const rk_ObjectID &other) const { return MultiCmp((int)catalog - (int)other.catalog, hash - other.hash); }

    void Format(FunctionRef<void(Span<const char>)> append) const
    {
        FmtArg arg = FmtSpan(hash.raw, FmtType::BigHex, "").Pad0(-2);
        Fmt(append, "%1%2", rk_BlobCatalogNames[(int)catalog], arg);
    }
    operator FmtArg() { return FmtCustom(*this); }

    uint64_t Hash() const { return hash.Hash(); }

    Span<const uint8_t> Raw() const { return MakeSpan((const uint8_t *)this, RG_SIZE(*this)); }
};
static_assert(RG_SIZE(rk_ObjectID) == 33);

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
    const char *name;
    const char *prefix;
    rk_ObjectID oid;
    Span<const uint8_t> payload;
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

class rk_Repository {
    struct IdSet {
        uint8_t rid[16];
        uint8_t cid[16];
    };

    rk_Disk *disk;
    IdSet ids;

    unsigned int modes = 0;
    const char *user = nullptr;
    const char *role = "Secure";
    rk_KeySet *keyset = nullptr;

    int compression_level;
    int64_t retain;

    Async tasks;

    BlockAllocator str_alloc;

public:
    rk_Repository(rk_Disk *disk, const rk_Config &config);
    ~rk_Repository() { Lock(); }

    bool IsRepository();

    bool Init(Span<const uint8_t> mkey, Span<const rk_UserInfo> users);

    bool Authenticate(const char *username, const char *pwd);
    bool Authenticate(Span<const uint8_t> mkey);
    void Lock();

    rk_Disk *GetDisk() const { return disk; }
    const char *GetUser() const { return user; } // Can be NULL
    const char *GetRole() const { return role; }
    Async *GetAsync() { return &tasks; }

    // You should not need this, but if you do, be careful!
    const rk_KeySet *GetKeySet() const { return keyset; }

    unsigned int GetModes() const { return modes; }
    bool HasMode(rk_AccessMode mode) const { return modes & (int)mode; }

    void MakeID(Span<uint8_t> out_id) const;
    void MakeSalt(rk_SaltKind kind, Span<uint8_t> out_buf) const;

    Span<const uint8_t> GetCID() const { return ids.cid; }
    bool ChangeCID();

    bool InitUser(const char *username, rk_UserRole role, const char *pwd, bool force);
    bool DeleteUser(const char *username);
    bool ListUsers(Allocator *alloc, bool verify, HeapArray<rk_UserInfo> *out_users);

    bool ReadBlob(const rk_ObjectID &oid, int *out_type, HeapArray<uint8_t> *out_blob);
    rk_WriteResult WriteBlob(const rk_ObjectID &oid, int type, Span<const uint8_t> blob, int64_t *out_size = nullptr);
    bool RetainBlob(const rk_ObjectID &oid);
    StatResult TestBlob(const rk_ObjectID &oid, int64_t *out_size = nullptr);

    bool WriteTag(const rk_ObjectID &oid, Span<const uint8_t> payload);
    bool ListTags(Allocator *alloc, HeapArray<rk_TagInfo> *out_tags);

private:
    bool WriteKeys(const char *path, const char *pwd, rk_UserRole role, const rk_KeySet &keys);
    bool ReadKeys(const char *path, const char *pwd, rk_UserRole *out_role, rk_KeySet *out_keys);

    bool WriteConfig(const char *path, Span<const uint8_t> buf, bool overwrite);
    bool ReadConfig(const char *path, Span<uint8_t> out_buf);

    bool CheckRepository();
};

std::unique_ptr<rk_Repository> rk_OpenRepository(rk_Disk *disk, const rk_Config &config, bool authenticate);

// Does not log anything
bool rk_ParseOID(Span<const char> str, rk_ObjectID *out_oid);

}
