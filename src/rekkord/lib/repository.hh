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
#include "key.hh"

namespace RG {

struct rk_Config;
class rk_Disk;
enum class rk_WriteResult;

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

enum class rk_SaltKind {
    BlobHash = 0,
    SplitterSeed = 1
};

struct rk_TagInfo {
    const char *name;
    const char *prefix;
    rk_ObjectID oid;
    Span<const uint8_t> payload;
};

class rk_Repository {
    struct IdSet {
        uint8_t rid[16];
        uint8_t cid[16];
    };

    rk_Disk *disk;
    IdSet ids;

    rk_KeySet *keyset = nullptr;

    int compression_level;
    int64_t retain;

    Async tasks;

    std::atomic_bool cw_tested = false;
    std::mutex cw_mutex;
    bool cw_support;

    BlockAllocator str_alloc;

public:
    rk_Repository(rk_Disk *disk, const rk_Config &config);
    ~rk_Repository() { Lock(); }

    bool IsRepository();

    bool Init(Span<const uint8_t> mkey);

    bool Authenticate(const char *filename);
    bool Authenticate(Span<const uint8_t> key);
    void Lock();

    rk_Disk *GetDisk() const { return disk; }
    Async *GetAsync() { return &tasks; }

    const rk_KeySet &GetKeys() const { return *keyset; }
    const char *GetRole() const { return keyset ? rk_UserRoleNames[(int)keyset->role] : "Secure"; }
    unsigned int GetModes() const { return keyset ? keyset->modes : 0; }
    bool HasMode(rk_AccessMode mode) const { return keyset ? keyset->HasMode(mode) : false; }

    void MakeID(Span<uint8_t> out_id) const;
    void MakeSalt(rk_SaltKind kind, Span<uint8_t> out_buf) const;

    Span<const uint8_t> GetCID() const { return ids.cid; }
    bool ChangeCID();

    bool ReadBlob(const rk_ObjectID &oid, int *out_type, HeapArray<uint8_t> *out_blob, int64_t *out_size = nullptr);
    rk_WriteResult WriteBlob(const rk_ObjectID &oid, int type, Span<const uint8_t> blob, int64_t *out_size = nullptr);
    bool RetainBlob(const rk_ObjectID &oid);
    StatResult TestBlob(const rk_ObjectID &oid, int64_t *out_size = nullptr);

    bool WriteTag(const rk_ObjectID &oid, Span<const uint8_t> payload);
    bool ListTags(Allocator *alloc, HeapArray<rk_TagInfo> *out_tags);

    bool TestConditionalWrites(bool *out_cw = nullptr);

private:
    bool WriteConfig(const char *path, Span<const uint8_t> buf, bool overwrite);
    bool ReadConfig(const char *path, Span<uint8_t> out_buf);

    bool HasConditionalWrites();
};

std::unique_ptr<rk_Repository> rk_OpenRepository(rk_Disk *disk, const rk_Config &config, bool authenticate);

// Does not log anything
bool rk_ParseOID(Span<const char> str, rk_ObjectID *out_oid = nullptr);

}
