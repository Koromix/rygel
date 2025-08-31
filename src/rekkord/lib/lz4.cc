// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "lz4.hh"

namespace K {

DecodeLZ4::DecodeLZ4()
{
    LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&decoder, LZ4F_VERSION);

    if (LZ4F_isError(err))
        K_BAD_ALLOC();
}

DecodeLZ4::~DecodeLZ4()
{
    LZ4F_freeDecompressionContext(decoder);
}

Span<uint8_t> DecodeLZ4::PrepareAppend(Size needed)
{
    in_buf.Grow(needed);

    Span<uint8_t> buf = MakeSpan(in_buf.end(), in_buf.capacity - in_buf.len);
    in_buf.len += needed;

    return buf;
}

bool DecodeLZ4::Flush(bool complete, FunctionRef<bool(Span<const uint8_t>)> func)
{
    Size treshold = complete ? 1 : in_hint;

    while (!done && in_buf.len >= treshold) {
        // Rekkord pads blobs (Padmé), so ignore data past end of LZ4 frame

        const uint8_t *next_in = in_buf.ptr;
        uint8_t *next_out = out_buf;
        size_t avail_in = (size_t)in_buf.len;
        size_t avail_out = (size_t)K_SIZE(out_buf);

        LZ4F_decompressOptions_t opt = {};
        size_t ret = LZ4F_decompress(decoder, next_out, &avail_out, next_in, &avail_in, &opt);

        if (!ret) {
            done = true;
        } else if (LZ4F_isError(ret)) {
            LogError("Malformed LZ4 stream: %1", LZ4F_getErrorName(ret));
            return false;
        }

        MemMove(in_buf.ptr, in_buf.ptr + avail_in, in_buf.len - avail_in);
        in_buf.len -= avail_in;
        in_hint = (Size)ret;

        Span<const uint8_t> buf = MakeSpan(out_buf, (Size)avail_out);

        if (!func(buf))
            return false;
    }

    return true;
}

EncodeLZ4::EncodeLZ4()
{
    LZ4F_errorCode_t err = LZ4F_createCompressionContext(&encoder, LZ4F_VERSION);

    if (LZ4F_isError(err))
        K_BAD_ALLOC();
}

EncodeLZ4::~EncodeLZ4()
{
    LZ4F_freeCompressionContext(encoder);
}

bool EncodeLZ4::Start(int level)
{
    dynamic_buf.Grow(LZ4F_HEADER_SIZE_MAX);

    LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
    prefs.compressionLevel = level;

    size_t available = (size_t)dynamic_buf.Available();
    size_t ret = LZ4F_compressBegin(encoder, dynamic_buf.end(), available, &prefs);

    if (LZ4F_isError(ret)) {
        LogError("Failed to start LZ4 stream: %1", LZ4F_getErrorName(ret));
        return false;
    }

    dynamic_buf.len += ret;

    started = true;
    return true;
}

bool EncodeLZ4::Append(Span<const uint8_t> buf)
{
    K_ASSERT(started);

    size_t needed = LZ4F_compressBound((size_t)buf.len, nullptr);
    dynamic_buf.Grow((Size)needed);

    size_t available = (size_t)dynamic_buf.Available();
    size_t ret = LZ4F_compressUpdate(encoder, dynamic_buf.end(), available, buf.ptr, (size_t)buf.len, nullptr);

    if (LZ4F_isError(ret)) {
        LogError("Failed to write LZ4 stream: %1", LZ4F_getErrorName(ret));
        return false;
    }

    dynamic_buf.len += (Size)ret;

    return true;
}

bool EncodeLZ4::Flush(bool complete, FunctionRef<Size(Span<const uint8_t>)> func)
{
    K_ASSERT(started);

    if (complete) {
        size_t needed = LZ4F_compressBound(0, nullptr);
        dynamic_buf.Grow((Size)needed);

        size_t ret = LZ4F_compressEnd(encoder, dynamic_buf.end(),
                                      (size_t)(dynamic_buf.capacity - dynamic_buf.len), nullptr);

        if (LZ4F_isError(ret)) {
            LogError("Failed to finalize LZ4 stream: %1", LZ4F_getErrorName(ret));
            return false;
        }

        dynamic_buf.len += (Size)ret;
    }

    while (dynamic_buf.len) {
        Size processed = func(dynamic_buf);

        if (processed < 0)
            return false;
        if (!processed)
            break;

        MemMove(dynamic_buf.ptr, dynamic_buf.ptr + processed, dynamic_buf.len - processed);
        dynamic_buf.len -= processed;
    }

    return true;
}

}
