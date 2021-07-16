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

#include "../libcc/libcc.hh"
#include "otp.hh"
#include "../../../vendor/mbedtls/include/mbedtls/sha1.h"
#include "../../../vendor/qrcodegen/QrCode.hpp"
#include "../../../vendor/miniz/miniz.h"

namespace RG {

Size sec_EncodeBase32(Span<const uint8_t> bytes, bool pad, Span<char> out_buf)
{
    RG_ASSERT(out_buf.len >= sec_GetBase32EncodedLength(bytes.len));

    static const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    Size len = 0;

    for (Size i = 0, j = 0;; i += 5, j = (j + 1) & 0x7) {
        Size offset = i / 8;

        if (offset >= bytes.len) {
            if (pad) {
                Size pad_len = (8 - j) & 0x7;
                for (Size k = 0; k < pad_len; k++) {
                    out_buf[len++] = '=';
                }
            }

            break;
        }

        int b0 = bytes[offset];
        int b1 = (offset + 1) < bytes.len ? bytes[offset + 1] : 0;

        int value;
        switch (j) {
            case 0: { value = ((b0 >> 3) & 0x1F); } break;
            case 1: { value = ((b0 << 2) & 0x1C) | ((b1 >> 6) & 0x3); } break;
            case 2: { value = ((b0 >> 1) & 0x1F); } break;
            case 3: { value = ((b0 << 4) & 0x10) | ((b1 >> 4) & 0xF); } break;
            case 4: { value = ((b0 << 1) & 0x1E) | ((b1 >> 7) & 0x1); } break;
            case 5: { value = ((b0 >> 2) & 0x1F); } break;
            case 6: { value = ((b0 << 3) & 0x18) | ((b1 >> 5) & 0x7); } break;
            case 7: { value = b0 & 0x1F; } break;
        }

        RG_ASSERT(value >= 0 && value < 32);
        out_buf[len++] = chars[value];
    }
    out_buf[len] = 0;

    return len;
}

static inline uint8_t DecodeBase32Char(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return (uint8_t)(c - 'A');
    } else if (c >= 'a' && c <= 'z') {
        return (uint8_t)(c - 'a');
    } else if (c >= '2' && c <= '7') {
        return (uint8_t)(c - '2' + 26);
    } else {
        return 0xFF;
    }
}

Size sec_DecodeBase32(Span<const char> b32, Span<uint8_t> out_buf)
{
    RG_ASSERT(out_buf.len >= sec_GetBase32DecodedLength(b32.len));

    Size len = 0;

    for (Size i = 0, j = 0; i < b32.len && b32[i] != '='; i++, j = (j + 1) & 0x7) {
        uint8_t value = DecodeBase32Char(b32[i]);

        if (RG_UNLIKELY(value == 0xFF)) {
            LogError("Unexpected Base32 character '%1'", b32[i]);
            return -1;
        }

        switch (j) {
            case 0: { out_buf[len] = ((value << 3) & 0xF8); } break;
            case 1: {
                out_buf[len++] |= ((value >> 2) & 0x7);
                out_buf[len] = ((value << 6) & 0xC0);
            } break;
            case 2: { out_buf[len] |= ((value << 1) & 0x3E); } break;
            case 3: {
                out_buf[len++] |= ((value >> 4) & 0x1);
                out_buf[len] = ((value << 4) & 0xF0);
            } break;
            case 4: {
                out_buf[len++] |= ((value >> 1) & 0xF);
                out_buf[len] = ((value << 7) & 0x80);
            } break;
            case 5: { out_buf[len] |= ((value << 2) & 0x7C); } break;
            case 6: {
                out_buf[len++] |= ((value >> 3) & 0x3);
                out_buf[len] = ((value << 5) & 0xE0);
            } break;
            case 7: { out_buf[len++] |= (value & 0x1F); } break;
        }
    }

    return len;
}

// XXX: Kind of duplicated in libnet (but changed to allow @). Clean this up!
static void EncodeUrlSafe(const char *str, HeapArray<char> *out_buf)
{
    for (Size i = 0; str[i]; i++) {
        int c = str[i];

        if (IsAsciiAlphaOrDigit(c) || c == '-' || c == '.' || c == '_' || c == '~' || c == '@') {
            out_buf->Append(c);
        } else {
            Fmt(out_buf, "%%%1", FmtHex((uint8_t)c).Pad0(-2));
        }
    }

    out_buf->Grow(1);
    out_buf->ptr[out_buf->len] = 0;
}

const char *sec_GenerateHotpUrl(const char *label, const char *username, const char *issuer,
                                const char *secret, int digits, Allocator *alloc)
{
    HeapArray<char> buf(alloc);

    Fmt(&buf, "otpauth://totp/"); EncodeUrlSafe(label, &buf);
    Fmt(&buf, ":"); EncodeUrlSafe(username, &buf);
    Fmt(&buf, "?secret=%1&digits=%2", secret, digits);
    if (issuer) {
        Fmt(&buf, "&issuer="); EncodeUrlSafe(issuer, &buf);
    }

    const char *url = buf.TrimAndLeak(1).ptr;
    return url;
}

