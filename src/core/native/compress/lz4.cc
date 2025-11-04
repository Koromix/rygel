// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"

#include "vendor/lz4/lib/lz4.h"
#include "vendor/lz4/lib/lz4hc.h"
#include "vendor/lz4/lib/lz4frame.h"

namespace K {

class LZ4Decompressor: public StreamDecoder {
    LZ4F_dctx *decoder = nullptr;
    bool done = false;

    uint8_t in_buf[256 * 1024];
    Size in_len = 0;
    Size in_hint = K_SIZE(in_buf);

    uint8_t out_buf[256 * 1024];
    Size out_len = 0;

public:
    LZ4Decompressor(StreamReader *reader, CompressionType type);
    ~LZ4Decompressor();

    Size Read(Size max_len, void *out_buf) override;
};

LZ4Decompressor::LZ4Decompressor(StreamReader *reader, CompressionType)
    : StreamDecoder(reader)
{
    LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&decoder, LZ4F_VERSION);
    if (LZ4F_isError(err))
        K_BAD_ALLOC();
}

LZ4Decompressor::~LZ4Decompressor()
{
    LZ4F_freeDecompressionContext(decoder);
}

Size LZ4Decompressor::Read(Size max_len, void *user_buf)
{
    for (;;) {
        if (out_len || done) {
            Size copy_len = std::min(max_len, out_len);

            out_len -= copy_len;
            MemCpy(user_buf, out_buf, copy_len);
            MemMove(out_buf, out_buf + copy_len, out_len);

            SetEOF(!out_len && done);
            return copy_len;
        }

        if (in_len < in_hint) {
            Size raw_len = ReadRaw(in_hint - in_len, in_buf + in_len);
            if (raw_len < 0)
                return -1;
            in_len += raw_len;
        }

        const uint8_t *next_in = in_buf;
        uint8_t *next_out = out_buf + out_len;
        size_t avail_in = (size_t)in_len;
        size_t avail_out = (size_t)(K_SIZE(out_buf) - out_len);

        LZ4F_decompressOptions_t opt = {};
        size_t ret = LZ4F_decompress(decoder, next_out, &avail_out, next_in, &avail_in, &opt);

        if (!ret) {
            done = true;
        } else if (LZ4F_isError(ret)) {
            LogError("Malformed LZ4 stream in '%1': %2", GetFileName(), LZ4F_getErrorName(ret));
            return -1;
        }

        MemMove(in_buf, in_buf + avail_in, in_len - avail_in);
        in_len -= avail_in;
        in_hint = std::min(K_SIZE(in_buf), (Size)ret);

        out_len += (Size)avail_out;
    }

    K_UNREACHABLE();
}

class LZ4Compressor: public StreamEncoder {
    LZ4F_cctx *encoder = nullptr;
    LZ4F_preferences_t prefs = {};

    HeapArray<uint8_t> dynamic_buf;

public:
    LZ4Compressor(StreamWriter *writer, CompressionType type, CompressionSpeed speed);
    ~LZ4Compressor();

    bool Write(Span<const uint8_t> buf) override;
    bool Finalize() override;
};

LZ4Compressor::LZ4Compressor(StreamWriter *writer, CompressionType, CompressionSpeed speed)
    : StreamEncoder(writer)
{
    LZ4F_errorCode_t err = LZ4F_createCompressionContext(&encoder, LZ4F_VERSION);
    if (LZ4F_isError(err))
        K_BAD_ALLOC();

    switch (speed) {
        case CompressionSpeed::Default: { prefs.compressionLevel = LZ4HC_CLEVEL_MIN; } break;
        case CompressionSpeed::Slow: { prefs.compressionLevel = LZ4HC_CLEVEL_MAX; } break;
        case CompressionSpeed::Fast: { prefs.compressionLevel = 0; } break;
    }

    dynamic_buf.Grow(LZ4F_HEADER_SIZE_MAX);

    size_t ret = LZ4F_compressBegin(encoder, dynamic_buf.end(), dynamic_buf.capacity - dynamic_buf.len, &prefs);
    if (LZ4F_isError(ret))
        K_BAD_ALLOC();

    dynamic_buf.len += ret;
}

LZ4Compressor::~LZ4Compressor()
{
    LZ4F_freeCompressionContext(encoder);
}

bool LZ4Compressor::Write(Span<const uint8_t> buf)
{
    size_t needed = LZ4F_compressBound((size_t)buf.len, &prefs);
    dynamic_buf.Grow((Size)needed);

    size_t available = (size_t)dynamic_buf.Available();
    size_t ret = LZ4F_compressUpdate(encoder, dynamic_buf.end(), available, buf.ptr, (size_t)buf.len, nullptr);

    if (LZ4F_isError(ret)) {
        LogError("Failed to write LZ4 stream for '%1': %2", GetFileName(), LZ4F_getErrorName(ret));
        return false;
    }

    dynamic_buf.len += (Size)ret;

    if (dynamic_buf.len >= 512) {
        if (!WriteRaw(dynamic_buf))
            return false;
        dynamic_buf.len = 0;
    }

    return true;
}

bool LZ4Compressor::Finalize()
{
    size_t needed = LZ4F_compressBound(0, &prefs);
    dynamic_buf.Grow((Size)needed);

    size_t ret = LZ4F_compressEnd(encoder, dynamic_buf.end(),
                                  (size_t)(dynamic_buf.capacity - dynamic_buf.len), nullptr);

    if (LZ4F_isError(ret)) {
        LogError("Failed to finalize LZ4 stream for '%1': %2", GetFileName(), LZ4F_getErrorName(ret));
        return false;
    }

    dynamic_buf.len += (Size)ret;

    if (!WriteRaw(dynamic_buf))
        return false;
    dynamic_buf.len = 0;

    return true;
}

K_REGISTER_DECOMPRESSOR(CompressionType::LZ4, LZ4Decompressor);
K_REGISTER_COMPRESSOR(CompressionType::LZ4, LZ4Compressor);

}
