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
#include "src/core/wrap/json.hh"
#include "embed.hh"

namespace RG {

// For simplicity, I've replicated the required data structures from libcc
// and packer.hh directly below. Don't forget to keep them in sync.
static const char *const CodePrefix =
R"(// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
typedef int64_t Size;
#elif defined(__i386__) || defined(_M_IX86) || defined(__arm__) || defined(__EMSCRIPTEN__)
typedef int32_t Size;
#endif

#if defined(EXPORT)
    #if defined(_WIN32)
        #define EXPORT_SYMBOL __declspec(dllexport)
    #else
        #define EXPORT_SYMBOL __attribute__((visibility("default")))
    #endif
#else
    #define EXPORT_SYMBOL
#endif
#if defined(__cplusplus)
    #define EXTERN extern "C"
#else
    #define EXTERN extern
#endif

typedef struct Span {
    const void *ptr;
    Size len;
} Span;

typedef struct AssetInfo {
    const char *name;
    int compression_type; // CompressionType
    Span data;
} AssetInfo;)";

struct BlobInfo {
    const char *name;

    CompressionType compression_type;
    Size len;
};

static const char *StripDirectoryComponents(Span<const char> filename, int strip_count)
{
    const char *name = filename.ptr;
    for (int i = 0; filename.len && i <= strip_count; i++) {
        name = SplitStrAny(filename, RG_PATH_SEPARATORS, &filename).ptr;
    }

    return name;
}

static CompressionType AdaptCompression(const char *filename, CompressionType compression_type)
{
    if (compression_type == CompressionType::None)
        return CompressionType::None;
    if (!CanCompressFile(filename))
        return CompressionType::None;

    return compression_type;
}

