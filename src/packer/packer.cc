// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    // Avoid windows.h
    extern "C" __declspec(dllimport) int __stdcall PathMatchSpecA(const char *pszFile, const char *pszSpec);
#else
    #include <fnmatch.h>
#endif

#include "../libcc/libcc.hh"
#include "generator.hh"
#include "packer.hh"

struct MergeRule {
    const char *name;
    MergeMode merge_mode;
    SourceMapType source_map_type;
    HeapArray<const char *> include;
    HeapArray<const char *> exclude;
};

enum class GeneratorType {
    CXX,
    Files
};
static const char *const GeneratorTypeNames[] = {
    "C++",
    "Files"
};

static MergeMode FindDefaultMergeMode(const char *filename)
{
    Span<const char> extension = GetPathExtension(filename);

    if (extension == ".css") {
        return MergeMode::CSS;
    } else if (extension == ".js") {
        return MergeMode::JS;
    } else {
        return MergeMode::Naive;
    }
}

static bool LoadMergeRules(const char *filename, HeapArray<MergeRule> *out_rules, Allocator *alloc)
{
    DEFER_NC(out_guard, len = out_rules->len) { out_rules->RemoveFrom(len); };

    StreamReader st(filename);
    if (st.error)
        return false;

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    DEFER { PopLogHandler(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }

            MergeRule *rule = out_rules->AppendDefault();
            rule->name = DuplicateString(prop.section, alloc).ptr;
            rule->merge_mode = FindDefaultMergeMode(rule->name);

            bool changed_merge_mode = false;
            do {
                if (prop.key == "MergeMode") {
                    if (prop.value == "Naive") {
                        rule->merge_mode = MergeMode::Naive;
                    } else if (prop.value == "CSS") {
                        rule->merge_mode = MergeMode::CSS;
                    } else if (prop.value == "JS") {
                        rule->merge_mode = MergeMode::JS;
                    } else {
                        LogError("Invalid MergeMode value '%1'", prop.value);
                        valid = false;
                    }

                    changed_merge_mode = true;
                } else if (prop.key == "SourceMap") {
                    if (prop.value == "None") {
                        rule->source_map_type = SourceMapType::None;
                    } else if (prop.value == "JSv3") {
                        rule->source_map_type = SourceMapType::JSv3;
                    } else {
                        LogError("Invalid SourceMap value '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "Include") {
                    rule->include.Append(DuplicateString(prop.value, alloc).ptr);
                } else if (prop.key == "Exclude") {
                    rule->exclude.Append(DuplicateString(prop.value, alloc).ptr);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (rule->merge_mode == MergeMode::Naive && !changed_merge_mode) {
                LogError("Using naive merge method for '%1'", filename);
            }
        }
    }
    if (ini.error || !valid)
        return false;

    out_guard.disable();
    return true;
}

static const MergeRule *FindMergeRule(Span<const MergeRule> rules, const char *filename)
{
#ifdef _WIN32
    const auto test_pattern = [&](const char *pattern) { return PathMatchSpecA(filename, pattern); };
#else
    const auto test_pattern = [&](const char *pattern) { return !fnmatch(pattern, filename, 0); };
#endif

    for (const MergeRule &rule: rules) {
        if (std::any_of(rule.include.begin(), rule.include.end(), test_pattern) &&
                !std::any_of(rule.exclude.begin(), rule.exclude.end(), test_pattern))
            return &rule;
    }

    return nullptr;
}

static void InitSourceMergeData(SourceInfo *src, MergeMode merge_mode, Allocator *alloc)
{
    switch (merge_mode) {
        case MergeMode::Naive: {
            src->prefix = "";
            src->suffix = "";
        } break;
        case MergeMode::CSS: {
            src->prefix = Fmt(alloc, "/* %1\n   ------------------------------------ */\n\n", src->filename).ptr;
            src->suffix = "\n";
        } break;
        case MergeMode::JS: {
            src->prefix = Fmt(alloc, "// %1\n// ------------------------------------\n\n", src->filename).ptr;
            src->suffix = "\n";
        } break;
    }
}

static const char *StripDirectoryComponents(Span<const char> filename, int strip_count)
{
    const char *name = filename.ptr;
    for (int i = 0; filename.len && i <= strip_count; i++) {
        name = SplitStrAny(filename, PATH_SEPARATORS, &filename).ptr;
    }

    return name;
}

int main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: packer <filename> ...

Options:
    -g, --generator <gen>        Set output file generator
                                 (default: C++)
    -O, --output_file <file>     Redirect output to file or directory

    -s, --strip <count>          Strip first count directory components, or 'All'
                                 (default: All)
    -c, --compress <type>        Compress data, see below for available types
                                 (default: %1)

    -M, --merge_file <file>      Load merge rules from file
        --source_map             Generate source maps when applicable

Available generators:)", CompressionTypeNames[0]);
        for (const char *gen: GeneratorTypeNames) {
            PrintLn(fp, "    %1", gen);
        }
        PrintLn(fp, R"(
Available compression types:)");
        for (const char *type: CompressionTypeNames) {
            PrintLn(fp, "    %1", type);
        }
    };

    GeneratorType generator = GeneratorType::CXX;
    const char *output_path = nullptr;
    int strip_count = INT_MAX;
    CompressionType compression_type = CompressionType::None;
    const char *merge_file = nullptr;
    bool source_maps = false;
    HeapArray<const char *> filenames;
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-g", "--generator", OptionType::Value)) {
                const char *const *name = FindIf(GeneratorTypeNames,
                                                 [&](const char *name) { return TestStr(name, opt.current_value); });
                if (!name) {
                    LogError("Unknown generator type '%1'", opt.current_value);
                    return 1;
                }

                generator = (GeneratorType)(name - GeneratorTypeNames);
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                output_path = opt.current_value;
            } else if (opt.Test("-s", "--strip", OptionType::Value)) {
                if (TestStr(opt.current_value, "All")) {
                    strip_count = INT_MAX;
                } else if (!ParseDec(opt.current_value, &strip_count)) {
                    return 1;
                }
            } else if (opt.Test("-c", "--compress", OptionType::Value)) {
                const char *const *name = FindIf(CompressionTypeNames,
                                                 [&](const char *name) { return TestStr(name, opt.current_value); });
                if (!name) {
                    LogError("Unknown compression type '%1'", opt.current_value);
                    return 1;
                }

                compression_type = (CompressionType)(name - CompressionTypeNames);
            } else if (opt.Test("-M", "--merge_file", OptionType::Value)) {
                merge_file = opt.current_value;
            } else if (opt.Test("--source_map")) {
                source_maps = true;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        opt.ConsumeNonOptions(&filenames);
        if (!filenames.len) {
            LogError("No filename specified");
            return 1;
        }
    }

    HeapArray<MergeRule> merge_rules;
    if (merge_file && !LoadMergeRules(merge_file, &merge_rules, &temp_alloc))
        return 1;
    if (!source_maps) {
        for (MergeRule &rule: merge_rules) {
            rule.source_map_type = SourceMapType::None;
        }
    }

    // Map source files to assets
    HeapArray<AssetInfo> assets;
    {
        HashMap<const void *, Size> merge_map;
        for (const char *filename: filenames) {
            const char *basename = SplitStrReverseAny(filename, PATH_SEPARATORS).ptr;
            const MergeRule *rule = FindMergeRule(merge_rules, basename);

            SourceInfo src = {};
            src.filename = filename;
            src.name = StripDirectoryComponents(filename, strip_count);

            if (rule) {
                InitSourceMergeData(&src, rule->merge_mode, &temp_alloc);

                Size asset_idx = merge_map.FindValue(rule, -1);
                if (asset_idx >= 0) {
                    assets[asset_idx].sources.Append(src);
                } else {
                    merge_map.Append(rule, assets.len);

                    AssetInfo asset = {};
                    asset.name = rule->name;
                    asset.source_map_type = rule->source_map_type;
                    if (rule->source_map_type != SourceMapType::None) {
                        asset.source_map_name = Fmt(&temp_alloc, "%1.map", rule->name).ptr;
                    }
                    assets.Append(asset)->sources.Append(src);
                }
            }

            if (!rule || rule->source_map_type != SourceMapType::None) {
                InitSourceMergeData(&src, MergeMode::Naive, &temp_alloc);

                AssetInfo asset = {};
                asset.name = src.name;
                assets.Append(asset)->sources.Append(src);
            }
        }
    }

    switch (generator) {
        case GeneratorType::CXX: return !GenerateCXX(assets, output_path, compression_type);
        case GeneratorType::Files: return !GenerateFiles(assets, output_path, compression_type);
    }
    Assert(false);
}
