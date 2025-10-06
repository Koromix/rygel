// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"

namespace K {

struct EmbedAsset {
    const char *name;
    CompressionType compression_type;

    const char *src_filename;
};

struct EmbedAssetSet {
    HeapArray<EmbedAsset> assets;
    BlockAllocator str_alloc;
};

enum class EmbedFlag {
    UseEmbed = 1 << 0,
    UseLiterals = 1 << 1,
    NoSymbols = 1 << 2,
    NoArray = 1 << 3,
    MaxCompression = 1 << 4
};
static const char *const EmbedFlagNames[] = {
    "UseEmbed",
    "UseLiterals",
    "NoSymbols",
    "NoArray",
    "MaxCompression"
};

bool ResolveAssets(Span<const char *const> filenames, int strip_count, CompressionType compression_type, EmbedAssetSet *out_set);
bool PackAssets(Span<const EmbedAsset> assets, unsigned int flags, const char *output_path);

}
