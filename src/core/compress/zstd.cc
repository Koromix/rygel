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

#include "src/core/base/base.hh"

#include "vendor/zstd/lib/zstd.h"

namespace RG {

class ZstdDecompressor: public StreamDecoder {
    ZSTD_DStream *ctx = nullptr;
    bool done = false;

    HeapArray<uint8_t> in_buf;
    HeapArray<uint8_t> out_buf;

public:
    ZstdDecompressor(StreamReader *reader, CompressionType type);
    ~ZstdDecompressor();

    Size Read(Size max_len, void *out_buf) override;
};

ZstdDecompressor::ZstdDecompressor(StreamReader *reader, CompressionType)
    : StreamDecoder(reader)
{
    ctx = ZSTD_createDStream();
    if (!ctx)
        RG_BAD_ALLOC();

    in_buf.Reserve(ZSTD_DStreamInSize());
    out_buf.Reserve(ZSTD_DStreamOutSize());
}

ZstdDecompressor::~ZstdDecompressor()
{
    ZSTD_freeDStream(ctx);
}

Size ZstdDecompressor::Read(Size max_len, void *user_buf)
{
    for (;;) {
        if (out_buf.len || done) {
            Size copy_len = std::min(max_len, out_buf.len);

            out_buf.len -= copy_len;
            MemCpy(user_buf, out_buf.ptr, copy_len);
            MemMove(out_buf.ptr, out_buf.ptr + copy_len, out_buf.len);

            SetEOF(!out_buf.len && done);
            return copy_len;
        }

        if (in_buf.Available()) {
            Size raw_len = ReadRaw(in_buf.Available(), in_buf.end());
            if (raw_len < 0)
                return -1;
            in_buf.len += raw_len;
        }

        ZSTD_inBuffer input = { in_buf.ptr, (size_t)in_buf.len, 0 };
        ZSTD_outBuffer output = { out_buf.ptr, (size_t)out_buf.capacity, 0 };

        size_t ret = ZSTD_decompressStream(ctx, &output, &input);

        if (!ret) {
            done = true;
        } else if (ZSTD_isError(ret)) {
            LogError("Malformed Zstandard stream in '%1'", GetFileName());
            return -1;
        }

        in_buf.len = (Size)(input.size - input.pos);
        MemMove(in_buf.ptr, in_buf.ptr + input.pos, in_buf.len);

        out_buf.len = (Size)output.pos;
    }

    RG_UNREACHABLE();
}

class ZstdCompressor: public StreamEncoder {
    ZSTD_CStream *ctx = nullptr;

    HeapArray<uint8_t> out_buf;

public:
    ZstdCompressor(StreamWriter *writer, CompressionType type, CompressionSpeed speed);
    ~ZstdCompressor();

    bool Write(Span<const uint8_t> buf) override;
    bool Finalize() override;
};

ZstdCompressor::ZstdCompressor(StreamWriter *writer, CompressionType, CompressionSpeed speed)
    : StreamEncoder(writer)
{
    ctx = ZSTD_createCStream();
    if (!ctx)
        RG_BAD_ALLOC();

    out_buf.Reserve(ZSTD_CStreamOutSize());

    switch (speed) {
        case CompressionSpeed::Default: { ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, 3); } break;
        case CompressionSpeed::Slow: { ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, 9); } break;
        case CompressionSpeed::Fast: { ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, 1); } break;
    }

    ZSTD_CCtx_setParameter(ctx, ZSTD_c_checksumFlag, 1);
}

ZstdCompressor::~ZstdCompressor()
{
    ZSTD_freeCStream(ctx);
}

bool ZstdCompressor::Write(Span<const uint8_t> buf)
{
    ZSTD_inBuffer input = { buf.ptr, (size_t)buf.len, 0 };

    while (input.pos < input.size) {
        ZSTD_EndDirective mode = ZSTD_e_continue;
        ZSTD_outBuffer output = { out_buf.ptr, (size_t)out_buf.capacity, 0 };

        size_t remaining = ZSTD_compressStream2(ctx, &output, &input, mode);

        if (ZSTD_isError(remaining)) {
            LogError("Failed to write Zstandard stream for '%1': %2", GetFileName(), ZSTD_getErrorName(remaining));
            return false;
        }

        out_buf.len = (Size)output.pos;

        if (!WriteRaw(out_buf))
            return false;
    }

    return true;
}

bool ZstdCompressor::Finalize()
{
    ZSTD_inBuffer input = { nullptr, 0, 0 };
    size_t remaining = 0;

    do {
        ZSTD_EndDirective mode = ZSTD_e_end;
        ZSTD_outBuffer output = { out_buf.ptr, (size_t)out_buf.capacity, 0 };

        remaining = ZSTD_compressStream2(ctx, &output, &input, mode);

        if (ZSTD_isError(remaining)) {
            LogError("Failed to write Zstandard stream for '%1': %2", GetFileName(), ZSTD_getErrorName(remaining));
            return false;
        }

        out_buf.len = (Size)output.pos;

        if (!WriteRaw(out_buf))
            return false;
    } while (remaining);

    return true;
}

RG_REGISTER_DECOMPRESSOR(CompressionType::Zstd, ZstdDecompressor);
RG_REGISTER_COMPRESSOR(CompressionType::Zstd, ZstdCompressor);

}
