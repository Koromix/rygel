// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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

#include "src/core/base/base.hh"
#include "embed.hh"

namespace RG {

int RunEmbed(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    unsigned int flags = 0;
    const char *output_path = nullptr;
    int strip_count = 0;
    CompressionType compression_type = CompressionType::None;
    HeapArray<const char *> filenames;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 embed [option...] [filename...]%!0

Options:

    %!..+-O, --output_file filename%!0     Redirect output to file or directory

    %!..+-f, --flags flags%!0              Set embedding flags
    %!..+-s, --strip count%!0              Strip first count directory components, or 'All'
                                   %!D..(default: 0)%!0

    %!..+-c, --compress type%!0            Compress data, see below for available types
                                   %!D..(default: %2)%!0

Available embedding flags: %!..+%3%!0
Available compression types: %!..+%4%!0)", FelixTarget, CompressionTypeNames[(int)compression_type],
                                       FmtSpan(EmbedFlagNames), FmtSpan(CompressionTypeNames));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-f", "--flags", OptionType::Value)) {
                const char *flags_str = opt.current_value;

                while (flags_str[0]) {
                    Span<const char> part = TrimStr(SplitStrAny(flags_str, " ,", &flags_str), " ");

                    if (part.len && !OptionToFlagI(EmbedFlagNames, part, &flags)) {
                        LogError("Unknown embedding flag '%1'", part);
                        return 1;
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
                if (!OptionToEnumI(CompressionTypeNames, opt.current_value, &compression_type)) {
                    LogError("Unknown compression type '%1'", opt.current_value);
                    return 1;
                }
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        const char *filename;
        while ((filename = opt.ConsumeNonOption())) {
            char *filename2 = NormalizePath(filename, &temp_alloc).ptr;
#if defined(_WIN32)
            for (Size i = 0; filename2[i]; i++) {
                filename2[i] = (filename2[i] == '\\' ? '/' : filename2[i]);
            }
#endif

            filenames.Append(filename2);
        }
    }

    // Resolve list of assets
    EmbedAssetSet asset_set;
    if (!ResolveAssets(filenames, strip_count, compression_type, &asset_set))
        return 1;

    // Generate output
    if (!PackAssets(asset_set.assets, flags, output_path))
        return 1;

    return 0;
}

}
