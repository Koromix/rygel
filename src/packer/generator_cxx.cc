// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "generator.hh"
#include "output.hh"
#include "packer.hh"

// For simplicity, I've replicated the required data structures from libcc
// and packer.hh directly below. Don't forget to keep them in sync.
static const char *const OutputPrefix =
R"(// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <initializer_list>
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

template <typename T>
struct Span {
    T *ptr;
    Size len;

    Span() = default;
    constexpr Span(T *ptr_, Size len_) : ptr(ptr_), len(len_) {}
    template <Size N>
    constexpr Span(T (&arr)[N]) : ptr(arr), len(N) {}
};

enum class CompressionType {
    None,
    Zlib,
    Gzip
};

struct pack_Asset {
    const char *name;
    CompressionType compression_type;
    Span<const uint8_t> data;

    const char *source_map;
};)";

struct BlobInfo {
    const char *name;
    Size len;

    const char *source_map;
};

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

bool GenerateCXX(Span<const AssetInfo> assets, const char *output_path,
                 CompressionType compression_type)
{
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
            blob.name = asset.name;

            PrintLn(&st, "    // %1", blob.name);
            Print(&st, "    ");
            blob.len = PackAsset(asset.sources, compression_type,
                                 [&](Span<const uint8_t> buf) { PrintAsHexArray(buf, &st); });
            if (blob.len < 0)
                return 1;
            PrintLn(&st);

            if (asset.source_map_name) {
                blob.source_map = asset.source_map_name;

                BlobInfo blob_map = {};
                blob_map.name = blob.source_map;

                PrintLn(&st, "    // %1", blob_map.name);
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
                PrintLn(&st, "    {\"%1\", (CompressionType)%2, {raw_data + %3, %4}, \"%5\"},",
                        blob.name, (int)compression_type, cumulative_len, blob.len,
                        blob.source_map);
            } else {
                PrintLn(&st, "    {\"%1\", (CompressionType)%2, {raw_data + %3, %4}},",
                        blob.name, (int)compression_type, cumulative_len, blob.len);
            }
            cumulative_len += blob.len;
        }

        PrintLn(&st, R"(};

EXPORT extern const Span<const pack_Asset> packer_assets;
const Span<const pack_Asset> packer_assets = assets;)");
    } else {
        PrintLn(&st, R"(
EXPORT extern const Span<const pack_Asset> packer_assets;
const Span<const pack_Asset> packer_assets = {};)");
    }

    return st.Close();
}
