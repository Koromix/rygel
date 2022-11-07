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
RG_STATIC_ASSERT(crypto_secretbox_KEYBYTES == 32);
RG_STATIC_ASSERT(crypto_secretstream_xchacha20poly1305_KEYBYTES == 32);

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
struct ObjectIntro {
    int8_t version;
    int8_t type;
    uint8_t ekey[crypto_secretstream_xchacha20poly1305_KEYBYTES + crypto_box_SEALBYTES];
    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
};
#pragma pack(pop)

static const int SecretVersion = 1;
static const int CacheVersion = 2;
static const int ObjectVersion = 2;
static const Size ObjectSplit = Kibibytes(32);

int rk_ComputeDefaultThreads()
{
    static int threads;

    if (!threads) {
        const char *env = GetQualifiedEnv("THREADS");

        if (env) {
            char *end_ptr;
            long value = strtol(env, &end_ptr, 10);

            if (end_ptr > env && !end_ptr[0] && value > 0) {
                threads = (int)value;
            } else {
                LogError("KIPPIT_THREADS must be positive number (ignored)");
                threads = (int)std::thread::hardware_concurrency() * 4;
            }
        } else {
            threads = (int)std::thread::hardware_concurrency() * 4;
        }

        RG_ASSERT(threads > 0);
    }

    return threads;
}

bool rk_Disk::Open(const char *pwd)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);

    RG_DEFER_N(err_guard) { Close(); };

    // Open disk and determine mode
    {
        bool error = false;

        if (ReadKey("keys/write", pwd, pkey, &error)) {
            mode = rk_DiskMode::WriteOnly;
            memset(skey, 0, RG_SIZE(skey));
        } else if (ReadKey("keys/full", pwd, skey, &error)) {
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
    if (!ReadSecret("rekord", id)) {
        LogInfo("Generating new repository ID");

        randombytes_buf(id, RG_SIZE(id));

        if (!WriteSecret("rekord", id))
            return false;
    }

    // Open cache
    {
        const char *cache_dir = GetUserCachePath("rekord", &str_alloc);
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

                    RG_STATIC_ASSERT(CacheVersion == 2);
                }

                if (!cache_db.SetUserVersion(CacheVersion))
                    return false;

                return true;
            });
            if (!success)
                return false;
        }
    }

    err_guard.Disable();
    return true;
}

void rk_Disk::Close()
{
    mode = rk_DiskMode::Secure;

    ZeroMemorySafe(id, RG_SIZE(id));
    ZeroMemorySafe(pkey, RG_SIZE(pkey));
    ZeroMemorySafe(skey, RG_SIZE(skey));

    cache_db.Close();
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
    RG_ASSERT(mode == rk_DiskMode::WriteOnly || mode == rk_DiskMode::ReadWrite);

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
    RG_ASSERT(mode == rk_DiskMode::WriteOnly || mode == rk_DiskMode::ReadWrite);

    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "blobs/%1/%2", FmtHex(id.hash[0]).Pad0(-2), id).len;

    return TestFast(path.data);
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
    if (!ListRaw("tags", &temp_alloc, &filenames))
        return false;

    HeapArray<bool> ready;
    out_ids->AppendDefault(filenames.len);
    ready.AppendDefault(filenames.len);

    Async async(threads);

    // List snapshots
    for (Size i = 0; i < filenames.len; i++) {
        const char *filename = filenames[i];

        async.Run([=, &ready, this]() {
            uint8_t obj[crypto_box_SEALBYTES + 32];
            Size len = ReadRaw(filename, obj);

            if (len != crypto_box_SEALBYTES + 32) {
                if (len >= 0) {
                    LogError("Malformed tag file '%1' (ignoring)", filename);
                }
                return true;
            }

            rk_ID id = {};
            if (crypto_box_seal_open(id.hash, obj, RG_SIZE(obj), pkey, skey) != 0) {
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

bool rk_Disk::InitKeys(const char *full_pwd, const char *write_pwd)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);

    // Drop created files if anything fails
    HeapArray<const char *> names;
    RG_DEFER_N(err_guard) {
        Close();

        DeleteRaw("keys/full");
        DeleteRaw("keys/write");
    };

    if (TestSlow("keys/full")) {
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
    if (!WriteKey("keys/full", full_pwd, skey))
        return false;
    names.Append("keys/full");
    if (!WriteKey("keys/write", write_pwd, pkey))
        return false;
    names.Append("keys/write");

    // Success!
    mode = rk_DiskMode::ReadWrite;

    err_guard.Disable();
    return true;
}

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

    Size written = WriteRaw(path, buf.len, [&](FunctionRef<bool(Span<const uint8_t>)> func) { return func(buf); });
    return written;
}

}
