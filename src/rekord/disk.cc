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

RG_STATIC_ASSERT(crypto_box_PUBLICKEYBYTES == 32);
RG_STATIC_ASSERT(crypto_box_SECRETKEYBYTES == 32);
RG_STATIC_ASSERT(crypto_secretstream_xchacha20poly1305_KEYBYTES == 32);

#pragma pack(push, 1)
struct ObjectIntro {
    int8_t version;
    int8_t type;
    uint8_t ekey[crypto_secretstream_xchacha20poly1305_KEYBYTES + crypto_box_SEALBYTES];
    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
};
#pragma pack(pop)

static const int CacheVersion = 2;

static const int ObjectVersion = 2;
static const Size ObjectSplit = Kibibytes(32);

bool rk_Disk::InitCache()
{
    const char *cache_dir = GetUserCachePath("rekord", &str_alloc);
    if (!MakeDirectory(cache_dir, false))
        return false;

    const char *cache_filename = Fmt(&str_alloc, "%1%/%2.db", cache_dir, FmtSpan(pkey, FmtType::SmallHex, "").Pad0(-2)).ptr;
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

                RG_STATIC_ASSERT(CacheVersion == 2);
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

bool rk_Disk::ReadObject(const rk_ID &id, rk_ObjectType *out_type, HeapArray<uint8_t> *out_obj)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::ReadWrite);

    Size prev_len = out_obj->len;
    RG_DEFER_N(err_guard) { out_obj->RemoveFrom(prev_len); };

    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "blobs/%1/%2", FmtHex(id.hash[0]).Pad0(-2), id).len;

    // Read the object, we use the same buffer for the cypher and the decrypted data,
    // just 512 bytes apart which is more than enough for ChaCha20 (64-byte blocks).
    Span<const uint8_t> obj;
    {
        out_obj->Grow(512);
        out_obj->len += 512;

        Size offset = out_obj->len;
        if (!ReadRaw(path.data, out_obj))
            return false;
        obj = MakeSpan(out_obj->ptr + offset, out_obj->len - offset);
    }

    // Init object decryption
    crypto_secretstream_xchacha20poly1305_state state;
    rk_ObjectType type;
    {
        ObjectIntro intro;
        if (obj.len < RG_SIZE(intro)) {
            LogError("Truncated object");
            return false;
        }
        memcpy(&intro, obj.ptr, RG_SIZE(intro));

        if (intro.version > ObjectVersion) {
            LogError("Unexpected object version %1 (expected %2)", intro.version, ObjectVersion);
            return false;
        }
        if (intro.type < 0 || intro.type >= RG_LEN(rk_ObjectTypeNames)) {
            LogError("Invalid object type 0x%1", FmtHex(intro.type));
            return false;
        }
        type = (rk_ObjectType)intro.type;

        uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
        if (crypto_box_seal_open(key, intro.ekey, RG_SIZE(intro.ekey), pkey, skey) != 0) {
            LogError("Failed to unseal object (wrong key?)");
            return false;
        }

        if (crypto_secretstream_xchacha20poly1305_init_pull(&state, intro.header, key) != 0) {
            LogError("Failed to initialize symmetric decryption (corrupt object?)");
            return false;
        }

        obj.ptr += RG_SIZE(ObjectIntro);
        obj.len -= RG_SIZE(ObjectIntro);
    }

    // Read and decrypt object
    Size new_len = prev_len;
    while (obj.len) {
        Size in_len = std::min(obj.len, ObjectSplit + crypto_secretstream_xchacha20poly1305_ABYTES);
        Size out_len = in_len - crypto_secretstream_xchacha20poly1305_ABYTES;

        Span<const uint8_t> cypher = MakeSpan(obj.ptr, in_len);
        uint8_t *buf = out_obj->ptr + new_len;

        unsigned long long buf_len = 0;
        uint8_t tag;
        if (crypto_secretstream_xchacha20poly1305_pull(&state, buf, &buf_len, &tag,
                                                       cypher.ptr, cypher.len, nullptr, 0) != 0) {
            LogError("Failed during symmetric decryption (corrupt object?)");
            return false;
        }

        obj.ptr += cypher.len;
        obj.len -= cypher.len;
        new_len += out_len;

        if (!obj.len) {
            if (tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
                LogError("Truncated object");
                return false;
            }
            break;
        }
    }
    out_obj->len = new_len;

    *out_type = type;
    err_guard.Disable();
    return true;
}

Size rk_Disk::WriteObject(const rk_ID &id, rk_ObjectType type, Span<const uint8_t> obj)
{
    RG_ASSERT(url);

    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "blobs/%1/%2", FmtHex(id.hash[0]).Pad0(-2), id).len;

    Size len;
    {
        Size parts = obj.len / ObjectSplit;
        Size remain = obj.len % ObjectSplit;

        len = RG_SIZE(ObjectIntro) + parts * (ObjectSplit + crypto_secretstream_xchacha20poly1305_ABYTES) +
                                     remain + crypto_secretstream_xchacha20poly1305_ABYTES;
    }

    Size written = WriteRaw(path.data, len, [&](FunctionRef<bool(Span<const uint8_t>)> func) {
        // Write object intro
        crypto_secretstream_xchacha20poly1305_state state;
        {
            ObjectIntro intro = {};

            intro.version = ObjectVersion;
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

        // Encrypt object data
        {
            bool complete = false;

            do {
                Span<const uint8_t> frag;
                frag.len = std::min(ObjectSplit, obj.len);
                frag.ptr = obj.ptr;

                complete |= (frag.len < ObjectSplit);

                uint8_t cypher[ObjectSplit + crypto_secretstream_xchacha20poly1305_ABYTES];
                unsigned char tag = complete ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
                unsigned long long cypher_len;
                crypto_secretstream_xchacha20poly1305_push(&state, cypher, &cypher_len, frag.ptr, frag.len, nullptr, 0, tag);

                if (!func(MakeSpan(cypher, (Size)cypher_len)))
                    return false;

                obj.ptr += frag.len;
                obj.len -= frag.len;
            } while (!complete);
        }

        return true;
    });

    return written;
}

bool rk_Disk::HasObject(const rk_ID &id)
{
    RG_ASSERT(url);

    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "blobs/%1/%2", FmtHex(id.hash[0]).Pad0(-2), id).len;

    return TestRaw(path.data);
}

Size rk_Disk::WriteTag(const rk_ID &id)
{
    RG_ASSERT(url);

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

        Size written = WriteRaw(path.data, cypher);

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

    RG_DEFER_NC(out_guard, len = out_ids->len) { out_ids->RemoveFrom(len); };

    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::ReadWrite);

    HeapArray<const char *> filenames;
    if (!ListRaw("tags", &temp_alloc, &filenames))
        return false;

    // List snapshots
    {
        // Reuse for performance
        HeapArray<uint8_t> obj;

        for (const char *filename: filenames) {
            obj.RemoveFrom(0);
            if (!ReadRaw(filename, &obj))
                return false;

            if (obj.len != crypto_box_SEALBYTES + 32) {
                LogError("Malformed tag file '%1' (ignoring)", filename);
                continue;
            }

            rk_ID id = {};
            if (crypto_box_seal_open(id.hash, obj.ptr, (size_t)obj.len, pkey, skey) != 0) {
                LogError("Failed to unseal tag (ignoring)");
                continue;
            }

            out_ids->Append(id);
        }
    }

    out_guard.Disable();
    return true;
}

}
