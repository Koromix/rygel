// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/util.hh"

// Keep in sync with version in packer.cc
struct PackerAsset {
    const char *name;
    CompressionType compression_type;
    Span<const uint8_t> data;

    const char *source_map;
};
