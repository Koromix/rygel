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
#include "qrcode.hh"
#include "vendor/qrcodegen/qrcodegen.h"

namespace K {

template <typename T>
bool EncodeText(Span<const char> text, int border, T func, StreamWriter *out_st)
{
    uint8_t qr[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tmp[qrcodegen_BUFFER_LEN_MAX];
    static_assert(qrcodegen_BUFFER_LEN_MAX < Kibibytes(8));

    if (text.len > K_SIZE(tmp)) [[unlikely]] {
        LogError("Cannot encode %1 bytes as QR code (max = %2)", text.len, K_SIZE(tmp));
        return false;
    }

    bool success = qrcodegen_encodeText(text.ptr, (size_t)text.len, tmp, qr, qrcodegen_Ecc_MEDIUM,
                                        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);
    if (!success) {
        LogError("QR code encoding failed");
        return false;
    }

    if (!func(qr, border, out_st))
        return false;

    return true;
}

template <typename T>
bool EncodeBinary(Span<const uint8_t> data, int border, T func, StreamWriter *out_st)
{
    uint8_t qr[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tmp[qrcodegen_BUFFER_LEN_MAX];
    static_assert(qrcodegen_BUFFER_LEN_MAX < Kibibytes(8));

    if (data.len > K_SIZE(tmp)) [[unlikely]] {
        LogError("Cannot encode %1 bytes as QR code (max = %2)", data.len, K_SIZE(tmp));
        return false;
    }
    MemCpy(tmp, data.ptr, data.len);

    bool success = qrcodegen_encodeBinary(tmp, (size_t)data.len, qr, qrcodegen_Ecc_MEDIUM,
                                          qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);
    if (!success) {
        LogError("QR code encoding failed");
        return false;
    }

    if (!func(qr, border, out_st))
        return false;

    return true;
}

static bool GeneratePNG(const uint8_t qr[qrcodegen_BUFFER_LEN_MAX], int border, StreamWriter *out_st)
{
    // Account for scanline byte
    static const int MaxSize = (int)Kibibytes(2) - 1;

    static const uint8_t PngHeader[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    static const uint8_t PngFooter[] = { 0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82 };

    int size = qrcodegen_getSize(qr) + 2 * border / 4;
    int size4 = qrcodegen_getSize(qr) * 4 + 2 * border;

    if (size > MaxSize) [[unlikely]] {
        LogError("Excessive QR code image size");
        return false;
    }

    out_st->Write(PngHeader);

#pragma pack(push, 1)
    struct ChunkHeader {
        uint32_t len;
        uint8_t type[4];
    };
    struct IHDR {
        uint32_t width;
        uint32_t height;
        uint8_t bit_depth;
        uint8_t color_type;
        uint8_t compression;
        uint8_t filter;
        uint8_t interlace;
    };
#pragma pack(pop)

    // Write IHDR chunk
    {
        ChunkHeader chunk = {};
        IHDR ihdr = {};

        chunk.len = BigEndian((uint32_t)K_SIZE(ihdr));
        MemCpy(chunk.type, "IHDR", 4);
        ihdr.width = BigEndian(size4);
        ihdr.height = BigEndian(size4);
        ihdr.bit_depth = 1;
        ihdr.color_type = 0;
        ihdr.compression = 0;
        ihdr.filter = 0;
        ihdr.interlace = 0;

        uint32_t crc32 = 0;
        crc32 = CRC32(crc32, MakeSpan((const uint8_t *)&chunk + 4, K_SIZE(chunk) - 4));
        crc32 = CRC32(crc32, MakeSpan((const uint8_t *)&ihdr, K_SIZE(ihdr)));
        crc32 = BigEndian(crc32);

        out_st->Write(MakeSpan((const uint8_t *)&chunk, K_SIZE(chunk)));
        out_st->Write(MakeSpan((const uint8_t *)&ihdr, K_SIZE(ihdr)));
        out_st->Write(MakeSpan((const uint8_t *)&crc32, 4));
    }

    // Write image data (IDAT)
    {
        HeapArray<uint8_t> idat;
        ChunkHeader chunk = {};

        chunk.len = 0; // Unknown for now
        MemCpy(chunk.type, "IDAT", 4);
        idat.Append(MakeSpan((const uint8_t *)&chunk, K_SIZE(chunk)));

        StreamWriter writer(&idat, "<png>", 0, CompressionType::Zlib);
        for (int y = 0; y < size4; y++) {
            LocalArray<uint8_t, MaxSize + 1> buf;
            buf.Append((uint8_t)0); // Scanline filter

            for (int x = 0; x < size; x += 2) {
                uint8_t byte = (qrcodegen_getModule(qr, x + 0 - border / 4, y / 4 - border / 4) * 0xF0) |
                               (qrcodegen_getModule(qr, x + 1 - border / 4, y / 4 - border / 4) * 0x0F);
                buf.Append(~byte);
            }

            writer.Write(buf);
        }
        bool success = writer.Close();
        K_ASSERT(success);

        // Fix length
        {
            uint32_t len = BigEndian((uint32_t)(idat.len - K_SIZE(ChunkHeader)));
            uint32_t *ptr = (uint32_t *)idat.ptr;

            MemCpy(ptr, &len, K_SIZE(len));
        }

        uint32_t crc32 = 0;
        crc32 = CRC32(crc32, MakeSpan((const uint8_t *)idat.ptr + 4, idat.len - 4));
        crc32 = BigEndian(crc32);

        out_st->Write(idat);
        out_st->Write(MakeSpan((const uint8_t *)&crc32, 4));
    }

    // End image (IEND)
    out_st->Write(PngFooter);

    return true;
}

bool qr_EncodeTextToPng(Span<const char> text, int border, StreamWriter *out_st)
{
    return EncodeText(text, border, GeneratePNG, out_st);
}

bool qr_EncodeBinaryToPng(Span<const uint8_t> data, int border, StreamWriter *out_st)
{
    return EncodeBinary(data, border, GeneratePNG, out_st);
}

static void GenerateUnicodeBlocks(const uint8_t qr[qrcodegen_BUFFER_LEN_MAX], bool ansi, int border, StreamWriter *out_st)
{
    int size = qrcodegen_getSize(qr) + 2 * border;

    for (int y = 0; y < size; y += 2) {
        out_st->Write(ansi ? "\x1B[40;37m" : "");

        for (int x = 0; x < size; x++) {
            int combined = (qrcodegen_getModule(qr, x - border, y - border) << 0) |
                           (qrcodegen_getModule(qr, x - border, y - border + 1) << 1);

            switch (combined) {
                case 0: { out_st->Write("\u2588"); } break;
                case 1: { out_st->Write("\u2584"); } break;
                case 2: { out_st->Write("\u2580"); } break;
                case 3: { out_st->Write(' '); } break;
            }
        }

        out_st->Write(ansi ? "\x1B[0m\n" : "\n");
    }
}

bool qr_EncodeTextToBlocks(Span<const char> text, bool ansi, int border, StreamWriter *out_st)
{
    K_ASSERT(border % 2 == 0);

    return EncodeText(text, border, [&](const uint8_t qr[qrcodegen_BUFFER_LEN_MAX], int border, StreamWriter *out_st) {
        GenerateUnicodeBlocks(qr, ansi, border, out_st);
        return true;
    }, out_st);
}

bool qr_EncodeBinaryToBlocks(Span<const uint8_t> data, bool ansi, int border, StreamWriter *out_st)
{
    K_ASSERT(border % 2 == 0);

    return EncodeBinary(data, border, [&](const uint8_t qr[qrcodegen_BUFFER_LEN_MAX], int border, StreamWriter *out_st) {
        GenerateUnicodeBlocks(qr, ansi, border, out_st);
        return true;
    }, out_st);
}

}
