// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "packer.hh"

Size PackAsset(Span<const SourceInfo> sources, CompressionType compression_type,
               std::function<void(Span<const uint8_t> buf)> func);
Size PackSourceMap(Span<const SourceInfo> sources, SourceMapType source_map_type,
                   CompressionType compression_type, std::function<void(Span<const uint8_t>)> func);
