// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "merge.hh"

namespace RG {

enum class PackMode {
    C,
    Files
};
static const char *const PackModeNames[] = {
    "C",
    "Files"
};

bool PackToC(Span<const PackAssetInfo> assets, const char *output_path,
             CompressionType compression_type);
bool PackToFiles(Span<const PackAssetInfo> assets, const char *output_path,
                 CompressionType compression_type);

bool PackAssets(Span<const PackAssetInfo> assets, const char *output_path,
                PackMode mode, CompressionType compression_type);

}
