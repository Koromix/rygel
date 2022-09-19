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

static const int MaxPathSize = 4096 - 128;

#pragma pack(push, 1)
struct ChunkIntro {
    int8_t version;
    uint8_t ekey[crypto_secretstream_xchacha20poly1305_KEYBYTES + crypto_box_SEALBYTES];
    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
};
#pragma pack(pop)
#define CHUNK_VERSION 1
#define CHUNK_SPLIT Kibibytes(8)

class LocalDisk: public kt_Disk {
    LocalArray<char, MaxPathSize + 128> directory;

    uint8_t pkey[crypto_box_PUBLICKEYBYTES];
    uint8_t skey[crypto_box_SECRETKEYBYTES];

public:
    LocalDisk(Span<const char> directory, kt_DiskMode mode,
              uint8_t skey[crypto_box_SECRETKEYBYTES], uint8_t pkey[crypto_box_PUBLICKEYBYTES]);
    ~LocalDisk() override;

    bool ListTags(Allocator *alloc, HeapArray<const char *> *out_tags) override;

    bool ListChunks(const char *type, HeapArray<kt_ID> *out_ids) override;
    bool ReadChunk(const kt_ID &id, HeapArray<uint8_t> *out_buf) override;
    Size WriteChunk(const kt_ID &id, Span<const uint8_t> chunk) override;
};

LocalDisk::LocalDisk(Span<const char> directory, kt_DiskMode mode,
                     uint8_t skey[crypto_box_SECRETKEYBYTES], uint8_t pkey[crypto_box_PUBLICKEYBYTES])
    : kt_Disk(mode)
{
    RG_ASSERT(directory.len <= MaxPathSize);

    this->directory.Append(directory);
    this->directory.data[this->directory.len] = 0;

    memcpy(this->skey, skey, RG_SIZE(this->skey));
    memcpy(this->pkey, pkey, RG_SIZE(this->pkey));
}

LocalDisk::~LocalDisk()
{
}

bool LocalDisk::ListTags(Allocator *alloc, HeapArray<const char *> *out_tags)
{
    RG_UNREACHABLE();
}

bool LocalDisk::ListChunks(const char *type, HeapArray<kt_ID> *out_ids)
{
    RG_UNREACHABLE();
}

bool LocalDisk::ReadChunk(const kt_ID &id, HeapArray<uint8_t> *out_buf)
{
    RG_ASSERT(GetMode() == kt_DiskMode::ReadWrite);

    RG_DEFER_NC(err_guard, len = out_buf->len) { out_buf->RemoveFrom(len); };

    LocalArray<char, MaxPathSize + 128> path;
    path.len = Fmt(path.data, "%1%/chunks%/%2%/%3", directory, FmtHex(id.hash[0]).Pad0(-2), id).len;

    StreamReader reader(path.data);

    // Read chunk header
    ChunkIntro intro;
    if (reader.Read(RG_SIZE(intro), &intro) != RG_SIZE(intro)) {
        if (reader.IsValid()) {
            LogError("Truncated chunk");
        }
        return false;
    }

    // Check signature
    if (intro.version != CHUNK_VERSION) {
        LogError("Unexpected chunk version %1 (expected %2)", intro.version, CHUNK_VERSION);
        return false;
    }

    // Decrypt symmetric key
    uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    if (crypto_box_seal_open(key, intro.ekey, RG_SIZE(intro.ekey), pkey, skey) != 0) {
        LogError("Failed to unseal chunk (wrong key?)");
        return false;
    }

    // Init symmetric decryption
    crypto_secretstream_xchacha20poly1305_state state;
    if (crypto_secretstream_xchacha20poly1305_init_pull(&state, intro.header, key) != 0) {
        LogError("Failed to initialize symmetric decryption (corrupt chunk?)");
        return false;
    }

    // Read and decrypt chunk
    for (;;) {
        LocalArray<uint8_t, CHUNK_SPLIT + crypto_secretstream_xchacha20poly1305_ABYTES> cypher;
        cypher.len = reader.Read(cypher.data);
        if (cypher.len < 0)
            return false;

        uint8_t buf[CHUNK_SPLIT];
        unsigned long long buf_len = 0;
        uint8_t tag;
        if (crypto_secretstream_xchacha20poly1305_pull(&state, buf, &buf_len, &tag,
                                                       cypher.data, cypher.len, nullptr, 0) != 0) {
            LogError("Failed during symmetric decryption (corrupt chunk?)");
            return false;
        }

        out_buf->Append(MakeSpan(buf, (Size)buf_len));

        if (reader.IsEOF()) {
            if (tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
                LogError("Truncated chunk");
                return false;
            }
            break;
        }
    }

    err_guard.Disable();
    return true;
}

