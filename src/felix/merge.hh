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

#include "src/core/libcc/libcc.hh"

namespace RG {

enum class MergeFlag {
    SourceMap = 1 << 0,
    RunTransform = 1 << 1
};
static const char *const MergeFlagNames[] = {
    "SourceMap",
    "RunTransform"
};

enum class MergeMode {
    Naive,
    CSS,
    JS
};

enum class SourceMapType {
    None,
    JSv3
};

struct MergeRule {
    const char *name;

    HeapArray<const char *> include;
    HeapArray<const char *> exclude;

    bool override_compression;
    CompressionType compression_type;

    MergeMode merge_mode;
    SourceMapType source_map_type;
    const char *transform_cmd;
};

struct MergeRuleSet {
    HeapArray<MergeRule> rules;
    BlockAllocator str_alloc;
};

struct PackSourceInfo {
    const char *filename;
    const char *name;

    const char *prefix;
    const char *suffix;
};

struct PackAssetInfo {
    const char *name;
    HeapArray<PackSourceInfo> sources;

    CompressionType compression_type;

    const char *transform_cmd;
    SourceMapType source_map_type;
    const char *source_map_name;
};

struct PackAssetSet {
    HeapArray<PackAssetInfo> assets;
    BlockAllocator str_alloc;
};

bool LoadMergeRules(const char *filename, unsigned int flags, MergeRuleSet *out_set);

void ResolveAssets(Span<const char *const> filenames, int strip_count, Span<const MergeRule> rules,
                   CompressionType compression_type, PackAssetSet *out_set);

}
