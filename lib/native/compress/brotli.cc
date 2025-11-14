// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"

#include "vendor/brotli/c/include/brotli/decode.h"
#include "vendor/brotli/c/include/brotli/encode.h"

namespace K {

class BrotliDecompressor: public StreamDecoder {
    BrotliDecoderState *state = nullptr;
    bool done = false;

    uint8_t in_buf[256 * 1024];
    Size in_len = 0;

    uint8_t out_buf[256 * 1024];
    Size out_len = 0;

public:
    BrotliDecompressor(StreamReader *reader, CompressionType type);
    ~BrotliDecompressor();

    Size Read(Size max_len, void *out_buf) override;
};

BrotliDecompressor::BrotliDecompressor(StreamReader *reader, CompressionType)
    : StreamDecoder(reader)
{
    state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    if (!state)
        K_BAD_ALLOC();
}

BrotliDecompressor::~BrotliDecompressor()
{
    BrotliDecoderDestroyInstance(state);
}

Size BrotliDecompressor::Read(Size max_len, void *user_buf)
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

        if (in_len < K_SIZE(in_buf)) {
            Size raw_len = ReadRaw(K_SIZE(in_buf) - in_len, in_buf + in_len);
            if (raw_len < 0)
                return -1;
            in_len += raw_len;
        }

        const uint8_t *next_in = in_buf;
        uint8_t *next_out = out_buf + out_len;
        size_t avail_in = (size_t)in_len;
        size_t avail_out = (size_t)(K_SIZE(out_buf) - out_len);

        BrotliDecoderResult ret = BrotliDecoderDecompressStream(state, &avail_in, &next_in,
                                                                &avail_out, &next_out, nullptr);

        if (ret == BROTLI_DECODER_RESULT_SUCCESS) {
            done = true;
        } else if (ret == BROTLI_DECODER_RESULT_ERROR) {
            LogError("Malformed Brotli stream in '%1'", GetFileName());
            return -1;
        }

        MemMove(in_buf, next_in, avail_in);
        in_len = avail_in;

        out_len = next_out - out_buf - out_len;
    }

    K_UNREACHABLE();
}

class BrotliCompressor: public StreamEncoder {
    BrotliEncoderStateStruct *state = nullptr;

public:
    BrotliCompressor(StreamWriter *writer, CompressionType type, CompressionSpeed speed);
    ~BrotliCompressor();

    bool Write(Span<const uint8_t> buf) override;
    bool Finalize() override;
};

BrotliCompressor::BrotliCompressor(StreamWriter *writer, CompressionType, CompressionSpeed speed)
    : StreamEncoder(writer)
{
    state = BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);
    if (!state)
        K_BAD_ALLOC();

    static_assert(BROTLI_MIN_QUALITY == 0 && BROTLI_MAX_QUALITY == 11);

    switch (speed) {
        case CompressionSpeed::Default: { BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, 6); } break;
        case CompressionSpeed::Slow: { BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, 11); } break;
        case CompressionSpeed::Fast: { BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, 0); } break;
    }
}

BrotliCompressor::~BrotliCompressor()
{
    BrotliEncoderDestroyInstance(state);
}

bool BrotliCompressor::Write(Span<const uint8_t> buf)
{
    uint8_t output_buf[2048];

    while (buf.len || BrotliEncoderHasMoreOutput(state)) {
        const uint8_t *next_in = buf.ptr;
        uint8_t *next_out = output_buf;
        size_t avail_in = (size_t)buf.len;
        size_t avail_out = K_SIZE(output_buf);

        if (!BrotliEncoderCompressStream(state, BROTLI_OPERATION_PROCESS,
                                         &avail_in, &next_in, &avail_out, &next_out, nullptr)) {
            LogError("Failed to compress '%1' with Brotli", GetFileName());
            return false;
        }
        if (!WriteRaw(MakeSpan(output_buf, next_out - output_buf)))
            return false;

        buf.len -= next_in - buf.ptr;
        buf.ptr = next_in;
    }

    return true;
}

bool BrotliCompressor::Finalize()
{
    uint8_t output_buf[2048];

    do {
        const uint8_t *next_in = nullptr;
        uint8_t *next_out = output_buf;
        size_t avail_in = 0;
        size_t avail_out = K_SIZE(output_buf);

        if (!BrotliEncoderCompressStream(state, BROTLI_OPERATION_FINISH,
                                         &avail_in, &next_in, &avail_out, &next_out, nullptr)) {
            LogError("Failed to compress '%1' with Brotli", GetFileName());
            return false;
        }
        if (!WriteRaw(MakeSpan(output_buf, next_out - output_buf)))
            return false;
    } while (BrotliEncoderHasMoreOutput(state));

    return true;
}

K_REGISTER_DECOMPRESSOR(CompressionType::Brotli, BrotliDecompressor);
K_REGISTER_COMPRESSOR(CompressionType::Brotli, BrotliCompressor);

}