bool sec_GenerateHotpPng(const char *url, HeapArray<uint8_t> *out_buf)
{
    RG_ASSERT(!out_buf->len);

    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(url, qrcodegen::QrCode::Ecc::MEDIUM);

    int border = 4;
    int resolution = 4;
    int size = qr.getSize() * resolution + 2 * border * resolution;

    // Create uncompressed black and white 32-byte RGB image
    HeapArray<uint32_t> buf;
    buf.Reserve(size * size);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            bool value = qr.getModule(x / resolution - border, y / resolution - border);

#ifdef RG_ARCH_LITTLE_ENDIAN
            buf.Append(value ? 0xFF000000u : 0xFFFFFFFFu);
#else
            buf.Append(value ? 0x000000FFu : 0xFFFFFFFFu);
#endif
        }
    }

    // Compress to PNG
    size_t png_len;
    uint8_t *png_buf = (uint8_t *)tdefl_write_image_to_png_file_in_memory((void *)buf.ptr, size, size, 4, &png_len);
    if (!png_buf) {
        LogError("Failed to encode PNG image");
        return 1;
    }
    RG_DEFER { mz_free(png_buf); };

    // Copy... kind of sad but we're not in hotcode territory here
    out_buf->Append(MakeSpan(png_buf, (Size)png_len));

    return true;
}

static void HmacSha1(Span<const uint8_t> key, Span<const uint8_t> message, uint8_t out_digest[20])
{
    uint8_t padded_key[64];

    // Hash and/or pad key
    if (key.len > RG_SIZE(padded_key)) {
        mbedtls_sha1_ret(key.ptr, (size_t)key.len, padded_key);
        memset_safe(padded_key + 20, 0, RG_SIZE(padded_key) - 20);
    } else {
        memcpy_safe(padded_key, key.ptr, (size_t)key.len);
        memset_safe(padded_key + key.len, 0, (size_t)(RG_SIZE(padded_key) - key.len));
    }

    // Inner hash
    uint8_t inner_hash[20];
    {
        mbedtls_sha1_context ctx;
        mbedtls_sha1_init(&ctx);
        mbedtls_sha1_starts_ret(&ctx);
        RG_DEFER { mbedtls_sha1_free(&ctx); };

        for (Size i = 0; i < RG_SIZE(padded_key); i++) {
            padded_key[i] ^= 0x36;
        }

        mbedtls_sha1_update_ret(&ctx, padded_key, RG_SIZE(padded_key));
        mbedtls_sha1_update_ret(&ctx, message.ptr, (size_t)message.len);
        mbedtls_sha1_finish_ret(&ctx, (unsigned char *)inner_hash);
    }

    // Outer hash
    {
        mbedtls_sha1_context ctx;
        mbedtls_sha1_init(&ctx);
        mbedtls_sha1_starts_ret(&ctx);
        RG_DEFER { mbedtls_sha1_free(&ctx); };

        for (Size i = 0; i < RG_SIZE(padded_key); i++) {
            padded_key[i] ^= 0x36; // IPAD is still there
            padded_key[i] ^= 0x5C;
        }

        mbedtls_sha1_update_ret(&ctx, padded_key, RG_SIZE(padded_key));
        mbedtls_sha1_update_ret(&ctx, inner_hash, RG_SIZE(inner_hash));
        mbedtls_sha1_finish_ret(&ctx, (unsigned char *)out_digest);
    }
}

int sec_ComputeHotp(Span<const uint8_t> key, int64_t counter, int digits)
{
    union { int64_t i; uint8_t raw[8]; } message;
    message.i = BigEndian(counter);

    // HMAC-SHA1
    uint8_t digest[20];
    HmacSha1(key, message.raw, digest);

    // Dynamic truncation
    int offset = digest[19] & 0xF;
    uint32_t sbits = (((uint32_t)digest[offset + 0] & 0x7F) << 24) |
                     (((uint32_t)digest[offset + 1] & 0xFF) << 16) |
                     (((uint32_t)digest[offset + 2] & 0xFF) << 8) |
                     (((uint32_t)digest[offset + 3] & 0xFF) << 0);

    // Return just enough digits
    switch (digits) {
        case 6: { return sbits % 1000000; } break;
        case 7: { return sbits % 10000000; } break;
        case 8: { return sbits % 100000000; } break;

        default: {
            LogError("Invalid number of digits");
            return -1;
        } break;
    }
}

bool sec_CheckHotp(Span<const uint8_t> key, int64_t counter, int digits, int window, const char *code)
{
    for (int i = -window; i <= window; i++) {
        int ret = sec_ComputeHotp(key, counter + i, digits);
        if (ret < 0)
            return false;

        char buf[16];
        Fmt(buf, "%1", FmtArg(ret).Pad0(-digits));

        if (TestStr(buf, code))
            return true;
    }

    return false;
}

}
