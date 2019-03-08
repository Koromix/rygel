// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "generator.hh"
#include "output.hh"
#include "packer.hh"

// For simplicity, I've replicated the required data structures from libcc
// and packer.hh directly below. Don't forget to keep them in sync.
static const char *const OutputPrefix =
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

typedef struct pack_Asset {
    const char *name;
    int compression_type; // CompressionType
    Span data;

    const char *source_map;
} pack_Asset;)";

struct BlobInfo {
    const char *str_name;
    const char *var_name;
    Size len;

    const char *source_map;
};

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

bool GenerateC(Span<const AssetInfo> assets, const char *output_path,
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
        return 1;

    PrintLn(&st, OutputPrefix);

    // Work around the ridiculousness of C++ not liking empty arrays
    if (assets.len) {
        PrintLn(&st, R"(
static const uint8_t raw_data[] = {)");

        // Pack assets and source maps
        HeapArray<BlobInfo> blobs;
        for (const AssetInfo &asset: assets) {
            BlobInfo blob = {};
            blob.str_name = asset.name;
            blob.var_name = CreateVariableName(asset.name, &temp_alloc);

            PrintLn(&st, "    // %1", blob.str_name);
            Print(&st, "    ");
            blob.len = PackAsset(asset.sources, compression_type,
                                 [&](Span<const uint8_t> buf) { PrintAsHexArray(buf, &st); });
            if (blob.len < 0)
                return 1;
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
                    return 1;
                PrintLn(&st);

                blobs.Append(blob);
                blobs.Append(blob_map);
            } else {
                blobs.Append(blob);
            }
        }

        PrintLn(&st, R"(};

static pack_Asset assets[%1] = {)", blobs.len);

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
R"(EXPORT extern const pack_Asset *const pack_asset_%1;
const pack_Asset *const pack_asset_%1 = &assets[%2];)", blob.var_name, i);
        }
    } else {
        PrintLn(&st, R"(
EXPORT extern const Span pack_assets;
const Span pack_assets = {};)");
    }

    return st.Close();
}
