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

#include "../core/libcc/libcc.hh"
#include "../core/libwrap/json.hh"
#include "pack.hh"

namespace RG {

// For simplicity, I've replicated the required data structures from libcc
// and packer.hh directly below. Don't forget to keep them in sync.
static const char *const CodePrefix =
R"(// This program is free software: you can redistribute it and/or modify
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

    const char *source_map;
} AssetInfo;)";

struct BlobInfo {
    const char *name;

    CompressionType compression_type;
    Size len;

    const char *source_map;
};

static FmtArg FormatZigzagVLQ64(int value)
{
    RG_ASSERT(value != INT_MIN);

    static const char literals[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    RG_STATIC_ASSERT(RG_SIZE(literals) - 1 == 64);
    RG_STATIC_ASSERT((RG_SIZE(FmtArg::u.buf) - 1) * 6 - 2 >= RG_SIZE(value) * 16);

    FmtArg arg;
    arg.type = FmtType::Buffer;

    // First character
    unsigned int u;
    if (value >= 0) {
        u = value >> 4;
        arg.u.buf[0] = literals[(value & 0xF) << 1 | (u ? 0x20 : 0)];
    } else {
        value = -value;
        u = value >> 4;
        arg.u.buf[0] = literals[((value & 0xF) << 1) | (u ? 0x21 : 0x1)];
    }

    // Remaining characters
    Size len = 1;
    while (u) {
        unsigned int idx = u & 0x1F;
        u >>= 5;
        arg.u.buf[len++] = literals[idx | (u ? 0x20 : 0)];
    }

    arg.u.buf[len] = 0;

    return arg;
}

static int CountNewLines(Span<const char> buf)
{
    int lines = 0;
    for (Size i = 0;; lines++) {
        const char *ptr = (const char *)memchr(buf.ptr + i, '\n', buf.len - i);
        if (!ptr)
            break;
        i = ptr - buf.ptr + 1;
    }

    return lines;
}

static bool BuildJavaScriptMap3(Span<const PackSourceInfo> sources, StreamWriter *out_writer)
{
    json_Writer writer(out_writer);

    writer.StartObject();

    writer.Key("version"); writer.Int(3);
    writer.Key("sources"); writer.StartArray();
    for (const PackSourceInfo &src: sources) {
        const char *basename = SplitStrReverseAny(src.name, RG_PATH_SEPARATORS).ptr;
        writer.String(basename);
    }
    writer.EndArray();
    writer.Key("names"); writer.StartArray(); writer.EndArray();

    writer.Key("mappings"); writer.StartString();
    for (Size i = 0, prev_lines = 0; i < sources.len; i++) {
        const PackSourceInfo &src = sources[i];

        Size lines = 0;
        {
            StreamReader reader(src.filename);
            do {
                LocalArray<char, 16384> buf;
                buf.len = reader.Read(buf.data);
                if (buf.len < 0)
                    return false;

                lines += CountNewLines(buf);
            } while (!reader.IsEOF());
        }

        Print(out_writer, "%1", FmtArg(";").Repeat(CountNewLines(src.prefix)));
        if (lines) {
            Print(out_writer, "A%1%2A;", i ? "C" : "A", FormatZigzagVLQ64((int)-prev_lines));
            lines--;

            for (Size j = 0; j < lines; j++) {
                Print(out_writer, "AACA;");
            }
        }
        Print(out_writer, "%1", FmtArg(";").Repeat(CountNewLines(src.suffix)));

        prev_lines = lines;
    }
    writer.EndString();

    writer.EndObject();

    return true;
}

static bool MergeAssetSourceFiles(Span<const PackSourceInfo> sources,
                                  FunctionRef<void(Span<const uint8_t> buf)> func)
{
    for (const PackSourceInfo &src: sources) {
        func(Span<const char>(src.prefix).CastAs<const uint8_t>());

        StreamReader reader(src.filename);
        do {
            LocalArray<uint8_t, 16384> read_buf;
            read_buf.len = reader.Read(read_buf.data);
            if (read_buf.len < 0)
                return false;

            func(read_buf);
        } while (!reader.IsEOF());

        func(Span<const char>(src.suffix).CastAs<const uint8_t>());
    }

    return true;
}

static Size PackAsset(const PackAssetInfo &asset, FunctionRef<void(Span<const uint8_t> buf)> func)
{
    Size compressed_len = 0;
    StreamWriter compressor([&](Span<const uint8_t> buf) { 
        func(buf);
        compressed_len += buf.len;

        return true;
    }, nullptr, asset.compression_type);

    if (asset.transform_cmd) {
        Span<const uint8_t> bridge = {};

        // Create user-level thread to run merge function
        Fiber merger([&]() {
            const auto write = [&](Span<const uint8_t> buf) {
                bridge = buf;
                Fiber::SwitchBack();
            };

            return MergeAssetSourceFiles(asset.sources, write);
        });

        // Run transform command
        {
            const auto read = [&]() {
                bridge = {};
                merger.SwitchTo();
                return bridge;
            };
            const auto write = [&](Span<const uint8_t> buf) { compressor.Write(buf); };

            int code;
            if (!ExecuteCommandLine(asset.transform_cmd, read, write, &code))
                return -1;

            if (code) {
                LogError("Transform command '%1' failed with code: %2", asset.transform_cmd, code);
                return -1;
            }
        }

        if (!merger.Finalize())
            return -1;
    } else {
        const auto write = [&](Span<const uint8_t> buf) { compressor.Write(buf); };

        if (!MergeAssetSourceFiles(asset.sources, write))
            return -1;
    }

    bool success = compressor.Close();
    RG_ASSERT(success);

    return compressed_len;
}

static Size PackSourceMap(const PackAssetInfo &asset, FunctionRef<void(Span<const uint8_t> buf)> func)
{
    RG_ASSERT(!asset.transform_cmd);

    HeapArray<uint8_t> buf;
    StreamWriter writer(&buf, nullptr, asset.compression_type);

    switch (asset.source_map_type) {
        case SourceMapType::None: { RG_UNREACHABLE(); } break;
        case SourceMapType::JSv3: {
            if (!BuildJavaScriptMap3(asset.sources, &writer))
                return false;
        } break;
    }

    bool success = writer.Close();
    RG_ASSERT(success);
    func(buf);

    return buf.len;
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

bool PackAssets(Span<const PackAssetInfo> assets, unsigned int flags, const char *output_path)
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
        for (const PackAssetInfo &asset: assets) {
            BlobInfo blob = {};

            blob.name = asset.name;
            blob.compression_type = asset.compression_type;

            PrintLn(&st, "    // %1", blob.name);
            Print(&st, "    ");
            blob.len = PackAsset(asset, print);
            if (blob.len < 0)
                return false;

            // Put NUL byte at the end to make it a valid C string
            print(0);
            PrintLn(&st);

            if (asset.source_map_name) {
                const char *basename = SplitStrReverseAny(asset.source_map_name, RG_PATH_SEPARATORS).ptr;
                blob.source_map = basename;

                BlobInfo blob_map = {};

                blob_map.name = asset.source_map_name;
                blob_map.compression_type = asset.compression_type;
                PrintLn(&st, "    // %1", blob_map.name);
                Print(&st, "    ");
                blob_map.len = PackSourceMap(asset, print);
                if (blob_map.len < 0)
                    return false;

                print(0);
                PrintLn(&st);

                blobs.Append(blob);
                blobs.Append(blob_map);
            } else {
                blobs.Append(blob);
            }
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

                if (blob.source_map) {
                    PrintLn(&st, "    {\"%1\", %2, {raw_data + %3, %4}, \"%5\"},",
                                 blob.name, (int)blob.compression_type, raw_offset, blob.len,
                                 blob.source_map);
                } else {
                    PrintLn(&st, "    {\"%1\", %2, {raw_data + %3, %4}, 0},",
                                 blob.name, (int)blob.compression_type, raw_offset, blob.len);
                }
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

            if (blob.source_map) {
                PrintLn(&st, "EXPORT_SYMBOL EXTERN_SYMBOL const AssetInfo %1;", var);
                PrintLn(&st, "const AssetInfo %1 = {\"%2\", %3, {raw_data + %4, %5}, \"%6\"};",
                             var, blob.name, (int)blob.compression_type, raw_offset,
                             blob.len, blob.source_map);
            } else {
                PrintLn(&st, "EXPORT_SYMBOL EXTERN_SYMBOL const AssetInfo %1;", var);
                PrintLn(&st, "const AssetInfo %1 = {\"%2\", %3, {raw_data + %4, %5}, 0};",
                             var, blob.name, (int)blob.compression_type, raw_offset, blob.len);
            }
            raw_offset += blob.len + 1;
        }
    }

    return st.Close();
}

}
