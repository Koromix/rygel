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
#include "chunker.hh"
#include "disk.hh"
#include "repository.hh"
#include "vendor/blake3/c/blake3.h"

namespace RG {

static const Size ChunkAverage = Kibibytes(512);
static const Size ChunkMin = Kibibytes(256);
static const Size ChunkMax = Kibibytes(1280);

bool kt_ExtractFile(kt_Disk *disk, const kt_ID &id, const char *dest_filename, Size *out_len)
{
    // Open destination file
    StreamWriter writer(dest_filename);
    if (!writer.IsValid())
        return false;

    // Read file summary
    HeapArray<uint8_t> summary;
    {
        if (!disk->Read(id, &summary))
            return false;
        if (summary.len % RG_SIZE(kt_ID)) {
            LogError("Malformed file summary '%1'", id);
            return false;
        }
    }

    // Write unencrypted file
    for (Size idx = 0, offset = 0; offset < summary.len; idx++, offset += RG_SIZE(kt_ID)) {
        kt_ID id = {};
        memcpy(&id, summary.ptr + offset, RG_SIZE(id));

        HeapArray<uint8_t> buf;
        if (!disk->Read(id, &buf))
            return false;
        if (!writer.Write(buf))
            return false;
    }

    if (!writer.Close())
        return false;

    if (out_len) {
        *out_len = writer.GetRawWritten();
    }
    return true;
}

bool kt_BackupFile(kt_Disk *disk, const char *src_filename, kt_ID *out_id, Size *out_written)
{
    Span<const uint8_t> salt = disk->GetSalt();
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes

    blake3_hasher file_hasher;
    blake3_hasher_init_keyed(&file_hasher, salt.ptr);

    // Split the file
    HeapArray<uint8_t> summary;
    Size written = 0;
    {
        StreamReader st(src_filename);

        kt_Chunker chunker(ChunkAverage, ChunkMin, ChunkMax);
        HeapArray<uint8_t> buf;

        do {
            Size processed = 0;

            do {
                buf.Grow(Mebibytes(1));

                Size read = st.Read(buf.TakeAvailable());
                if (read < 0)
                    return false;
                buf.len += read;

                processed = chunker.Process(buf, st.IsEOF(), [&](Size idx, Size total, Span<const uint8_t> chunk) {
                    kt_ID id = {};
                    {
                        blake3_hasher hasher;
                        blake3_hasher_init_keyed(&hasher, salt.ptr);
                        blake3_hasher_update(&hasher, chunk.ptr, chunk.len);
                        blake3_hasher_finalize(&hasher, id.hash, RG_SIZE(id.hash));
                    }

                    Size ret = disk->Write(id, chunk);
                    if (ret < 0)
                        return false;
                    written += ret;

                    summary.Append(MakeSpan((const uint8_t *)&id, RG_SIZE(id)));
                    blake3_hasher_update(&file_hasher, chunk.ptr, (size_t)chunk.len);

                    return true;
                });
                if (processed < 0)
                    return false;
            } while (!processed);

            memmove_safe(buf.ptr, buf.ptr + processed, buf.len - processed);
            buf.len -= processed;
        } while (!st.IsEOF());
    }

    // Write list of chunks
    kt_ID id = {};
    {
        blake3_hasher_finalize(&file_hasher, id.hash, RG_SIZE(id.hash));

        Size ret = disk->Write(id, summary);
        if (ret < 0)
            return false;
        written += ret;
    }

    *out_id = id;
    if (out_written) {
        *out_written = written;
    }
    return true;
}

}
