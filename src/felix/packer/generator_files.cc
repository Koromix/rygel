// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "generator.hh"
#include "output.hh"
#include "packer.hh"

bool GenerateFiles(Span<const AssetInfo> assets, const char *output_path,
                   CompressionType compression_type)
{
    BlockAllocator temp_alloc;

    if (!output_path) {
        LogError("Output directory was not specified");
        return false;
    }
    if (!TestFile(output_path, FileType::Directory)) {
        LogError("Directory '%1' does not exist", output_path);
        return false;
    }

    const char *compression_ext = nullptr;
    switch (compression_type) {
        case CompressionType::None: { compression_ext = ""; } break;
        case CompressionType::Gzip: { compression_ext = ".gz"; } break;
        case CompressionType::Zlib: {
            LogError("This generator cannot use Zlib compression");
            return false;
        } break;
    }
    DebugAssert(compression_ext);

    for (const AssetInfo &asset: assets) {
        StreamWriter st;

        if (UNLIKELY(PathIsAbsolute(asset.name))) {
            LogError("Asset name '%1' cannot be an absolute path", asset.name);
            return false;
        }
        if (UNLIKELY(PathContainsDotDot(asset.name))) {
            LogError("Asset name '%1' must not contain '..'", asset.name);
            return false;
        }

        Span<const char> directory;
        const char *filename = Fmt(&temp_alloc, "%1%/%2%3", output_path, asset.name, compression_ext).ptr;
        SplitStrReverseAny(filename, PATH_SEPARATORS, &directory);

        if (!MakeDirectoryRec(directory))
            return false;

        if (!st.Open(filename))
            return false;
        if (PackAsset(asset.sources, compression_type,
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
