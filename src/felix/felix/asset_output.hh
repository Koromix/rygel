// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "asset_generator.hh"

namespace RG {

Size PackAsset(Span<const PackSourceInfo> sources, CompressionType compression_type,
               std::function<void(Span<const uint8_t> buf)> func);
Size PackSourceMap(Span<const PackSourceInfo> sources, SourceMapType source_map_type,
                   CompressionType compression_type, std::function<void(Span<const uint8_t>)> func);

}
