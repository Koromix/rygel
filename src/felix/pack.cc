// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "../core/libwrap/json.hh"
#include "pack.hh"

namespace RG {

// For simplicity, I've replicated the required data structures from libcc
// and packer.hh directly below. Don't forget to keep them in sync.
static const char *const CodePrefix =
R"(// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
            while (!reader.IsEOF()) {
                LocalArray<char, 16384> buf;
                buf.len = reader.Read(buf.data);
                if (buf.len < 0)
                    return false;

                lines += CountNewLines(buf);
            }
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
        while (!reader.IsEOF()) {
            LocalArray<uint8_t, 16384> read_buf;
            read_buf.len = reader.Read(read_buf.data);
            if (read_buf.len < 0)
                return false;

            func(read_buf);
        }

        func(Span<const char>(src.suffix).CastAs<const uint8_t>());
    }

    return true;
}

static Size PackAsset(const PackAssetInfo &asset, FunctionRef<void(Span<const uint8_t> buf)> func)
{
    HeapArray<uint8_t> compressed_buf;
    StreamWriter compressor(&compressed_buf, nullptr, asset.compression_type);

    Size written_len = 0;
    const auto flush_compressor_buffer = [&]() {
        func(compressed_buf);
        written_len += compressed_buf.len;
        compressed_buf.RemoveFrom(0);
    };

    if (asset.transform_cmd) {
        // XXX: Implement some kind of stream API for external process input / output
        HeapArray<uint8_t> merge_buf;
        if (!MergeAssetSourceFiles(asset.sources, [&](Span<const uint8_t> buf) { merge_buf.Append(buf); }))
            return -1;

        // Execute transform command
        {
            int code;
            bool success = ExecuteCommandLine(asset.transform_cmd, merge_buf,
                                              [&](Span<const uint8_t> buf) {
                compressor.Write(buf);
                flush_compressor_buffer();
            }, &code);
            if (!success)
                return -1;

            if (code) {
                LogError("Transform command '%1' failed with code: %2", asset.transform_cmd, code);
                return -1;
            }
        }
    } else {
        bool success = MergeAssetSourceFiles(asset.sources, [&](Span<const uint8_t> buf) {
            compressor.Write(buf);
            flush_compressor_buffer();
        });
        if (!success)
            return -1;
    }

    bool success = compressor.Close();
    RG_ASSERT(success);
    flush_compressor_buffer();

    return written_len;
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

static void PrintAsC(Span<const uint8_t> bytes, bool as_array, StreamWriter *out_st)
{
    if (as_array) {
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
    } else {
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
}

bool PackToC(Span<const PackAssetInfo> assets, bool use_arrays, const char *output_path)
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
    if (assets.len) {
        PrintLn(&st, R"(
static const uint8_t raw_data[] = {)");

        // Pack assets and source maps
        HeapArray<BlobInfo> blobs;
        for (const PackAssetInfo &asset: assets) {
            BlobInfo blob = {};
            blob.name = asset.name;

            blob.compression_type = asset.compression_type;

            PrintLn(&st, "    // %1", blob.name);
            Print(&st, "    ");
            blob.len = PackAsset(asset, [&](Span<const uint8_t> buf) { PrintAsC(buf, use_arrays, &st); });
            if (blob.len < 0)
                return false;

            // Put NUL byte at the end to make it a valid C string
            PrintAsC(0, use_arrays, &st);
            PrintLn(&st);

            if (asset.source_map_name) {
                const char *basename = SplitStrReverseAny(asset.source_map_name, RG_PATH_SEPARATORS).ptr;
                blob.source_map = basename;

                BlobInfo blob_map = {};

                blob_map.name = asset.source_map_name;
                blob_map.compression_type = asset.compression_type;
                PrintLn(&st, "    // %1", blob_map.name);
                Print(&st, "    ");
                blob_map.len = PackSourceMap(asset, [&](Span<const uint8_t> buf) { PrintAsC(buf, use_arrays, &st); });
                if (blob_map.len < 0)
                    return false;

                PrintAsC(0, use_arrays, &st);
                PrintLn(&st);

                blobs.Append(blob);
                blobs.Append(blob_map);
            } else {
                blobs.Append(blob);
            }
        }

        PrintLn(&st, R"(};

static AssetInfo assets[%1] = {)", blobs.len);

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

        PrintLn(&st, R"(};

EXPORT_SYMBOL extern const Span PackedAssets;
const Span PackedAssets = {assets, %1};)", blobs.len);
    } else {
        PrintLn(&st, R"(
EXPORT_SYMBOL extern const Span PackedAssets;
const Span PackedAssets = {0, 0};)");
    }

    return st.Close();
}

bool PackToFiles(Span<const PackAssetInfo> assets, const char *output_path)
{
    BlockAllocator temp_alloc;

    if (!output_path) {
        LogError("Output directory was not specified");
        return false;
    }
    if (!MakeDirectory(output_path, false))
        return false;

    for (const PackAssetInfo &asset: assets) {
        StreamWriter st;

        if (RG_UNLIKELY(PathIsAbsolute(asset.name))) {
            LogError("Asset name '%1' cannot be an absolute path", asset.name);
            return false;
        }
        if (RG_UNLIKELY(PathContainsDotDot(asset.name))) {
            LogError("Asset name '%1' must not contain '..'", asset.name);
            return false;
        }

        const char *compression_ext = nullptr;
        switch (asset.compression_type) {
            case CompressionType::None: { compression_ext = ""; } break;
            case CompressionType::Gzip: { compression_ext = ".gz"; } break;
            case CompressionType::Zlib: {
                LogError("This generator cannot use Zlib compression");
                return false;
            } break;
        }
        RG_ASSERT(compression_ext);

        const char *filename = Fmt(&temp_alloc, "%1%/%2%3", output_path, asset.name, compression_ext).ptr;

        if (!EnsureDirectoryExists(filename))
            return false;

        if (!st.Open(filename))
            return false;
        if (PackAsset(asset, [&](Span<const uint8_t> buf) { st.Write(buf); }) < 0)
            return false;
        if (!st.Close())
            return false;

        if (asset.source_map_name) {
            const char *map_filename = Fmt(&temp_alloc, "%1%/%2%3", output_path,
                                           asset.source_map_name, compression_ext).ptr;

            if (!st.Open(map_filename))
                return false;
            if (PackSourceMap(asset, [&](Span<const uint8_t> buf) { st.Write(buf); }) < 0)
                return false;
            if (!st.Close())
                return false;
        }
    }

    return true;
}

bool PackAssets(Span<const PackAssetInfo> assets, const char *output_path, PackMode mode)
{
    switch (mode) {
        case PackMode::C: return PackToC(assets, false, output_path);
        case PackMode::Carray: return PackToC(assets, true, output_path);
        case PackMode::Files: return PackToFiles(assets, output_path);
    }

    RG_UNREACHABLE();
}

}