static bool LoadMetaFile(const char *filename, CompressionType compression_type,
                         Allocator *alloc, HeapArray<EmbedAsset> *out_assets)
{
    RG_DEFER_NC(out_guard, len = out_assets->len) { out_assets->RemoveFrom(len); };

    StreamReader st(filename);
    if (!st.IsValid())
        return false;

    IniParser ini(&st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }

            EmbedAsset *asset = out_assets->AppendDefault();

            asset->name = DuplicateString(prop.section, alloc).ptr;
            asset->compression_type = AdaptCompression(asset->name, compression_type);

            do {
                if (prop.key == "CompressionType") {
                    if (!OptionToEnumI(CompressionTypeNames, prop.value, &asset->compression_type)) {
                        LogError("Unknown compression type '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "File") {
                    asset->src_filename = DuplicateString(prop.value, alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (!asset->src_filename) {
                LogError("Missing File attribute");
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    out_guard.Disable();
    return true;
}

bool ResolveAssets(Span<const char *const> filenames, int strip_count,
                   CompressionType compression_type, EmbedAssetSet *out_set)
{
    Size prev_len = out_set->assets.len;
    RG_DEFER_N(out_guard) { out_set->assets.RemoveFrom(prev_len); };

    for (const char *filename: filenames) {
        if (filename[0] == '@') {
            if (!LoadMetaFile(filename + 1, compression_type, &out_set->str_alloc, &out_set->assets))
                return false;
        } else {
            EmbedAsset *asset = out_set->assets.AppendDefault();

            asset->name = StripDirectoryComponents(filename, strip_count);
            asset->compression_type = AdaptCompression(filename, compression_type);
            asset->src_filename = filename;
        }
    }

    // Deduplicate assets
    {
        HashSet<const char *> known_filenames;

        Size j = prev_len;
        for (Size i = prev_len; i < out_set->assets.len; i++) {
            const EmbedAsset &asset = out_set->assets[i];

            out_set->assets[j] = out_set->assets[i];

            bool inserted;
            known_filenames.TrySet(asset.src_filename, &inserted);

            j += inserted;
        }
        out_set->assets.len = j;
    }

    out_guard.Disable();
    return true;
}

static Size WriteAsset(const EmbedAsset &asset, FunctionRef<void(Span<const uint8_t> buf)> func)
{
    Size compressed_len = 0;
    StreamWriter compressor([&](Span<const uint8_t> buf) {
        func(buf);
        compressed_len += buf.len;

        return true;
    }, nullptr, asset.compression_type);

    if (!compressor.IsValid())
        return -1;

    // Pass through
    {
        StreamReader reader(asset.src_filename);

        if (!SpliceStream(&reader, -1, &compressor))
            return -1;
    }

    bool success = compressor.Close();
    RG_ASSERT(success);

    return compressed_len;
}

static void PrintAsLiterals(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    // Inspired by https://gitlab.com/mbitsnbites/lsb2s

    static const char *const LookupTable =
        "\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\x08\\x09\\x0A\\x0B\\x0C\\x0D\\x0E\\x0F"
        "\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1A\\x1B\\x1C\\x1D\\x1E\\x1F"
        "\\x20\\x21\\x22\\x23\\x24\\x25\\x26\\x27\\x28\\x29\\x2A\\x2B\\x2C\\x2D\\x2E\\x2F"
        "\\x30\\x31\\x32\\x33\\x34\\x35\\x36\\x37\\x38\\x39\\x3A\\x3B\\x3C\\x3D\\x3E\\x3F"
        "\\x40\\x41\\x42\\x43\\x44\\x45\\x46\\x47\\x48\\x49\\x4A\\x4B\\x4C\\x4D\\x4E\\x4F"
        "\\x50\\x51\\x52\\x53\\x54\\x55\\x56\\x57\\x58\\x59\\x5A\\x5B\\x5C\\x5D\\x5E\\x5F"
        "\\x60\\x61\\x62\\x63\\x64\\x65\\x66\\x67\\x68\\x69\\x6A\\x6B\\x6C\\x6D\\x6E\\x6F"
        "\\x70\\x71\\x72\\x73\\x74\\x75\\x76\\x77\\x78\\x79\\x7A\\x7B\\x7C\\x7D\\x7E\\x7F"
        "\\x80\\x81\\x82\\x83\\x84\\x85\\x86\\x87\\x88\\x89\\x8A\\x8B\\x8C\\x8D\\x8E\\x8F"
        "\\x90\\x91\\x92\\x93\\x94\\x95\\x96\\x97\\x98\\x99\\x9A\\x9B\\x9C\\x9D\\x9E\\x9F"
        "\\xA0\\xA1\\xA2\\xA3\\xA4\\xA5\\xA6\\xA7\\xA8\\xA9\\xAA\\xAB\\xAC\\xAD\\xAE\\xAF"
        "\\xB0\\xB1\\xB2\\xB3\\xB4\\xB5\\xB6\\xB7\\xB8\\xB9\\xBA\\xBB\\xBC\\xBD\\xBE\\xBF"
        "\\xC0\\xC1\\xC2\\xC3\\xC4\\xC5\\xC6\\xC7\\xC8\\xC9\\xCA\\xCB\\xCC\\xCD\\xCE\\xCF"
        "\\xD0\\xD1\\xD2\\xD3\\xD4\\xD5\\xD6\\xD7\\xD8\\xD9\\xDA\\xDB\\xDC\\xDD\\xDE\\xDF"
        "\\xE0\\xE1\\xE2\\xE3\\xE4\\xE5\\xE6\\xE7\\xE8\\xE9\\xEA\\xEB\\xEC\\xED\\xEE\\xEF"
        "\\xF0\\xF1\\xF2\\xF3\\xF4\\xF5\\xF6\\xF7\\xF8\\xF9\\xFA\\xFB\\xFC\\xFD\\xFE\\xFF";

    Size i = 0;

    for (; i < bytes.len / 16 * 16; i += 16) {
        char buf[66] = {};

        buf[0] = '"';
        MemCpy(buf + 1, LookupTable + 4 * bytes[i + 0], 4);
        MemCpy(buf + 5, LookupTable + 4 * bytes[i + 1], 4);
        MemCpy(buf + 9, LookupTable + 4 * bytes[i + 2], 4);
        MemCpy(buf + 13, LookupTable + 4 * bytes[i + 3], 4);
        MemCpy(buf + 17, LookupTable + 4 * bytes[i + 4], 4);
        MemCpy(buf + 21, LookupTable + 4 * bytes[i + 5], 4);
        MemCpy(buf + 25, LookupTable + 4 * bytes[i + 6], 4);
        MemCpy(buf + 29, LookupTable + 4 * bytes[i + 7], 4);
        MemCpy(buf + 33, LookupTable + 4 * bytes[i + 8], 4);
        MemCpy(buf + 37, LookupTable + 4 * bytes[i + 9], 4);
        MemCpy(buf + 41, LookupTable + 4 * bytes[i + 10], 4);
        MemCpy(buf + 45, LookupTable + 4 * bytes[i + 11], 4);
        MemCpy(buf + 49, LookupTable + 4 * bytes[i + 12], 4);
        MemCpy(buf + 53, LookupTable + 4 * bytes[i + 13], 4);
        MemCpy(buf + 57, LookupTable + 4 * bytes[i + 14], 4);
        MemCpy(buf + 61, LookupTable + 4 * bytes[i + 15], 4);
        buf[65] = '"';

        out_st->Write(MakeSpan(buf, RG_SIZE(buf)));
    }

    if (i < bytes.len) {
        out_st->Write("\"");
        do {
            Span<const char> part = MakeSpan(LookupTable + (int)bytes[i] * 4, 4);
            out_st->Write(part);
        } while (++i < bytes.len);
        out_st->Write("\"");
    }
}

static void PrintAsArray(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    // Inspired by https://gitlab.com/mbitsnbites/lsb2s

    static const char *const LookupTable =
        "0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,"
        "0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,"
        "0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,"
        "0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,"
        "0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,"
        "0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,"
        "0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,"
        "0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,"
        "0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,"
        "0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,"
        "0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,"
        "0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,"
        "0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,"
        "0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,"
        "0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,"
        "0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF,";

    Size i = 0;

    for (; i < bytes.len / 16 * 16; i += 16) {
        char buf[80] = {};

        MemCpy(buf + 0, LookupTable + 5 * bytes[i + 0], 5);
        MemCpy(buf + 5, LookupTable + 5 * bytes[i + 1], 5);
        MemCpy(buf + 10, LookupTable + 5 * bytes[i + 2], 5);
        MemCpy(buf + 15, LookupTable + 5 * bytes[i + 3], 5);
        MemCpy(buf + 20, LookupTable + 5 * bytes[i + 4], 5);
        MemCpy(buf + 25, LookupTable + 5 * bytes[i + 5], 5);
        MemCpy(buf + 30, LookupTable + 5 * bytes[i + 6], 5);
        MemCpy(buf + 35, LookupTable + 5 * bytes[i + 7], 5);
        MemCpy(buf + 40, LookupTable + 5 * bytes[i + 8], 5);
        MemCpy(buf + 45, LookupTable + 5 * bytes[i + 9], 5);
        MemCpy(buf + 50, LookupTable + 5 * bytes[i + 10], 5);
        MemCpy(buf + 55, LookupTable + 5 * bytes[i + 11], 5);
        MemCpy(buf + 60, LookupTable + 5 * bytes[i + 12], 5);
        MemCpy(buf + 65, LookupTable + 5 * bytes[i + 13], 5);
        MemCpy(buf + 70, LookupTable + 5 * bytes[i + 14], 5);
        MemCpy(buf + 75, LookupTable + 5 * bytes[i + 15], 5);

        out_st->Write(MakeSpan(buf, RG_SIZE(buf)));
    }

    for (; i < bytes.len; i++) {
        Span<const char> part = MakeSpan(LookupTable + (int)bytes[i] * 5, 5);
        out_st->Write(part);
    }
}

static const char *MakeVariableName(const char *name, Allocator *alloc)
{
    HeapArray<char> buf(alloc);
    bool up = true;

    if (!IsAsciiAlpha(name[0]) && name[0] != '_') {
        buf.Append('_');
    }
    for (Size i = 0; name[i]; i++) {
        int c = name[i];

        if (IsAsciiAlphaOrDigit(c)) {
            buf.Append(up ? UpperAscii(c) : (char)c);
            up = false;
        } else {
            up = true;
        }
    }

    buf.Grow(1);
    buf.ptr[buf.len] = 0;

    return buf.Leak().ptr;
}

bool PackAssets(Span<const EmbedAsset> assets, unsigned int flags, const char *output_path)
{
    BlockAllocator temp_alloc;

    if ((flags & (int)EmbedFlag::UseEmbed) && (flags & (int)EmbedFlag::UseLiterals)) {
        LogError("Cannot use both UseEmbed and UseLiterals flags");
        return false;
    }

    StreamWriter c;
    StreamWriter bin;
    if (output_path) {
        if (!c.Open(output_path))
            return false;

        if (flags & (int)EmbedFlag::UseEmbed) {
            const char *bin_filename = Fmt(&temp_alloc, "%1.bin", output_path).ptr;

            if (!bin.Open(bin_filename))
                return false;
        }
    } else {
        if (flags & (int)EmbedFlag::UseEmbed) {
            LogError("You must use an explicit output path for UseEmbed");
            return false;
        }

        if (!c.Open(STDOUT_FILENO, "<stdout>"))
            return false;
    }

    PrintLn(&c, CodePrefix);

    // Work around the ridiculousness of C++ not liking empty arrays
    HeapArray<BlobInfo> blobs;
    if (assets.len) {
        std::function<void(Span<const uint8_t>)> print;

        PrintLn(&c, R"(
static const uint8_t raw_data[] = {)");

        if (flags & (int)EmbedFlag::UseEmbed) {
            PrintLn(&c, "    #embed \"%1.bin\"", output_path);
            print = [&](Span<const uint8_t> buf) { bin.Write(buf); };
        } else if (flags & (int)EmbedFlag::UseLiterals) {
            print = [&](Span<const uint8_t> buf) { PrintAsLiterals(buf, &c); };
        } else {
            print = [&](Span<const uint8_t> buf) { PrintAsArray(buf, &c); };
        }

        // Embed assets and source maps
        for (const EmbedAsset &asset: assets) {
            BlobInfo blob = {};

            blob.name = asset.name;
            blob.compression_type = asset.compression_type;

            if (flags & (int)EmbedFlag::UseEmbed) {
                blob.len = WriteAsset(asset, print);
                if (blob.len < 0)
                    return false;

                // Put NUL byte at the end to make it a valid C string
                print(0);
            } else {
                PrintLn(&c, "    // %1", blob.name);
                Print(&c, "    ");
                blob.len = WriteAsset(asset, print);
                if (blob.len < 0)
                    return false;

                // Put NUL byte at the end to make it a valid C string
                print(0);
                PrintLn(&c);
            }

            blobs.Append(blob);
        }

        PrintLn(&c, "};");
    }

    if (!(flags & (int)EmbedFlag::NoArray)) {
        PrintLn(&c);

        PrintLn(&c, "EXPORT_SYMBOL EXTERN const Span EmbedAssets;");
        if (assets.len) {
            PrintLn(&c, "static AssetInfo assets[%1] = {", blobs.len);

            // Write asset table
            for (Size i = 0, raw_offset = 0; i < blobs.len; i++) {
                const BlobInfo &blob = blobs[i];

                PrintLn(&c, "    { \"%1\", %2, { raw_data + %3, %4 } },",
                             blob.name, (int)blob.compression_type, raw_offset, blob.len);

                raw_offset += blob.len + 1;
            }

            PrintLn(&c, "};");
        }
        PrintLn(&c, "const Span EmbedAssets = { %1, %2 };", blobs.len ? "assets" : "0", blobs.len);
    }

    if (!(flags & (int)EmbedFlag::NoSymbols)) {
        PrintLn(&c);

        for (Size i = 0, raw_offset = 0; i < blobs.len; i++) {
            const BlobInfo &blob = blobs[i];
            const char *var = MakeVariableName(blob.name, &temp_alloc);

            PrintLn(&c, "EXPORT_SYMBOL EXTERN const AssetInfo %1;", var);
            PrintLn(&c, "const AssetInfo %1 = { \"%2\", %3, { raw_data + %4, %5 } };",
                         var, blob.name, (int)blob.compression_type, raw_offset, blob.len);

            raw_offset += blob.len + 1;
        }
    }

    if (!c.Close())
        return false;
    if ((flags & (int)EmbedFlag::UseEmbed) && !bin.Close())
        return false;

    return true;
}

}
