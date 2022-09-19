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

#pragma pack(push, 1)
struct ChunkIntro {
    int8_t version;
    uint8_t ekey[crypto_secretstream_xchacha20poly1305_KEYBYTES + crypto_box_SEALBYTES];
    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
};
#pragma pack(pop)
#define CHUNK_VERSION 1
#define CHUNK_SPLIT Kibibytes(8)

kt_Disk::kt_Disk(kt_DiskMode mode, uint8_t skey[32], uint8_t pkey[32])
    : mode(mode)
{
    memcpy(this->skey, skey, RG_SIZE(this->skey));
    memcpy(this->pkey, pkey, RG_SIZE(this->pkey));
}

bool kt_Disk::ReadChunk(const kt_ID &id, HeapArray<uint8_t> *out_chunk)
{
    RG_ASSERT(mode == kt_DiskMode::ReadWrite);

    Size prev_len = out_chunk->len;
    RG_DEFER_N(err_guard) { out_chunk->RemoveFrom(prev_len); };

    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "chunks%/%1%/%2", FmtHex(id.hash[0]).Pad0(-2), id).len;

    // Read the blob, we use the same buffer for the cypher and the decrypted data,
    // just 512 bytes apart which is more than enough for ChaCha20 (64-byte blocks).
    Span<const uint8_t> blob;
    {
        out_chunk->Grow(512);
        out_chunk->len += 512;

        Size offset = out_chunk->len;
        if (!ReadBlob(path.data, out_chunk))
            return false;
        blob = MakeSpan(out_chunk->ptr + offset, out_chunk->len - offset);
    }

    // Init chunk decryption
    crypto_secretstream_xchacha20poly1305_state state;
    {
        ChunkIntro intro;
        if (blob.len < RG_SIZE(intro)) {
            LogError("Truncated chunk");
            return false;
        }
        memcpy(&intro, blob.ptr, RG_SIZE(intro));

        if (intro.version != CHUNK_VERSION) {
            LogError("Unexpected chunk version %1 (expected %2)", intro.version, CHUNK_VERSION);
            return false;
        }

        uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
        if (crypto_box_seal_open(key, intro.ekey, RG_SIZE(intro.ekey), pkey, skey) != 0) {
            LogError("Failed to unseal chunk (wrong key?)");
            return false;
        }

        if (crypto_secretstream_xchacha20poly1305_init_pull(&state, intro.header, key) != 0) {
            LogError("Failed to initialize symmetric decryption (corrupt chunk?)");
            return false;
        }

        blob.ptr += RG_SIZE(ChunkIntro);
        blob.len -= RG_SIZE(ChunkIntro);
    }

    // Read and decrypt chunk
    Size new_len = prev_len;
    while (blob.len) {
        Size in_len = std::min(blob.len, (Size)CHUNK_SPLIT + crypto_secretstream_xchacha20poly1305_ABYTES);
        Size out_len = in_len - crypto_secretstream_xchacha20poly1305_ABYTES;

        Span<const uint8_t> cypher = MakeSpan(blob.ptr, in_len);
        uint8_t *buf = out_chunk->ptr + new_len;

        unsigned long long buf_len = 0;
        uint8_t tag;
        if (crypto_secretstream_xchacha20poly1305_pull(&state, buf, &buf_len, &tag,
                                                       cypher.ptr, cypher.len, nullptr, 0) != 0) {
            LogError("Failed during symmetric decryption (corrupt chunk?)");
            return false;
        }

        blob.ptr += cypher.len;
        blob.len -= cypher.len;
        new_len += out_len;

        if (!blob.len) {
            if (tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
                LogError("Truncated chunk");
                return false;
            }
            break;
        }
    }
    out_chunk->len = new_len;

    err_guard.Disable();
    return true;
}

Size kt_Disk::WriteChunk(const kt_ID &id, Span<const uint8_t> chunk)
{
    LocalArray<char, 256> path;
    path.len = Fmt(path.data, "chunks%/%1%/%2", FmtHex(id.hash[0]).Pad0(-2), id).len;

    Size written = WriteBlob(path.data, [&](FunctionRef<bool(Span<const uint8_t>)> func) {
        // Write chunk intro
        crypto_secretstream_xchacha20poly1305_state state;
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

            Span<const uint8_t> buf = MakeSpan((const uint8_t *)&intro, RG_SIZE(intro));
            if (!func(buf))
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

                if (!func(MakeSpan(cypher, (Size)cypher_len)))
                    return false;

                chunk.ptr += frag.len;
                chunk.len -= frag.len;
            } while (!complete);
        }

        return true;
    });

    return written;
}

}
