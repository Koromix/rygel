// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "../wrappers/json.hh"
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

#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
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
    const char *str_name;
    const char *var_name;
    Size len;

    const char *source_map;
};

static FmtArg FormatZigzagVLQ64(int value)
{
    RG_ASSERT_DEBUG(value != INT_MIN);

    static const char literals[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    RG_STATIC_ASSERT(RG_SIZE(literals) - 1 == 64);
    RG_STATIC_ASSERT((RG_SIZE(FmtArg::value.buf) - 1) * 6 - 2 >= RG_SIZE(value) * 16);

    FmtArg arg;
    arg.type = FmtArg::Type::Buffer;

    // First character
    unsigned int u;
    if (value >= 0) {
        u = value >> 4;
        arg.value.buf[0] = literals[(value & 0xF) << 1 | (u ? 0x20 : 0)];
    } else {
        value = -value;
        u = value >> 4;
        arg.value.buf[0] = literals[((value & 0xF) << 1) | (u ? 0x21 : 0x1)];
    }

    // Remaining characters
    Size len = 1;
    while (u) {
        unsigned int idx = u & 0x1F;
        u >>= 5;
        arg.value.buf[len++] = literals[idx | (u ? 0x20 : 0)];
    }

    arg.value.buf[len] = 0;

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
        writer.String(src.name);
    }
    writer.EndArray();
    writer.Key("names"); writer.StartArray(); writer.EndArray();

    writer.Key("mappings"); writer.Flush(); out_writer->Write(":\"");
    for (Size i = 0, prev_lines = 0; i < sources.len; i++) {
        const PackSourceInfo &src = sources[i];

        Size lines = 0;
        {
            StreamReader reader(src.filename);
            while (!reader.eof) {
                char buf[128 * 1024];
                Size len = reader.Read(RG_SIZE(buf), &buf);
                if (len < 0)
                    return false;

                lines += CountNewLines(MakeSpan(buf, len));
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
    out_writer->Write('"');

    writer.EndObject();

    return true;
}

static Size PackAsset(const PackAssetInfo &asset, CompressionType compression_type,
                      std::function<void(Span<const uint8_t> buf)> func)
{
    Size written_len = 0;
    {
        HeapArray<uint8_t> buf;
        StreamWriter writer(&buf, nullptr, compression_type);

        const auto flush_buffer = [&]() {
            written_len += buf.len;
            func(buf);
            buf.RemoveFrom(0);
        };

        for (const PackSourceInfo &src: asset.sources) {
            writer.Write(src.prefix);

            StreamReader reader(src.filename);
            while (!reader.eof) {
                uint8_t read_buf[128 * 1024];
                Size read_len = reader.Read(RG_SIZE(read_buf), read_buf);
                if (read_len < 0)
                    return -1;

                RG_ASSERT(writer.Write(read_buf, read_len));
                flush_buffer();
            }

            writer.Write(src.suffix);
        }

        RG_ASSERT(writer.Close());
        flush_buffer();
    }

    return written_len;
}

static Size PackSourceMap(Span<const PackSourceInfo> sources, SourceMapType source_map_type,
                          CompressionType compression_type, std::function<void(Span<const uint8_t> buf)> func)
{
    HeapArray<uint8_t> buf;
    StreamWriter writer(&buf, nullptr, compression_type);

    switch (source_map_type) {
        case SourceMapType::None: {} break;
        case SourceMapType::JSv3: {
            if (!BuildJavaScriptMap3(sources, &writer))
                return false;
        } break;
    }

    RG_ASSERT(writer.Close());
    func(buf);

    return buf.len;
}

static const char *CreateVariableName(const char *name, Allocator *alloc)
{
    char *var_name = DuplicateString(name, alloc).ptr;

    for (Size i = 0; var_name[i]; i++) {
        var_name[i] = IsAsciiAlphaOrDigit(var_name[i]) ? var_name[i] : '_';
    }

    return var_name;
}

static void PrintAsHexArray(Span<const uint8_t> bytes, StreamWriter *out_st)
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

bool PackToC(Span<const PackAssetInfo> assets, const char *output_path,
             CompressionType compression_type)
{
    BlockAllocator temp_alloc;

    StreamWriter st;
    if (output_path) {
        st.Open(output_path);
    } else {
        st.Open(stdout, "<stdout>");
    }
    if (st.error)
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
            blob.str_name = asset.name;
            blob.var_name = CreateVariableName(asset.name, &temp_alloc);

            PrintLn(&st, "    // %1", blob.str_name);
            Print(&st, "    ");
            blob.len = PackAsset(asset, compression_type,
                                 [&](Span<const uint8_t> buf) { PrintAsHexArray(buf, &st); });
            if (blob.len < 0)
                return false;
            PrintLn(&st);

            if (asset.source_map_name) {
                blob.source_map = asset.source_map_name;

                BlobInfo blob_map = {};
                blob_map.str_name = blob.source_map;
                blob_map.var_name = CreateVariableName(blob.source_map, &temp_alloc);

                PrintLn(&st, "    // %1", blob_map.str_name);
                Print(&st, "    ");
                blob_map.len = PackSourceMap(asset.sources, asset.source_map_type, compression_type,
                                             [&](Span<const uint8_t> buf) { PrintAsHexArray(buf, &st); });
                if (blob_map.len < 0)
                    return false;
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
        for (Size i = 0, cumulative_len = 0; i < blobs.len; i++) {
            const BlobInfo &blob = blobs[i];

            if (blob.source_map) {
                PrintLn(&st, "    {\"%1\", %2, {raw_data + %3, %4}, \"%5\"},",
                        blob.str_name, (int)compression_type, cumulative_len, blob.len,
                        blob.source_map);
            } else {
                PrintLn(&st, "    {\"%1\", %2, {raw_data + %3, %4}, 0},",
                        blob.str_name, (int)compression_type, cumulative_len, blob.len);
            }
            cumulative_len += blob.len;
        }

        PrintLn(&st, R"(};

EXPORT extern const Span pack_assets;
const Span pack_assets = {assets, %1};
)", blobs.len);

        for (Size i = 0; i < blobs.len; i++) {
            const BlobInfo &blob = blobs[i];

            PrintLn(&st,
R"(EXPORT extern const AssetInfo *const pack_asset_%1;
const AssetInfo *const pack_asset_%1 = &assets[%2];)", blob.var_name, i);
        }
    } else {
        PrintLn(&st, R"(
EXPORT extern const Span pack_assets;
const Span pack_assets = {0, 0};)");
    }

    return st.Close();
}

bool PackToFiles(Span<const PackAssetInfo> assets, const char *output_path,
                   CompressionType compression_type)
{
    BlockAllocator temp_alloc;

    if (!output_path) {
        LogError("Output directory was not specified");
        return false;
    }
    if (!MakeDirectory(output_path, false))
        return false;

    const char *compression_ext = nullptr;
    switch (compression_type) {
        case CompressionType::None: { compression_ext = ""; } break;
        case CompressionType::Gzip: { compression_ext = ".gz"; } break;
        case CompressionType::Zlib: {
            LogError("This generator cannot use Zlib compression");
            return false;
        } break;
    }
    RG_ASSERT_DEBUG(compression_ext);

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

        const char *filename = Fmt(&temp_alloc, "%1%/%2%3", output_path, asset.name, compression_ext).ptr;

        if (!EnsureDirectoryExists(filename))
            return false;

        if (!st.Open(filename))
            return false;
        if (PackAsset(asset, compression_type,
                      [&](Span<const uint8_t> buf) { st.Write(buf); }) < 0)
            return false;
        if (!st.Close())
            return false;

        if (asset.source_map_name) {
            const char *map_filename = Fmt(&temp_alloc, "%1%/%2%3", output_path,
                                           asset.source_map_name, compression_ext).ptr;

            if (!st.Open(map_filename))
                return false;
            if (PackSourceMap(asset.sources, asset.source_map_type, compression_type,
                              [&](Span<const uint8_t> buf) { st.Write(buf); }) < 0)
                return false;
            if (!st.Close())
                return false;
        }
    }

    return true;
}

bool PackAssets(Span<const PackAssetInfo> assets, const char *output_path,
                PackMode mode, CompressionType compression_type)
{
    switch (mode) {
        case PackMode::C: return PackToC(assets, output_path, compression_type);
        case PackMode::Files: return PackToFiles(assets, output_path, compression_type);
    }
    RG_ASSERT(false);
}

}
