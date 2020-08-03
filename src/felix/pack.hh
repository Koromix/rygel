// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"
#include "merge.hh"

namespace RG {

enum class PackMode {
    Cstring,
    Carray,
    Files
};
static const char *const PackModeNames[] = {
    "Cstring",
    "Carray",
    "Files"
};

bool PackToC(Span<const PackAssetInfo> assets, bool use_arrays, const char *output_path);
bool PackToFiles(Span<const PackAssetInfo> assets, const char *output_path);

bool PackAssets(Span<const PackAssetInfo> assets, const char *output_path, PackMode mode);

}
