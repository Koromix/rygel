// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "merge.hh"
#include "pack.hh"

namespace RG {

int RunPack(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    PackMode mode = PackMode::C;
    const char *output_path = nullptr;
    int strip_count = INT_MAX;
    CompressionType compression_type = CompressionType::None;
    unsigned int merge_flags = 0;
    const char *merge_file = nullptr;
    HeapArray<const char *> filenames;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: felix pack <filename> ...

Options:
    -t, --type <type>            Set output file type
                                 (default: C)
    -O, --output_file <file>     Redirect output to file or directory

    -s, --strip <count>          Strip first count directory components, or 'All'
                                 (default: All)
    -c, --compress <type>        Compress data, see below for available types
                                 (default: %2)

    -M, --merge_file <file>      Load merge rules from file
    -m, --merge_option <options> Merge options (see below)

Available output types:)", PackModeNames[(int)mode],
                           CompressionTypeNames[(int)compression_type]);
        for (const char *gen: PackModeNames) {
            PrintLn(fp, "    %1", gen);
        }
        PrintLn(fp, R"(
Available compression types:)");
        for (const char *type: CompressionTypeNames) {
            PrintLn(fp, "    %1", type);
        }
        PrintLn(fp, R"(
Available merge options:)");
        for (const char *option: MergeFlagNames) {
            PrintLn(fp, "    %1", option);
        }
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-t", "--type", OptionType::Value)) {
                const char *const *name = FindIf(PackModeNames,
                                                 [&](const char *name) { return TestStr(name, opt.current_value); });
                if (!name) {
                    LogError("Unknown generator type '%1'", opt.current_value);
                    return 1;
                }

                mode = (PackMode)(name - PackModeNames);
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
            } else if (opt.Test("-m", "--merge_option", OptionType::Value)) {
                const char *flags_str = opt.current_value;

                while (flags_str[0]) {
                    Span<const char> flag = TrimStr(SplitStr(flags_str, ',', &flags_str), " ");
                    const char *const *name = FindIf(MergeFlagNames,
                                                     [&](const char *name) { return TestStr(name, flag); });
                    if (!name) {
                        LogError("Unknown merge flag '%1'", flag);
                        return 1;
                    }
                    merge_flags |= 1u << (name - MergeFlagNames);
                }
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        const char *filename;
        while ((filename = opt.ConsumeNonOption())) {
            char *filename2 = NormalizePath(filename, &temp_alloc).ptr;
#ifdef _WIN32
            for (Size i = 0; filename2[i]; i++) {
                filename2[i] = (filename2[i] == '\\' ? '/' : filename2[i]);
            }
#endif

            filenames.Append(filename2);
        }
    }

    // Load merge rules
    MergeRuleSet merge_rule_set;
    if (merge_file && !LoadMergeRules(merge_file, merge_flags, &merge_rule_set))
        return 1;

    // Resolve merge rules
    PackAssetSet asset_set;
    ResolveAssets(filenames, strip_count, merge_rule_set.rules, &asset_set);

    // Generate output
    return !PackAssets(asset_set.assets, output_path, mode, compression_type);
}

}
