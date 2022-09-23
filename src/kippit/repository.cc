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

static const Size ChunkAverage = Kibibytes(1024);
static const Size ChunkMin = Kibibytes(512);
static const Size ChunkMax = Kibibytes(2048);

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

    StreamReader st(src_filename);
    if (!st.IsValid())
        return false;

    blake3_hasher file_hasher;
    HeapArray<uint8_t> chunk_ids;
    blake3_hasher_init_keyed(&file_hasher, salt.ptr);

    // Split the file
    std::atomic<Size> written = 0;
    {
        kt_Chunker chunker(ChunkAverage, ChunkMin, ChunkMax);

        HeapArray<uint8_t> buf;
        {
            Size capacity = (st.ComputeRawLen() >= 0) ? st.ComputeRawLen() : Mebibytes(16);
            capacity = std::clamp(capacity, Mebibytes(2), Mebibytes(128));

            buf.SetCapacity(capacity);
        }

        do {
            Async async;

            // Fill buffer
            Span<const uint8_t> read;
            read.ptr = buf.end();
            read.len = st.Read(buf.TakeAvailable());
            if (read.len < 0)
                return false;
            buf.len += read.len;

            // Update global file hash
            async.Run([&]() {
                blake3_hasher_update(&file_hasher, read.ptr, (size_t)read.len);
                return true;
            });

            Span<const uint8_t> remain = buf;

            // We can't relocate in the inner loop
            chunk_ids.Grow((remain.len / ChunkMin + 1) * RG_SIZE(kt_ID));

            do {
                Size processed = chunker.Process(remain, st.IsEOF(), [&](Size idx, Size total, Span<const uint8_t> chunk) {
                    RG_ASSERT(idx * 32 == chunk_ids.len);
                    chunk_ids.len += 32;

                    async.Run([=, &written, &chunk_ids]() {
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

                        memcpy(chunk_ids.ptr + idx * RG_SIZE(id), &id, RG_SIZE(id));

                        return true;
                    });

                    return true;
                });
                if (processed < 0)
                    return false;
                if (!processed)
                    break;

                remain.ptr += processed;
                remain.len -= processed;
            } while (remain.len);

            if (!async.Sync())
                return false;

            memmove_safe(buf.ptr, remain.ptr, remain.len);
            buf.len = remain.len;
        } while (buf.len);

        RG_ASSERT(st.IsEOF());
    }

    // Write list of chunks
    kt_ID file_id = {};
    {
        blake3_hasher_finalize(&file_hasher, file_id.hash, RG_SIZE(file_id.hash));

        Size ret = disk->Write(file_id, chunk_ids);
        if (ret < 0)
            return false;
        written += ret;
    }

    *out_id = file_id;
    if (out_written) {
        *out_written = written;
    }
    return true;
}

}
