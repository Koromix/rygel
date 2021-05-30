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

#include "../core/libcc/libcc.hh"
#include "merge.hh"
#include "pack.hh"

namespace RG {

int RunPack(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    unsigned int flags = 0;
    const char *output_path = nullptr;
    int strip_count = 0;
    CompressionType compression_type = CompressionType::None;
    unsigned int merge_flags = 0;
    const char *merge_file = nullptr;
    HeapArray<const char *> filenames;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 pack <filename> ...%!0

Options:
    %!..+-f, --flags <flags>%!0          Set packing flags
    %!..+-O, --output_file <file>%!0     Redirect output to file or directory

    %!..+-s, --strip <count>%!0          Strip first count directory components, or 'All'
                                 %!D..(default: 0)%!0
    %!..+-c, --compress <type>%!0        Compress data, see below for available types
                                 %!D..(default: %2)%!0

    %!..+-M, --merge_file <file>%!0      Load merge rules from file
    %!..+-m, --merge_option <options>%!0 Merge options (see below)

Available packing flags: %!..+%3%!0
Available compression types: %!..+%4%!0
Available merge options: %!..+%5%!0)", FelixTarget, CompressionTypeNames[(int)compression_type],
                                       FmtSpan(PackFlagNames), FmtSpan(CompressionTypeNames),
                                       FmtSpan(MergeFlagNames));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-f", "--flags", OptionType::Value)) {
                const char *flags_str = opt.current_value;

                while (flags_str[0]) {
                    Span<const char> part = TrimStr(SplitStrAny(flags_str, " ,", &flags_str), " ");

                    if (part.len) {
                        PackFlag flag;
                        if (!OptionToEnum(PackFlagNames, part, &flag)) {
                            LogError("Unknown packing flag '%1'", part);
                            return 1;
                        }

                        flags |= 1u << (int)flag;
                    }
                }
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                output_path = opt.current_value;
            } else if (opt.Test("-s", "--strip", OptionType::Value)) {
                if (TestStr(opt.current_value, "All")) {
                    strip_count = INT_MAX;
                } else if (!ParseInt(opt.current_value, &strip_count)) {
                    return 1;
                }
            } else if (opt.Test("-c", "--compress", OptionType::Value)) {
                if (!OptionToEnum(CompressionTypeNames, opt.current_value, &compression_type)) {
                    LogError("Unknown compression type '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-M", "--merge_file", OptionType::Value)) {
                merge_file = opt.current_value;
            } else if (opt.Test("-m", "--merge_option", OptionType::Value)) {
                const char *flags_str = opt.current_value;

                while (flags_str[0]) {
                    Span<const char> part = TrimStr(SplitStrAny(flags_str, " ,", &flags_str), " ");

                    if (part.len) {
                        MergeFlag flag;
                        if (!OptionToEnum(MergeFlagNames, part, &flag)) {
                            LogError("Unknown merge flag '%1'", part);
                            return 1;
                        }

                        merge_flags |= 1u << (int)flag;
                    }
                }
            } else {
                if (!opt.TestHasFailed()) {
                    LogError("Unknown option '%1'", opt.current_option);
                }
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
    ResolveAssets(filenames, strip_count, merge_rule_set.rules, compression_type, &asset_set);

    // Generate output
    return !PackAssets(asset_set.assets, flags, output_path);
}

}
