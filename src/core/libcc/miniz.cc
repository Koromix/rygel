// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in 
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "libcc.hh"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vendor/miniz/miniz.h"

namespace RG {

class MinizDecompressor: public StreamDecompressor {
    tinfl_decompressor inflator;
    bool done = false;

    uint8_t in_buf[256 * 1024];
    uint8_t *in_ptr = nullptr;
    Size in_len = 0;

    uint8_t out_buf[256 * 1024];
    uint8_t *out_ptr = nullptr;
    Size out_len = 0;

    // Gzip support
    bool is_gzip = false;
    bool header_done = false;
    uint32_t crc32 = MZ_CRC32_INIT;
    Size uncompressed_size = 0;

public:
    MinizDecompressor(StreamReader *reader) : StreamDecompressor(reader) {}
    ~MinizDecompressor() {}

    bool Init() override;
    void Reset() override;
    Size Read(Size max_len, void *out_buf) override;
};

bool MinizDecompressor::Init()
{
    RG_STATIC_ASSERT(RG_SIZE(out_buf) >= TINFL_LZ_DICT_SIZE);

    tinfl_init(&inflator);
    is_gzip = (GetCompressionType() == CompressionType::Gzip);

    return true;
}

void MinizDecompressor::Reset()
{
    Init();
}

Size MinizDecompressor::Read(Size max_len, void *user_buf)
{
    // Gzip header is not directly supported by miniz. Currently this
    // will fail if the header is longer than 4096 bytes, which is
    // probably quite rare.
    if (is_gzip && !header_done) {
        uint8_t header[4096];
        Size header_len;

        header_len = ReadRaw(RG_SIZE(header), header);
        if (header_len < 0) {
            return -1;
        } else if (header_len < 10 || header[0] != 0x1F || header[1] != 0x8B) {
            LogError("File '%1' does not look like a Gzip stream", GetFileName());
            return -1;
        }

        Size header_offset = 10;
        if (header[3] & 0x4) { // FEXTRA
            if (header_len - header_offset < 2)
                goto truncated_error;
            uint16_t extra_len = (uint16_t)((header[11] << 8) | header[10]);
            if (extra_len > header_len - header_offset)
                goto truncated_error;
            header_offset += extra_len;
        }
        if (header[3] & 0x8) { // FNAME
            uint8_t *end_ptr = (uint8_t *)memchr(header + header_offset, '\0',
                                                 (size_t)(header_len - header_offset));
            if (!end_ptr)
                goto truncated_error;
            header_offset = end_ptr - header + 1;
        }
        if (header[3] & 0x10) { // FCOMMENT
            uint8_t *end_ptr = (uint8_t *)memchr(header + header_offset, '\0',
                                                 (size_t)(header_len - header_offset));
            if (!end_ptr)
                goto truncated_error;
            header_offset = end_ptr - header + 1;
        }
        if (header[3] & 0x2) { // FHCRC
            if (header_len - header_offset < 2)
                goto truncated_error;
            uint16_t crc16 = (uint16_t)(header[1] << 8 | header[0]);
            if ((mz_crc32(MZ_CRC32_INIT, header, (size_t)header_offset) & 0xFFFF) == crc16) {
                LogError("Failed header CRC16 check in '%s'", GetFileName());
                return -1;
            }
            header_offset += 2;
        }

        // Put back remaining data in the buffer
        memcpy_safe(in_buf, header + header_offset, (size_t)(header_len - header_offset));
        in_ptr = in_buf;
        in_len = header_len - header_offset;

        header_done = true;
    }

    // Inflate (with miniz)
    {
        Size read_len = 0;
        for (;;) {
            if (max_len < out_len) {
                memcpy_safe(user_buf, out_ptr, (size_t)max_len);
                read_len += max_len;
                out_ptr += max_len;
                out_len -= max_len;

                return read_len;
            } else {
                memcpy_safe(user_buf, out_ptr, (size_t)out_len);
                read_len += out_len;
                user_buf = (uint8_t *)out_buf + out_len;
                max_len -= out_len;
                out_ptr = out_buf;
                out_len = 0;

                if (done) {
                    SetEOF(true);
                    return read_len;
                }
            }

            while (out_len < RG_SIZE(out_buf)) {
                if (!in_len) {
                    in_ptr = in_buf;
                    in_len = ReadRaw(RG_SIZE(in_buf), in_buf);
                    if (in_len < 0)
                        return read_len ? read_len : in_len;
                }

                size_t in_arg = (size_t)in_len;
                size_t out_arg = (size_t)(RG_SIZE(out_buf) - out_len);
                uint32_t flags = (uint32_t)
                    ((is_gzip ? 0 : TINFL_FLAG_PARSE_ZLIB_HEADER) |
                     (IsSourceEOF() ? 0 : TINFL_FLAG_HAS_MORE_INPUT));

                tinfl_status status = tinfl_decompress(&inflator, in_ptr, &in_arg,
                                                       out_buf, out_buf + out_len,
                                                       &out_arg, flags);

                if (is_gzip) {
                    crc32 = (uint32_t)mz_crc32(crc32, out_buf + out_len, out_arg);
                    uncompressed_size += (Size)out_arg;
                }

                in_ptr += (Size)in_arg;
                in_len -= (Size)in_arg;
                out_len += (Size)out_arg;

                if (status == TINFL_STATUS_DONE) {
                    // Gzip footer (CRC and size check)
                    if (is_gzip) {
                        uint32_t footer[2];
                        RG_STATIC_ASSERT(RG_SIZE(footer) == 8);

                        if (in_len < RG_SIZE(footer)) {
                            memcpy_safe(footer, in_ptr, (size_t)in_len);

                            Size missing_len = RG_SIZE(footer) - in_len;
                            if (ReadRaw(missing_len, footer + in_len) < missing_len) {
                                if (IsValid()) {
                                    goto truncated_error;
                                } else {
                                    return -1;
                                }
                            }
                        } else {
                            memcpy_safe(footer, in_ptr, RG_SIZE(footer));
                        }
                        footer[0] = LittleEndian(footer[0]);
                        footer[1] = LittleEndian(footer[1]);

                        if (crc32 != footer[0] || (uint32_t)uncompressed_size != footer[1]) {
                            LogError("Failed CRC32 or size check in GZip stream '%1'", GetFileName());
                            return -1;
                        }
                    }

                    done = true;
                    break;
                } else if (status < TINFL_STATUS_DONE) {
                    LogError("Failed to decompress '%1' (Deflate)", GetFileName());
                    return -1;
                }
            }
        }
    }

truncated_error:
    LogError("Truncated Gzip header in '%1'", GetFileName());
    return -1;
}

RG_DEFINE_DECOMPRESSOR(CompressionType::Zlib, MinizDecompressor);
RG_DEFINE_DECOMPRESSOR(CompressionType::Gzip, MinizDecompressor);

}
