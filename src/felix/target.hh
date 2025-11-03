// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "compiler.hh"

namespace K {

struct TargetConfig;
struct SourceFileInfo;
struct SourceFeatures;

struct alignas(8) TargetInfo {
    const char *name;
    TargetType type;
    unsigned int platforms;
    bool enable_by_default;

    const char *title;
    const char *version_tag;
    const char *icon_filename;

    HeapArray<const TargetInfo *> imports;

    HeapArray<const char *> definitions;
    HeapArray<const char *> export_definitions;
    HeapArray<const char *> include_directories;
    HeapArray<const char *> export_directories;
    HeapArray<const char *> include_files;
    HeapArray<const char *> libraries;

    HeapArray<const char *> qt_components;
    int64_t qt_version;

    uint32_t enable_features;
    uint32_t disable_features;

    const SourceFileInfo *c_pch_src;
    const SourceFileInfo *cxx_pch_src;
    HeapArray<const char *> pchs;

    HeapArray<const SourceFileInfo *> sources;
    HeapArray<const char *> translations;

    const char *bundle_options;

    HeapArray<const char *> embed_filenames;
    const char *embed_options;

    int link_priority;

    uint32_t CombineFeatures(uint32_t features) const
    {
        features |= enable_features;
        features &= ~disable_features;

        return features;
    }

    bool TestPlatforms(HostPlatform platform) const { return platforms & (1 << (int)platform); }

    K_HASHTABLE_HANDLER(TargetInfo, name);
};

struct SourceFileInfo {
    // In order to build source files with the correct definitions (and include directories, etc.),
    // we need to use the options from the target that first found this source file!
    const TargetInfo *target;

    const char *filename;
    SourceType type;

    uint32_t enable_features;
    uint32_t disable_features;

    uint32_t CombineFeatures(uint32_t features) const
    {
        features = target->CombineFeatures(features);

        features |= enable_features;
        features &= ~disable_features;

        return features;
    }

    K_HASHTABLE_HANDLER(SourceFileInfo, filename);
};

struct TargetSet {
    const char *root_directory = nullptr;

    BucketArray<TargetInfo> targets;
    HashTable<const char *, TargetInfo *> targets_map;

    BucketArray<SourceFileInfo> sources;
    HashTable<const char *, SourceFileInfo *> sources_map;

    BlockAllocator str_alloc;
};

class TargetSetBuilder {
    K_DELETE_COPY(TargetSetBuilder)

    const Compiler *compiler;
    uint32_t features;

    BlockAllocator temp_alloc;

    HashSet<const char *> known_targets;
    TargetSet set;

public:
    TargetSetBuilder(const Compiler *compiler, uint32_t features)
        : compiler(compiler), features(features) {}

    bool LoadIni(StreamReader *st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(TargetSet *out_set);

private:
    const TargetInfo *CreateTarget(const char *root_directory, TargetConfig *target_config);
    const SourceFileInfo *CreateSource(const TargetInfo *target, const char *filename,
                                       SourceType type, const SourceFeatures *features);

    bool MatchPropertySuffix(Span<const char> str, bool *out_match);
};

unsigned int ParseSupportedPlatforms(Span<const char> str);
bool ParseArchitecture(Span<const char> str, HostArchitecture *out_architecture);

bool LoadTargetSet(Span<const char *const> filenames, const Compiler *compiler, uint32_t features, TargetSet *out_set);

}
