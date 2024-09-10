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

namespace RG {

// Skip None
static const Span<const char *const> AvailableAlgorithms = MakeSpan(CompressionTypeNames + 1, RG_LEN(CompressionTypeNames) - 1);

static int RunCompress(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> src_filenames;
    const char *output_filename = nullptr;
    const char *output_directory = nullptr;
    CompressionType compression_type = CompressionType::None;
    CompressionSpeed compression_speed = CompressionSpeed::Default;
    bool force = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 compress <source> [-O <destination>]
       %1 compress <sources...> [-D <destination>]%!0

Options:
    %!..+-O, --output_file <file>%!0     Set output file
    %!..+-D, --output_dir <dir>%!0       Set output directory

    %!..+-a, --algorithm <algo>%!0       Set algorithm, see below
    %!..+-s, --speed <speed>%!0          Set compression speed: Default, Fast or Slow
                                 %!D..(default: Default)%!0

    %!..+-f, --force%!0                  Overwrite destination files

Available compression algorithms: %!..+%2%!0)", FelixTarget, FmtSpan(AvailableAlgorithms));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                output_filename = opt.current_value;
            } else if (opt.Test("-D", "--output_dir", OptionType::Value)) {
                output_directory = opt.current_value;
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnumI(CompressionTypeNames, opt.current_value, &compression_type) ||
                        compression_type == CompressionType::None) {
                    LogError("Unknown compression algorithm '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-s", "--speed", OptionType::Value)) {
                if (TestStrI(opt.current_value, "Default")) {
                    compression_speed = CompressionSpeed::Default;
                } else if (TestStrI(opt.current_value, "Fast")) {
                    compression_speed = CompressionSpeed::Fast;
                } else if (TestStrI(opt.current_value, "Slow")) {
                    compression_speed = CompressionSpeed::Slow;
                } else {
                    LogError("Unknown compression speed '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-f", "--force")) {
                force = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.ConsumeNonOptions(&src_filenames);
    }

    if (!src_filenames.len) {
        src_filenames.Append("-");
    }

    if (output_filename && output_directory) {
        LogError("Cannot use --output_file and --output_dir at the same time");
        return 1;
    }
    if (output_filename && src_filenames.len > 1) {
        LogError("Option --output_file can only be used with one input");
        return 1;
    }
    if (output_directory && !TestFile(output_directory, FileType::Directory)) {
        LogError("Output directory '%1' does not exist", output_directory);
        return 1;
    }

    HeapArray<const char *> dest_filenames;
    if (src_filenames.len == 1) {
        const char *src_filename = src_filenames[0];

        if (TestStr(src_filename, "-")) {
            src_filenames[0] = nullptr;
            src_filename = nullptr;
        }

        if (output_filename) {
            if (TestStr(output_filename, "-")) {
                output_filename = nullptr;
            } else if (compression_type == CompressionType::None) {
                GetPathExtension(output_filename, &compression_type);
            }
        } else if (output_directory) {
            if (!src_filename) {
                LogError("Cannot compress standard input to directory");
                return 1;
            }

            const char *compression_ext = CompressionTypeExtensions[(int)compression_type];
            if (!compression_ext) {
                LogError("Cannot guess output filename without compression type");
                return 1;
            }

            const char *basename = SplitStrReverseAny(src_filename, RG_PATH_SEPARATORS).ptr;
            output_filename = Fmt(&temp_alloc, "%1%/%2%3", output_directory, basename, compression_ext).ptr;
        } else if (src_filename) {
            const char *compression_ext = CompressionTypeExtensions[(int)compression_type];
            if (!compression_ext) {
                LogError("Cannot guess output filename without compression type");
                return 1;
            }

            output_filename = Fmt(&temp_alloc, "%1%2", src_filename, compression_ext).ptr;
        }

        if (compression_type == CompressionType::None) {
            LogError("Cannot determine compression type, use --algorithm");
            return 1;
        }

        dest_filenames.Append(output_filename);
    } else {
        const char *compression_ext = CompressionTypeExtensions[(int)compression_type];
        if (compression_type == CompressionType::None) {
            LogError("You must set an explicit compression type for multiple files");
            return 1;
        }

        for (const char *src_filename: src_filenames) {
            if (output_directory) {
                const char *basename = SplitStrReverseAny(src_filename, RG_PATH_SEPARATORS).ptr;

                const char *dest_filename = Fmt(&temp_alloc, "%1%/%2%3", output_directory, basename, compression_ext).ptr;
                dest_filenames.Append(dest_filename);
            } else {
                const char *dest_filename = Fmt(&temp_alloc, "%1%2", src_filename, compression_ext).ptr;
                dest_filenames.Append(dest_filename);
            }
        }
    }
    RG_ASSERT(dest_filenames.len == src_filenames.len);

    unsigned int write_flags = (int)StreamWriterFlag::Atomic |
                               (force ? 0 : (int)StreamWriterFlag::Exclusive);

    Async async(-1, false);
    std::atomic_int compressions { 0 };

    for (Size i = 0; i < src_filenames.len; i++) {
        async.Run([&, i]() {
            const char *src_filename = src_filenames[i];
            const char *dest_filename = dest_filenames[i];

            StreamReader reader;
            StreamWriter writer;

            if (src_filename) {
                if (reader.Open(src_filename) != OpenResult::Success)
                    return false;
            } else {
                if (!reader.Open(STDIN_FILENO, "<stdin>"))
                    return false;
            }

            if (dest_filename) {
                if (!writer.Open(dest_filename, write_flags, compression_type, compression_speed))
                    return false;

                const char *basename = SplitStrReverseAny(dest_filename, RG_PATH_SEPARATORS).ptr;
                LogInfo("Compressing '%1'...", basename);
            } else {
                if (!writer.Open(STDOUT_FILENO, "<stdout>", write_flags, compression_type, compression_speed))
                    return false;

                LogInfo("Compressing to standard output...");
            }

            if (!SpliceStream(&reader, -1, &writer))
                return false;
            if (!writer.Close())
                return false;

            compressions++;
            return true;
        });
    }

    bool success = async.Sync();

    if (success) {
        LogInfo("Done!");
        return 0;
    } else if (compressions) {
        LogInfo("Some files were compressed");
        return 1;
    } else {
        LogError("No successful compression");
        return 1;
    }
}

