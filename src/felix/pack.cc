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
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "src/core/libwrap/json.hh"
#include "pack.hh"

namespace RG {

// For simplicity, I've replicated the required data structures from libcc
// and packer.hh directly below. Don't forget to keep them in sync.
static const char *const CodePrefix =
R"(// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
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

#ifdef EXPORT
    #ifdef _WIN32
        #define EXPORT_SYMBOL __declspec(dllexport)
    #else
        #define EXPORT_SYMBOL __attribute__((visibility("default")))
    #endif
#else
    #define EXPORT_SYMBOL
#endif
#ifdef __cplusplus
    #define EXTERN_SYMBOL extern "C"
#else
    #define EXTERN_SYMBOL extern
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

static bool MergeAssetSourceFiles(Span<const PackSource> sources, StreamWriter *writer)
{
    for (const PackSource &src: sources) {
        writer->Write(Span<const char>(src.prefix).As<const uint8_t>());

        StreamReader reader(src.filename);
        do {
            LocalArray<uint8_t, 16384> read_buf;
            read_buf.len = reader.Read(read_buf.data);
            if (read_buf.len < 0)
                return false;

            if (!writer->Write(read_buf))
                return false;
        } while (!reader.IsEOF());

        writer->Write(Span<const char>(src.suffix).As<const uint8_t>());
    }
    if (!writer->IsValid())
        return false;

    return true;
}

static Size WriteAsset(const PackAsset &asset, FunctionRef<void(Span<const uint8_t> buf)> func)
{
    Size compressed_len = 0;
    StreamWriter compressor([&](Span<const uint8_t> buf) { 
        func(buf);
        compressed_len += buf.len;

        return true;
    }, nullptr, asset.compression_type);

    if (!compressor.IsValid())
        return -1;

    if (!MergeAssetSourceFiles(asset.sources, &compressor))
        return -1;

    bool success = compressor.Close();
    RG_ASSERT(success);

    return compressed_len;
}

static void PrintAsLiterals(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    Size i = 0;
    for (Size end = bytes.len / 8 * 8; i < end; i += 8) {
        Print(out_st, "\"\\x%1\\x%2\\x%3\\x%4\\x%5\\x%6\\x%7\\x%8\" ",
                      FmtHex(bytes[i + 0]).Pad0(-2), FmtHex(bytes[i + 1]).Pad0(-2),
                      FmtHex(bytes[i + 2]).Pad0(-2), FmtHex(bytes[i + 3]).Pad0(-2),
                      FmtHex(bytes[i + 4]).Pad0(-2), FmtHex(bytes[i + 5]).Pad0(-2),
                      FmtHex(bytes[i + 6]).Pad0(-2), FmtHex(bytes[i + 7]).Pad0(-2));
    }

    if (i < bytes.len) {
        Print(out_st, "\"");
        for (; i < bytes.len; i++) {
            Print(out_st, "\\x%1", FmtHex(bytes[i]).Pad0(-2));
        }
        Print(out_st, "\" ");
    }
}

static void PrintAsArray(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    Size i = 0;
    for (Size end = bytes.len / 8 * 8; i < end; i += 8) {
        Print(out_st, "0x%1, 0x%2, 0x%3, 0x%4, 0x%5, 0x%6, 0x%7, 0x%8, ",
                      FmtHex(bytes[i + 0]).Pad0(-2), FmtHex(bytes[i + 1]).Pad0(-2),
                      FmtHex(bytes[i + 2]).Pad0(-2), FmtHex(bytes[i + 3]).Pad0(-2),
                      FmtHex(bytes[i + 4]).Pad0(-2), FmtHex(bytes[i + 5]).Pad0(-2),
                      FmtHex(bytes[i + 6]).Pad0(-2), FmtHex(bytes[i + 7]).Pad0(-2));
    }
    for (; i < bytes.len; i++) {
        Print(out_st, "0x%1, ", FmtHex(bytes[i]).Pad0(-2));
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

bool PackAssets(Span<const PackAsset> assets, unsigned int flags, const char *output_path)
{
    BlockAllocator temp_alloc;

    StreamWriter st;
    if (output_path) {
        st.Open(output_path);
    } else {
        st.Open(stdout, "<stdout>");
    }
    if (!st.IsValid())
        return false;

    PrintLn(&st, CodePrefix);

    // Work around the ridiculousness of C++ not liking empty arrays
    HeapArray<BlobInfo> blobs;
    if (assets.len) {
        bool use_literals = flags & (int)PackFlag::UseLiterals;

        std::function<void(Span<const uint8_t>)> print;
        if (use_literals) {
            print = [&](Span<const uint8_t> buf) { PrintAsLiterals(buf, &st); };
        } else {
            print = [&](Span<const uint8_t> buf) { PrintAsArray(buf, &st); };
        }

        PrintLn(&st, R"(
static const uint8_t raw_data[] = {)");

        // Pack assets and source maps
        for (const PackAsset &asset: assets) {
            BlobInfo blob = {};

            blob.name = asset.name;
            blob.compression_type = asset.compression_type;

            PrintLn(&st, "    // %1", blob.name);
            Print(&st, "    ");
            blob.len = WriteAsset(asset, print);
            if (blob.len < 0)
                return false;

            // Put NUL byte at the end to make it a valid C string
            print(0);
            PrintLn(&st);

            blobs.Append(blob);
        }

        PrintLn(&st, "};");
    }

    if (!(flags & (int)PackFlag::NoArray)) {
        PrintLn(&st);

        PrintLn(&st, "EXPORT_SYMBOL EXTERN_SYMBOL const Span PackedAssets;");
        if (assets.len) {
            PrintLn(&st, "static AssetInfo assets[%1] = {", blobs.len);

            // Write asset table
            for (Size i = 0, raw_offset = 0; i < blobs.len; i++) {
                const BlobInfo &blob = blobs[i];

                PrintLn(&st, "    {\"%1\", %2, { raw_data + %3, %4 }},",
                             blob.name, (int)blob.compression_type, raw_offset, blob.len);

                raw_offset += blob.len + 1;
            }

            PrintLn(&st, "};");
        }
        PrintLn(&st, "const Span PackedAssets = {%1, %2};", blobs.len ? "assets" : "0", blobs.len);
    }

    if (!(flags & (int)PackFlag::NoSymbols)) {
        PrintLn(&st);

        for (Size i = 0, raw_offset = 0; i < blobs.len; i++) {
            const BlobInfo &blob = blobs[i];
            const char *var = MakeVariableName(blob.name, &temp_alloc);

            PrintLn(&st, "EXPORT_SYMBOL EXTERN_SYMBOL const AssetInfo %1;", var);
            PrintLn(&st, "const AssetInfo %1 = {\"%2\", %3, { raw_data + %4, %5 }};",
                         var, blob.name, (int)blob.compression_type, raw_offset, blob.len);

            raw_offset += blob.len + 1;
        }
    }

    return st.Close();
}

}