Size LocalDisk::WriteChunk(const kt_ID &id, Span<const uint8_t> chunk)
{
    // Open destination file
    FILE *fp;
    LocalArray<char, MaxPathSize + 128> path;
    {
        path.len += Fmt(path.TakeAvailable(), "%1%/chunks%/%2", directory, FmtHex(id.hash[0]).Pad0(-2)).len;
        if (!MakeDirectory(path.data, false))
            return -1;
        path.len += Fmt(path.TakeAvailable(), "%/%1", id).len;

        bool exists = false;
        fp = OpenFile(path.data, (int)OpenFlag::Write | (int)OpenFlag::Exclusive, &exists);

        if (!fp)
            return exists ? 0 : -1;
    }
    RG_DEFER { fclose(fp); };

    StreamWriter writer(fp, path.data);
    crypto_secretstream_xchacha20poly1305_state state;

    // Write chunk intro
    {
        ChunkIntro intro = {};

        intro.version = CHUNK_VERSION;

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

        if (!writer.Write(&intro, RG_SIZE(intro)))
            return false;
    }

    // Encrypt chunk data
    {
        bool complete = false;

        do {
            Span<const uint8_t> frag;
            frag.len = std::min(CHUNK_SPLIT, chunk.len);
            frag.ptr = chunk.ptr;

            complete |= (frag.len < CHUNK_SPLIT);

            uint8_t cypher[CHUNK_SPLIT + crypto_secretstream_xchacha20poly1305_ABYTES];
            unsigned char tag = complete ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
            unsigned long long cypher_len;
            crypto_secretstream_xchacha20poly1305_push(&state, cypher, &cypher_len, frag.ptr, frag.len, nullptr, 0, tag);

            if (!writer.Write(cypher, (Size)cypher_len))
                return -1;

            chunk.ptr += frag.len;
            chunk.len -= frag.len;
        } while (!complete);
    }

    if (!writer.Close())
        return -1;

    return writer.GetRawWritten();
}

static bool ParseKey(const char *password, uint8_t out_key[32])
{
    size_t key_len;
    int ret = sodium_base642bin(out_key, 32, password, strlen(password), nullptr, &key_len,
                                nullptr, sodium_base64_VARIANT_ORIGINAL);
    if (ret || key_len != 32) {
        LogError("Malformed repository key");
        return false;
    }

    return true;
}

bool kt_CreateLocalDisk(const char *path, const char *password)
{
    BlockAllocator temp_alloc;

    uint8_t key[32];
    if (!ParseKey(password, key))
        return false;

    Span<const char> directory = TrimStrRight(path, RG_PATH_SEPARATORS);

    if (TestFile(path, FileType::Directory)) {
        LogError("Directory '%1' already exists", directory);
        return false;
    }
    if (directory.len > MaxPathSize) {
        LogError("Directory path '%1' is too long", directory);
        return false;
    }

    // Drop created files and directories if anything fails
    HeapArray<const char *> directories;
    HeapArray<const char *> files;
    RG_DEFER_N(root_guard) {
        for (const char *filename: files) {
            UnlinkFile(filename);
        }
        for (Size i = directories.len - 1; i >= 0; i--) {
            UnlinkDirectory(directories[i]);
        }
    };

    // Create repository directories
    {
        const auto make_directory = [&](const char *suffix) {
            const char *path = Fmt(&temp_alloc, "%1%2", directory, suffix).ptr;

            if (!MakeDirectory(path))
                return false;
            directories.Append(path);

            return true;
        };

        if (!make_directory(""))
            return false;
        if (!make_directory("/chunks"))
            return false;
        if (!make_directory("/info"))
            return false;
    }

    // Write control file
    {
        uint8_t version = CHUNK_VERSION;

        const char *version_filename = Fmt(&temp_alloc, "%1%/info/version", directory).ptr;

        uint8_t pkey[crypto_box_PUBLICKEYBYTES];
        uint8_t cypher[crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + 1];

        crypto_scalarmult_base(pkey, key);
        randombytes_buf(cypher, crypto_secretbox_NONCEBYTES);
        crypto_secretbox_easy(cypher + crypto_secretbox_NONCEBYTES, &version, RG_SIZE(version), cypher, pkey);

        if (!WriteFile(cypher, version_filename))
            return false;
        files.Append(version_filename);
    }

    root_guard.Disable();
    return true;
}

kt_Disk *kt_OpenLocalDisk(const char *path, const char *password)
{
    BlockAllocator temp_alloc;

    uint8_t key[32];
    if (!ParseKey(password, key))
        return nullptr;

    Span<const char> directory = TrimStrRight(path, RG_PATH_SEPARATORS);

    if (!TestFile(path, FileType::Directory)) {
        LogError("Directory '%1' does not exist", directory);
        return nullptr;
    }
    if (directory.len > MaxPathSize) {
        LogError("Directory path '%1' is too long", directory);
        return nullptr;
    }

    uint8_t cypher[crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + 1];
    {
        const char *version_filename = Fmt(&temp_alloc, "%1%/info/version", directory).ptr;

        Size read = ReadFile(version_filename, cypher);
        if (read < 0)
            return nullptr;
        if (read < RG_SIZE(cypher)) {
            LogError("Truncated version file");
            return nullptr;
        }
    }

    // Open disk and determine mode
    kt_DiskMode mode;
    uint8_t skey[crypto_box_SECRETKEYBYTES];
    uint8_t pkey[crypto_box_PUBLICKEYBYTES];
    {
        mode = kt_DiskMode::WriteOnly;
        memcpy(pkey, key, RG_SIZE(pkey));

        uint8_t version = 0;

        if (crypto_secretbox_open_easy(&version, cypher + crypto_secretbox_NONCEBYTES,
                                       RG_SIZE(cypher) - crypto_secretbox_NONCEBYTES, cypher, pkey) != 0) {
            mode = kt_DiskMode::ReadWrite;
            memcpy(skey, key, RG_SIZE(pkey));
            crypto_scalarmult_base(pkey, key);

            if (crypto_secretbox_open_easy(&version, cypher + crypto_secretbox_NONCEBYTES,
                                           RG_SIZE(cypher) - crypto_secretbox_NONCEBYTES, cypher, pkey) != 0) {
                LogError("Failed to open repository (wrong key?)");
                return nullptr;
            }
        }

        if (version != CHUNK_VERSION) {
            LogError("Unexpected repository version %1 (expected %2)", version, CHUNK_VERSION);
            return nullptr;
        }
    }

    kt_Disk *disk = new LocalDisk(directory, mode, skey, pkey);
    return disk;
}

}
