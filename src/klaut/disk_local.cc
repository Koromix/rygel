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

class LocalDisk: public kt_Disk {
    LocalArray<char, MaxPathSize + 128> directory;
    uint8_t pkey[crypto_box_PUBLICKEYBYTES];

public:
    LocalDisk(Span<const char> directory, uint8_t pkey[crypto_box_PUBLICKEYBYTES]);
    ~LocalDisk() override;

    bool ListTags(Allocator *alloc, HeapArray<const char *> *out_tags) override;

    bool ListChunks(const char *type, HeapArray<kt_Hash> *out_ids) override;
    bool ReadChunk(const kt_Hash &id, HeapArray<uint8_t> *out_buf) override;
    Size WriteChunk(const kt_Hash &id, Span<const uint8_t> chunk) override;
};

LocalDisk::LocalDisk(Span<const char> directory, uint8_t pkey[crypto_box_PUBLICKEYBYTES])
{
    RG_ASSERT(directory.len <= MaxPathSize);

    this->directory.Append(directory);
    this->directory.data[this->directory.len] = 0;
    memcpy(this->pkey, pkey, RG_SIZE(this->pkey));
}

LocalDisk::~LocalDisk()
{
}

bool LocalDisk::ListTags(Allocator *alloc, HeapArray<const char *> *out_tags)
{
    RG_UNREACHABLE();
}

bool LocalDisk::ListChunks(const char *type, HeapArray<kt_Hash> *out_ids)
{
    RG_UNREACHABLE();
}

bool LocalDisk::ReadChunk(const kt_Hash &id, HeapArray<uint8_t> *out_buf)
{
    RG_UNREACHABLE();
}

Size LocalDisk::WriteChunk(const kt_Hash &id, Span<const uint8_t> chunk)
{
    // Open destination file
    FILE *fp;
    LocalArray<char, MaxPathSize + 128> path;
    {
        path.len += Fmt(path.TakeAvailable(), "%1%/%2", directory, FmtHex(id.hash[0]).Pad0(-2)).len;
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

    // Prepare random symetric key
    uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    crypto_secretstream_xchacha20poly1305_keygen(key);

    // Encrypt and write the symetric key
    {
        uint8_t cipher[RG_SIZE(key) + crypto_box_SEALBYTES];
        crypto_box_seal(cipher, key, RG_SIZE(key), pkey);

        if (!writer.Write(cipher))
            return -1;
    }

    // Init symetric encryption
    crypto_secretstream_xchacha20poly1305_state state;
    {
        uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
        crypto_secretstream_xchacha20poly1305_init_push(&state, header, key);

        if (!writer.Write(header))
            return -1;
    }

    // Encrypt chunk data
    while (chunk.len) {
        uint8_t cipher[Kibibytes(16) + crypto_secretstream_xchacha20poly1305_ABYTES];

        Span<const uint8_t> buf;
        buf.len = std::min(Kibibytes(16), chunk.len);
        buf.ptr = chunk.ptr;

        chunk.ptr += buf.len;
        chunk.len -= buf.len;

        unsigned char tag = buf.len ? 0 : crypto_secretstream_xchacha20poly1305_TAG_FINAL;
        crypto_secretstream_xchacha20poly1305_push(&state, cipher, nullptr, buf.ptr, buf.len, nullptr, 0, tag);

        if (!writer.Write(cipher, buf.len + crypto_secretstream_xchacha20poly1305_ABYTES))
            return -1;
    }

    if (!writer.Close())
        return -1;

    return writer.GetRawWritten();
}

kt_Disk *kt_OpenLocalDisk(const char *path, const char *encrypt_key)
{
    Span<const char> directory = TrimStrRight(path, RG_PATH_SEPARATORS);

    if (!TestFile(path, FileType::Directory)) {
        LogError("Directory '%1' does not exist", directory);
        return nullptr;
    }
    if (directory.len > MaxPathSize) {
        LogError("Directory path '%1' is too long", directory);
        return nullptr;
    }

    // Derive asymmetric keys
    uint8_t pkey[crypto_box_PUBLICKEYBYTES];
    {
        RG_STATIC_ASSERT(crypto_scalarmult_BYTES == crypto_box_PUBLICKEYBYTES);

        size_t key_len;
        int ret = sodium_base642bin(pkey, RG_SIZE(pkey), encrypt_key, strlen(encrypt_key), nullptr, &key_len,
                                    nullptr, sodium_base64_VARIANT_ORIGINAL);
        if (ret || key_len != 32) {
            LogError("Malformed encryption key");
            return nullptr;
        }
    }

    kt_Disk *disk = new LocalDisk(directory, pkey);
    return disk;
}

}
