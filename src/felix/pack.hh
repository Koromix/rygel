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

#pragma once

#include "../core/libcc/libcc.hh"
#include "merge.hh"

namespace RG {

enum class PackMode {
    C,
    Carray,
    Files
};
static const char *const PackModeNames[] = {
    "C",
    "Carray",
    "Files"
};

bool PackToC(Span<const PackAssetInfo> assets, bool use_arrays, const char *output_path);
bool PackToFiles(Span<const PackAssetInfo> assets, const char *output_path);

bool PackAssets(Span<const PackAssetInfo> assets, const char *output_path, PackMode mode);

}
