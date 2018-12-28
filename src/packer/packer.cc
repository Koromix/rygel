// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "packer.hh"
#include "output.hh"

#ifdef _WIN32
    // Avoid windows.h
    extern "C" __declspec(dllimport) int __stdcall PathMatchSpecA(const char *pszFile, const char *pszSpec);
#else
    #include <fnmatch.h>
#endif

struct MergeRule {
    const char *name;
    MergeMode merge_mode;
    SourceMapType source_map_type;
    HeapArray<const char *> include;
    HeapArray<const char *> exclude;
};

struct AssetInfo {
    const char *name;

    SourceMapType source_map_type;
    HeapArray<SourceInfo> sources;
};

struct PackedAssetInfo {
    const char *name;
    Size len;

    const char *source_map;
};

// For simplicity, I've replicated the required data structures from kutil.hh
// and packer.hh directly below. Don't forget to keep them in sync.
static const char *const OutputPrefix =
R"(// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <initializer_list>
#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
    typedef int64_t Size;
#elif defined(__i386__) || defined(_M_IX86) || defined(__arm__) || defined(__EMSCRIPTEN__)
    typedef int32_t Size;
#endif

#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif

template <typename T>
struct Span {
    T *ptr;
    Size len;

    Span() = default;
    constexpr Span(T *ptr_, Size len_) : ptr(ptr_), len(len_) {}
    template <Size N>
    constexpr Span(T (&arr)[N]) : ptr(arr), len(N) {}
};

enum class CompressionType {
    None,
    Zlib,
    Gzip
};

struct PackerAsset {
    const char *name;
    CompressionType compression_type;
    Span<const uint8_t> data;

    const char *source_map;
};)";

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

