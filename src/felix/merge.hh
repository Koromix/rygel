// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

namespace RG {

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

    const char *transform_cmd;
    SourceMapType source_map_type;
    const char *source_map_name;
};

struct PackAssetSet {
    HeapArray<PackAssetInfo> assets;
    BlockAllocator str_alloc;
};

bool LoadMergeRules(const char *filename, MergeRuleSet *out_set);

void ResolveAssets(Span<const char *const> filenames, int strip_count,
                   Span<const MergeRule> rules, PackAssetSet *out_set);

}