static int RunDecompress(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> src_filenames;
    const char *output_filename = nullptr;
    const char *output_directory = nullptr;
    CompressionType compression_type = CompressionType::None;
    bool force = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 decompress <source> [-O <destination>]
       %1 decompress <sources...> [-D <destination>]%!0

Options:
    %!..+-O, --output_file <file>%!0     Set output file
    %!..+-D, --output_dir <dir>%!0       Set output directory

    %!..+-a, --algorithm <algo>%!0       Set algorithm, see below

    %!..+-f, --force%!0                  Overwrite destination file

Available decompression algorithms: %!..+%2%!0)", FelixTarget, FmtSpan(AvailableAlgorithms));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                output_filename = opt.current_value;
            } else if (opt.Test("-D", "--output_dir", OptionType::Value)) {
                output_directory = opt.current_value;
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnumI(CompressionTypeNames, opt.current_value, &compression_type) ||
                        compression_type == CompressionType::None) {
                    LogError("Unknown compression algorithm '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-f", "--force")) {
                force = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.ConsumeNonOptions(&src_filenames);
    }

    if (!src_filenames.len) {
        LogError("Missing input filenames (use '-' for standard input)");
        return 1;
    }

    if (output_filename && output_directory) {
        LogError("Cannot use --output_file and --output_dir at the same time");
        return 1;
    }
    if (output_filename && src_filenames.len > 1) {
        LogError("Option --output_file can only be used with one input");
        return 1;
    }
    if (output_directory && !TestFile(output_directory, FileType::Directory)) {
        LogError("Output directory '%1' does not exist", output_directory);
        return 1;
    }

    struct DestinationFile {
        const char *filename;
        CompressionType compression_type;
    };

    HeapArray<DestinationFile> destinations;
    if (src_filenames.len == 1) {
        const char *src_filename = src_filenames[0];
        CompressionType type = compression_type;

        if (TestStr(src_filename, "-")) {
            src_filenames[0] = nullptr;
            src_filename = nullptr;
        }

        if (type == CompressionType::None) {
            if (!src_filename) {
                LogError("Cannot determine compression type from standard input");
                return 1;
            }

            Span<const char> ext = GetPathExtension(src_filename, &type);

            if (type == CompressionType::None) {
                LogError("Cannot determine compression type from extension '%1'", ext);
                return 1;
            }
        }

        if (output_filename) {
            if (TestStr(output_filename, "-")) {
                output_filename = nullptr;
            }

            destinations.Append({ output_filename, type });
        } else if (output_directory) {
            if (!src_filename) {
                LogError("Cannot compress standard input to directory");
                return 1;
            }

            Span<const char> compression_ext = CompressionTypeExtensions[(int)type];

            // Only strip matching compression extension
            if (!compression_ext.ptr || compression_ext != GetPathExtension(src_filename)) {
                compression_ext.len = 0;
            }

            Span<const char> basename = SplitStrReverseAny(src_filename, RG_PATH_SEPARATORS).ptr;
            const char *dest_filename = Fmt(&temp_alloc, "%1%/%2", output_directory, basename.Take(0, basename.len - compression_ext.len)).ptr;

            destinations.Append({ dest_filename, type });
        } else if (src_filename) {
            Span<const char> compression_ext = CompressionTypeExtensions[(int)type];

            if (!compression_ext.ptr || compression_ext != GetPathExtension(src_filename)) {
                LogError("Cannot guess output filename");
                return 1;
            }

            Size src_len = strlen(src_filename);
            const char *dest_filename = DuplicateString(MakeSpan(src_filename, src_len - compression_ext.len), &temp_alloc).ptr;

            destinations.Append({ dest_filename, type });
        } else {
            destinations.Append({ nullptr, type });
        }
    } else {
        bool valid = true;

        for (const char *src_filename: src_filenames) {
            CompressionType type = compression_type;

            if (type == CompressionType::None) {
                Span<const char> ext = GetPathExtension(src_filename, &type);

                if (type == CompressionType::None) {
                    LogError("Cannot determine compression type from extension '%1'", ext);

                    valid = false;
                    continue;
                }
            }

            if (output_directory) {
                Span<const char> compression_ext = CompressionTypeExtensions[(int)type];

                // Only strip matching compression extension
                if (!compression_ext.ptr || compression_ext != GetPathExtension(src_filename)) {
                    compression_ext.len = 0;
                }

                Span<const char> basename = SplitStrReverseAny(src_filename, RG_PATH_SEPARATORS).ptr;
                const char *dest_filename = Fmt(&temp_alloc, "%1%/%2", output_directory, basename.Take(0, basename.len - compression_ext.len)).ptr;

                destinations.Append({ dest_filename, type });
            } else {
                Span<const char> compression_ext = CompressionTypeExtensions[(int)type];

                if (!compression_ext.ptr || compression_ext != GetPathExtension(src_filename)) {
                    LogError("Cannot guess output filename");

                    valid = false;
                    continue;
                }

                Size src_len = strlen(src_filename);
                const char *dest_filename = DuplicateString(MakeSpan(src_filename, src_len - compression_ext.len), &temp_alloc).ptr;

                destinations.Append({ dest_filename, type });
            }
        }

        if (!valid)
            return 1;
    }
    RG_ASSERT(destinations.len == src_filenames.len);

    unsigned int write_flags = (int)StreamWriterFlag::Atomic |
                               (force ? 0 : (int)StreamWriterFlag::Exclusive);

    Async async(-1, false);
    std::atomic_int decompressions = 0;

    for (Size i = 0; i < src_filenames.len; i++) {
        async.Run([&, i] {
            const char *src_filename = src_filenames[i];
            const DestinationFile &dest = destinations[i];

            StreamReader reader;
            StreamWriter writer;

            if (src_filename) {
                if (reader.Open(src_filename, dest.compression_type) != OpenResult::Success)
                    return false;
            } else {
                if (!reader.Open(STDIN_FILENO, "<stdin>", dest.compression_type))
                    return false;
            }

            if (dest.filename) {
                if (!writer.Open(dest.filename, write_flags))
                    return false;

                const char *basename = SplitStrReverseAny(dest.filename, RG_PATH_SEPARATORS).ptr;
                LogInfo("Decompressing '%1'...", basename);
            } else {
                if (!writer.Open(STDOUT_FILENO, "<stdout>", write_flags))
                    return false;

                LogInfo("Decompressing to standard output...");
            }

            if (!SpliceStream(&reader, -1, &writer))
                return false;
            if (!writer.Close())
                return false;

            decompressions++;
            return true;
        });
    }

    bool success = async.Sync();

    if (success) {
        LogInfo("Done!");
        return 0;
    } else if (decompressions) {
        LogInfo("Some files were decompressed");
        return 1;
    } else {
        LogError("No successful decompression");
        return 1;
    }
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 <command> [args]%!0

Commands:
    %!..+compress%!0                     Compress file
    %!..+decompress%!0                   Decompress file

Use %!..+%1 help <command>%!0 or %!..+%1 <command> --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(StdErr);
        PrintLn(StdErr);
        LogError("No command provided");
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);

    // Handle help and version arguments
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            cmd = arguments[0];
            arguments[0] = (cmd[0] == '-') ? cmd : "--help";
        } else {
            print_usage(StdOut);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    if (TestStr(cmd, "compress")) {
        return RunCompress(arguments);
    } else if (TestStr(cmd, "decompress")) {
        return RunDecompress(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