static const char *GetSimplifiedName(Span<const char> filename, int depth)
{
    const char *name = filename.ptr;
    for (Size j = 0; filename.len && j < depth; j++) {
        name = SplitStrReverseAny(filename, PATH_SEPARATORS, &filename).ptr;
    }

    return name;
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

int main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: packer <filename> ...

Options:
    -O, --output_file <file>     Redirect output to file

    -d, --depth <depth>          Keep <depth> last components of filename
                                 (default: keep full path)
    -c, --compress <type>        Compress data, see below for available types
                                 (default: %1)
        --span_name <name>       Change span name exported by output code
                                 (default: packer_assets)
    -e, --export                 Export span symbol

    -M, --merge_file <file>      Load merge rules from file
        --source_map             Generate source maps when applicable

Available compression types:)", CompressionTypeNames[0]);
        for (const char *type: CompressionTypeNames) {
            PrintLn(fp, "    %1", type);
        }
    };

    const char *output_path = nullptr;
    int depth = -1;
    const char *span_name = "packer_assets";
    bool export_span = false;
    CompressionType compression_type = CompressionType::None;
    const char *merge_file = nullptr;
    bool source_maps = false;
    HeapArray<const char *> filenames;
    {
        OptionParser opt_parser(argc, argv);

        while (opt_parser.Next()) {
            if (opt_parser.TestOption("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt_parser.TestOption("-d", "--depth")) {
                if (!opt_parser.RequireValue())
                    return 1;

                if (!ParseDec<int>(opt_parser.current_value, &depth))
                    return 1;
                if (depth <= 0) {
                    LogError("Option --depth requires value > 0");
                    return 1;
                }
            } else if (opt_parser.TestOption("--span_name")) {
                span_name = opt_parser.RequireValue();
                if (!span_name)
                    return 1;
            } else if (opt_parser.TestOption("--export", "-e")) {
                export_span = true;
            } else if (opt_parser.TestOption("-O", "--output_file")) {
                output_path = opt_parser.RequireValue();
                if (!output_path)
                    return 1;
            } else if (opt_parser.TestOption("-c", "--compress")) {
                if (!opt_parser.RequireValue())
                    return 1;

                Size i = 0;
                for (; i < ARRAY_SIZE(CompressionTypeNames); i++) {
                    if (TestStrI(CompressionTypeNames[i], opt_parser.current_value))
                        break;
                }
                if (i >= ARRAY_SIZE(CompressionTypeNames)) {
                    LogError("Unknown compression type '%1'", opt_parser.current_value);
                    return 1;
                }

                compression_type = (CompressionType)i;
            } else if (opt_parser.TestOption("-M", "--merge_file")) {
                merge_file = opt_parser.RequireValue();
                if (!merge_file)
                    return 1;
            } else if (opt_parser.TestOption("--source_map")) {
                source_maps = true;
            } else {
                LogError("Unknown option '%1'", opt_parser.current_option);
                return 1;
            }
        }

        opt_parser.ConsumeNonOptions(&filenames);
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

    StreamWriter st;
    if (output_path) {
        st.Open(output_path);
    } else {
        st.Open(stdout, "<stdout>");
    }
    if (st.error)
        return 1;

    PrintLn(&st,
R"(%1

static const uint8_t raw_data[] = {)", OutputPrefix);

    // Map source files to assets
    HeapArray<AssetInfo> assets;
    {
        HashMap<const void *, Size> merge_map;
        for (const char *filename: filenames) {
            const char *basename = SplitStrReverseAny(filename, PATH_SEPARATORS).ptr;
            const MergeRule *rule = FindMergeRule(merge_rules, basename);

            SourceInfo src = {};
            src.filename = filename;
            src.name = GetSimplifiedName(filename, depth);

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
                    assets.Append(asset)->sources.Append(src);
                }
            }

            if (!rule || rule->source_map_type != SourceMapType::None) {
                InitSourceMergeData(&src, MergeMode::Naive, &temp_alloc);

                AssetInfo asset = {};
                asset.name = src.name;
                asset.source_map_type = SourceMapType::None;
                assets.Append(asset)->sources.Append(src);
            }
        }
    }

    // Pack assets and source maps
    HeapArray<PackedAssetInfo> packed_assets;
    for (const AssetInfo &asset: assets) {
        PackedAssetInfo packed_asset = {};
        packed_asset.name = asset.name;

        PrintLn(&st, "    // %1", packed_asset.name);
        Print(&st, "    ");
        packed_asset.len = PackAsset(asset.sources, compression_type, &st);
        if (packed_asset.len < 0)
            return 1;
        PrintLn(&st);

        if (asset.source_map_type != SourceMapType::None) {
            packed_asset.source_map = Fmt(&temp_alloc, "%1.map", packed_asset.name).ptr;

            PackedAssetInfo packed_map = {};
            packed_map.name = packed_asset.source_map;

            PrintLn(&st, "    // %1", packed_map.name);
            Print(&st, "    ");
            packed_map.len = PackSourceMap(asset.sources, asset.source_map_type, compression_type, &st);
            if (packed_map.len < 0)
                return 1;
            PrintLn(&st);

            packed_assets.Append(packed_asset);
            packed_assets.Append(packed_map);
        } else {
            packed_assets.Append(packed_asset);
        }
    }

    PrintLn(&st,
R"(};

static PackerAsset assets[] = {)");

    // Write asset table
    for (Size i = 0, cumulative_len = 0; i < packed_assets.len; i++) {
        const PackedAssetInfo &packed_asset = packed_assets[i];

        if (packed_asset.source_map) {
            PrintLn(&st, "    {\"%1\", (CompressionType)%2, {raw_data + %3, %4}, \"%5\"},",
                    packed_asset.name, (int)compression_type, cumulative_len, packed_asset.len,
                    packed_asset.source_map);
        } else {
            PrintLn(&st, "    {\"%1\", (CompressionType)%2, {raw_data + %3, %4}},",
                    packed_asset.name, (int)compression_type, cumulative_len, packed_asset.len);
        }
        cumulative_len += packed_asset.len;
    }

    PrintLn(&st,
R"(};

%1extern const Span<const PackerAsset> %2;
const Span<const PackerAsset> %2 = assets;)", export_span ? "EXPORT " : "", span_name);

    return !st.Close();
}
