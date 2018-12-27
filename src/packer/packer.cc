// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"

#ifdef _WIN32
    // Avoid windows.h
    extern "C" __declspec(dllimport) int __stdcall PathMatchSpecA(const char *pszFile, const char *pszSpec);
#else
    #include <fnmatch.h>
#endif

enum class MergeMode {
    Naive,
    CSS,
    JS,
};

struct MergeRule {
    const char *filename;
    MergeMode merge_mode;
    HeapArray<const char *> include;
    HeapArray<const char *> exclude;
};

struct AssetInfo {
    const char *filename;
    MergeMode merge_mode;
    HeapArray<const char *> sources;
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
};)";

static MergeMode FindDefaultMergeMode(const char *filename)
{
    Span<const char> extension = GetPathExtension(filename);

    if (extension == ".css") {
        return MergeMode::CSS;
    } else if (extension == ".js") {
        return MergeMode::JS;
    } else {
        LogError("Using naive merge method for '%1'", filename);
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
            rule->filename = DuplicateString(prop.section, alloc).ptr;

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
                } else if (prop.key == "Include") {
                    rule->include.Append(DuplicateString(prop.value, alloc).ptr);
                } else if (prop.key == "Exclude") {
                    rule->exclude.Append(DuplicateString(prop.value, alloc).ptr);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (!changed_merge_mode) {
                rule->merge_mode = FindDefaultMergeMode(rule->filename);
            }
        }
    }
    if (ini.error || !valid)
        return false;

    out_guard.disable();
    return true;
}

static void PrintAsHexArray(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    Size i = 0;
    for (Size end = bytes.len / 8 * 8; i < end; i += 8) {
        Print(out_st, "0x%1, 0x%2, 0x%3, 0x%4, 0x%5, 0x%6, 0x%7, 0x%8,",
              FmtHex(bytes[i + 0]).Pad0(-2), FmtHex(bytes[i + 1]).Pad0(-2),
              FmtHex(bytes[i + 2]).Pad0(-2), FmtHex(bytes[i + 3]).Pad0(-2),
              FmtHex(bytes[i + 4]).Pad0(-2), FmtHex(bytes[i + 5]).Pad0(-2),
              FmtHex(bytes[i + 6]).Pad0(-2), FmtHex(bytes[i + 7]).Pad0(-2));
    }
    for (; i < bytes.len; i++) {
        Print(out_st, "0x%1, ", FmtHex(bytes[i]).Pad0(-2));
    }
}

static Size PackAsset(const AssetInfo &asset, CompressionType compression_type, StreamWriter *out_st)
{
    Size written_len = 0;
    {
        HeapArray<uint8_t> buf;
        StreamWriter writer(&buf, nullptr, compression_type);

        const auto flush_buffer = [&]() {
            written_len += buf.len;
            PrintAsHexArray(buf, out_st);
            buf.RemoveFrom(0);
        };

        for (const char *src: asset.sources) {
            switch (asset.merge_mode) {
                case MergeMode::Naive: {} break;
                case MergeMode::CSS: {
                    PrintLn(&writer, "/* %1\n   ------------------------------------ */\n", src);
                } break;
                case MergeMode::JS: {
                    PrintLn(&writer, "// %1\n// ------------------------------------\n", src);
                } break;
            }

            StreamReader reader(src);
            while (!reader.eof) {
                uint8_t read_buf[128 * 1024];
                Size read_len = reader.Read(SIZE(read_buf), read_buf);
                if (read_len < 0)
                    return false;

                writer.Write(read_buf, read_len);
                flush_buffer();
            }

            switch (asset.merge_mode) {
                case MergeMode::Naive: {} break;
                case MergeMode::CSS:
                case MergeMode::JS: { writer.Write('\n'); } break;
            }
        }

        writer.Close();
        flush_buffer();
    }

    return written_len;
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

    HeapArray<AssetInfo> assets;
    {
        HashMap<const void *, Size> merge_map;
        for (const char *filename: filenames) {
            const char *basename = SplitStrReverseAny(filename, PATH_SEPARATORS).ptr;
            const MergeRule *rule = FindMergeRule(merge_rules, basename);

            Size asset_idx = merge_map.FindValue(rule, -1);
            if (asset_idx >= 0) {
                // Nothing to do
            } else if (rule) {
                asset_idx = assets.len;

                AssetInfo asset = {};
                asset.filename = rule->filename;
                asset.merge_mode = rule->merge_mode;
                assets.Append(asset);

                merge_map.Append(rule, asset_idx);
            } else {
                asset_idx = assets.len;

                AssetInfo asset = {};
                asset.filename = filename;
                asset.merge_mode = MergeMode::Naive;
                assets.Append(asset);
            }

            assets[asset_idx].sources.Append(filename);
        }
    }

    HeapArray<Size> lengths;
    for (const AssetInfo &asset: assets) {
        PrintLn(&st, "    // %1", asset.filename);
        Print(&st, "    ");

        Size asset_len = PackAsset(asset, compression_type, &st);
        if (asset_len < 0)
            return 1;
        lengths.Append(asset_len);

        PrintLn(&st);
    }

    PrintLn(&st,
R"(};

static PackerAsset assets[] = {)");

    for (Size i = 0, cumulative_len = 0; i < assets.len; i++) {
        Span<const char> filename = assets[i].filename;

        Span<const char> name = filename;
        {
            Span<const char> remainder = filename;
            for (Size j = 0; remainder.len && j < depth; j++) {
                name = SplitStrReverseAny(remainder, PATH_SEPARATORS, &remainder);
            }
            name.len = filename.end() - name.ptr;
        }

        PrintLn(&st, "    {\"%1\", (CompressionType)%2, {raw_data + %3, %4}},",
                name, (int)compression_type, cumulative_len, lengths[i]);
        cumulative_len += lengths[i];
    }

    PrintLn(&st,
R"(};

%1extern const Span<const PackerAsset> %2;
const Span<const PackerAsset> %2 = assets;)", export_span ? "EXPORT " : "", span_name);

    return 0;
}
