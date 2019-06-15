// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

namespace RG {

enum class GeneratorType {
    C,
    Files
};
static const char *const GeneratorTypeNames[] = {
    "C",
    "Files"
};

struct PackSourceInfo {
    const char *filename;
    const char *name;

    const char *prefix;
    const char *suffix;
};

enum class SourceMapType {
    None,
    JSv3
};

struct PackAssetInfo {
    const char *name;
    HeapArray<PackSourceInfo> sources;

    SourceMapType source_map_type;
    const char *source_map_name;
};

bool GenerateC(Span<const PackAssetInfo> assets, const char *output_path,
               CompressionType compression_type);
bool GenerateFiles(Span<const PackAssetInfo> assets, const char *output_path,
                   CompressionType compression_type);

}
