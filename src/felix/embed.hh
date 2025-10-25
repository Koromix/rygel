// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

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
