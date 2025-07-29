// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/base/base.hh"
#include "crc.hh"
#include "crc64.inc"

namespace RG {

static uint64_t XzUpdate1(uint64_t state, uint8_t byte)
{
    uint64_t ret = (state >> 8) ^ Crc64XzTable0[byte ^ (uint8_t)state];
    return ret;
}

static uint64_t XzUpdate16(uint64_t state, const uint8_t *bytes)
{
    uint64_t ret = Crc64XzTable0[bytes[15]] ^
                   Crc64XzTable1[bytes[14]] ^
                   Crc64XzTable2[bytes[13]] ^
                   Crc64XzTable3[bytes[12]] ^
                   Crc64XzTable4[bytes[11]] ^
                   Crc64XzTable5[bytes[10]] ^
                   Crc64XzTable6[bytes[9]] ^
                   Crc64XzTable7[bytes[8]] ^
                   Crc64XzTable8[bytes[7] ^ (uint8_t)(state >> 56)] ^
                   Crc64XzTable9[bytes[6] ^ (uint8_t)(state >> 48)] ^
                   Crc64XzTable10[bytes[5] ^ (uint8_t)(state >> 40)] ^
                   Crc64XzTable11[bytes[4] ^ (uint8_t)(state >> 32)] ^
                   Crc64XzTable12[bytes[3] ^ (uint8_t)(state >> 24)] ^
                   Crc64XzTable13[bytes[2] ^ (uint8_t)(state >> 16)] ^
                   Crc64XzTable14[bytes[1] ^ (uint8_t)(state >> 8)] ^
                   Crc64XzTable15[bytes[0] ^ (uint8_t)(state >> 0)];
    return ret;
}

uint64_t CRC64xz(uint64_t state, Span<const uint8_t> buf)
{
    state = ~state;

    Size left = std::min(buf.len, AlignUp(buf.ptr, 16) - buf.ptr);
    Size right = std::max(left, AlignDown(buf.end(), 16) - buf.ptr);

    for (Size i = 0; i < left; i++) {
        state = XzUpdate1(state, buf[i]);
    }
    for (Size i = left; i < right; i += 16) {
        state = XzUpdate16(state, buf.ptr + i);
    }
    for (Size i = right; i < buf.len; i++) {
        state = XzUpdate1(state, buf[i]);
    }

    return ~state;
}

static uint64_t NvmeUpdate1(uint64_t state, uint8_t byte)
{
    uint64_t ret = (state >> 8) ^ Crc64NvmeTable0[byte ^ (uint8_t)state];
    return ret;
}

static uint64_t NvmeUpdate16(uint64_t state, const uint8_t *bytes)
{
    uint64_t ret = Crc64NvmeTable0[bytes[15]] ^
                   Crc64NvmeTable1[bytes[14]] ^
                   Crc64NvmeTable2[bytes[13]] ^
                   Crc64NvmeTable3[bytes[12]] ^
                   Crc64NvmeTable4[bytes[11]] ^
                   Crc64NvmeTable5[bytes[10]] ^
                   Crc64NvmeTable6[bytes[9]] ^
                   Crc64NvmeTable7[bytes[8]] ^
                   Crc64NvmeTable8[bytes[7] ^ (uint8_t)(state >> 56)] ^
                   Crc64NvmeTable9[bytes[6] ^ (uint8_t)(state >> 48)] ^
                   Crc64NvmeTable10[bytes[5] ^ (uint8_t)(state >> 40)] ^
                   Crc64NvmeTable11[bytes[4] ^ (uint8_t)(state >> 32)] ^
                   Crc64NvmeTable12[bytes[3] ^ (uint8_t)(state >> 24)] ^
                   Crc64NvmeTable13[bytes[2] ^ (uint8_t)(state >> 16)] ^
                   Crc64NvmeTable14[bytes[1] ^ (uint8_t)(state >> 8)] ^
                   Crc64NvmeTable15[bytes[0] ^ (uint8_t)(state >> 0)];
    return ret;
}

uint64_t CRC64nvme(uint64_t state, Span<const uint8_t> buf)
{
    state = ~state;

    Size left = std::min(buf.len, AlignUp(buf.ptr, 16) - buf.ptr);
    Size right = std::max(left, AlignDown(buf.end(), 16) - buf.ptr);

    for (Size i = 0; i < left; i++) {
        state = NvmeUpdate1(state, buf[i]);
    }
    for (Size i = left; i < right; i += 16) {
        state = NvmeUpdate16(state, buf.ptr + i);
    }
    for (Size i = right; i < buf.len; i++) {
        state = NvmeUpdate1(state, buf[i]);
    }

    return ~state;
}

}
