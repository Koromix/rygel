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

#include "vendor/lz4/lib/lz4.h"
#include "vendor/lz4/lib/lz4hc.h"
#include "vendor/lz4/lib/lz4frame.h"

namespace RG {

class LZ4Decompressor: public StreamDecompressor {
    LZ4F_dctx *decoder = nullptr;
    bool done = false;

    uint8_t in_buf[256 * 1024];
    Size in_len = 0;

    uint8_t out_buf[256 * 1024];
    Size out_len = 0;

public:
    LZ4Decompressor(StreamReader *reader) : StreamDecompressor(reader) {}
    ~LZ4Decompressor();

    bool Init() override;
    void Reset() override;
    Size Read(Size max_len, void *out_buf) override;
};

LZ4Decompressor::~LZ4Decompressor()
{
    if (decoder) {
        LZ4F_freeDecompressionContext(decoder);
    }
}

bool LZ4Decompressor::Init()
{
    LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&decoder, LZ4F_VERSION);

    if (LZ4F_isError(err)) {
        LogError("Failed to initialize LZ4 decompression: %1", LZ4F_getErrorName(err));
        return false;
    }

    return true;
}

void LZ4Decompressor::Reset()
{
    LZ4F_resetDecompressionContext(decoder);
}

Size LZ4Decompressor::Read(Size max_len, void *user_buf)
{
    for (;;) {
        if (out_len || done) {
            Size copy_len = std::min(max_len, out_len);

            out_len -= copy_len;
            memcpy(user_buf, out_buf, copy_len);
            memmove(out_buf, out_buf + copy_len, out_len);

            SetEOF(!out_len && done);
            return copy_len;
        }

        if (in_len < RG_SIZE(in_buf)) {
            Size raw_len = ReadRaw(RG_SIZE(in_buf) - in_len, in_buf + in_len);
            if (raw_len < 0)
                return -1;
            in_len += raw_len;
        }

        const uint8_t *next_in = in_buf;
        uint8_t *next_out = out_buf + out_len;
        size_t avail_in = (size_t)in_len;
        size_t avail_out = (size_t)(RG_SIZE(out_buf) - out_len);

        LZ4F_decompressOptions_t opt = {};
        size_t ret = LZ4F_decompress(decoder, next_out, &avail_out, next_in, &avail_in, &opt);

        if (!ret) {
            done = true;
        } else if (LZ4F_isError(ret)) {
            LogError("Malformed LZ4 stream in '%1': %2", GetFileName(), LZ4F_getErrorName(ret));
            return -1;
        }

        memmove_safe(in_buf, in_buf + avail_in, (size_t)in_len - avail_in);
        in_len -= avail_in;

        out_len += (Size)avail_out;
    }

    RG_UNREACHABLE();
}

RG_DEFINE_DECOMPRESSOR(CompressionType::LZ4, LZ4Decompressor);

}
