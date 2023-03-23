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
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/libcc/libcc.hh"

namespace RG {

// Skip None
static const Span<const char *const> AvailableAlgorithms = MakeSpan(CompressionTypeNames + 1, RG_LEN(CompressionTypeNames) - 1);

static int RunCompress(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *src_filename = nullptr;
    const char *dest_filename = nullptr;
    CompressionType compression_type = CompressionType::None;
    CompressionSpeed compression_speed = CompressionSpeed::Default;
    bool force = false;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 compress <source> [-O <destination>]

Options:
    %!..+-O, --output_file <file>%!0     Set output file

    %!..+-a, --algorithm <algo>%!0       Set algorithm, see below
    %!..+-s, --speed <speed>%!0          Set compression speed: Default, Fast or Slow
                                 %!D..(default: Default)%!0

    %!..+-f, --force%!0                  Overwrite destination file

Available compression algorithms: %!..+%2%!0)", FelixTarget, FmtSpan(AvailableAlgorithms));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                dest_filename = opt.current_value;
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnum(CompressionTypeNames, opt.current_value, &compression_type) ||
                        compression_type == CompressionType::None) {
                    LogError("Unknown compression algorithm '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-s", "--speed", OptionType::Value)) {
                if (TestStr(opt.current_value, "Default")) {
                    compression_speed = CompressionSpeed::Default;
                } else if (TestStr(opt.current_value, "Fast")) {
                    compression_speed = CompressionSpeed::Fast;
                } else if (TestStr(opt.current_value, "Slow")) {
                    compression_speed = CompressionSpeed::Slow;
                } else {
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

        src_filename = opt.ConsumeNonOption();
    }

    if (!src_filename) {
        LogError("Missing input filename");
        return 1;
    }

    if (dest_filename) {
        if (compression_type == CompressionType::None) {
            Span<const char> ext = GetPathExtension(dest_filename, &compression_type);

            if (compression_type == CompressionType::None) {
                LogError("Cannot determine compression type from extension '%1'", ext);
                return 1;
            }
        }
    } else {
        const char *compression_ext = CompressionTypeExtensions[(int)compression_type];

        if (!compression_ext) {
            LogError("Cannot guess output filename without compression type");
            return 1;
        }

        dest_filename = Fmt(&temp_alloc, "%1%2", src_filename, compression_ext).ptr;
    }

    unsigned int write_flags = (int)StreamWriterFlag::Atomic |
                               (force ? 0 : (int)StreamWriterFlag::Exclusive);

    StreamReader reader(src_filename);
    StreamWriter writer(dest_filename, write_flags, compression_type, compression_speed);

    LogInfo("Compressing...");
    if (!SpliceStream(&reader, -1, &writer))
        return 1;
    if (!writer.Close())
        return 1;

    LogInfo("Done!");
    return 0;
}

static int RunDecompress(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *src_filename = nullptr;
    const char *dest_filename = nullptr;
    CompressionType compression_type = CompressionType::None;
    bool force = false;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 decompress <source> [-O <destination>]

Options:
    %!..+-O, --output_file <file>%!0     Set output file

    %!..+-a, --algorithm <algo>%!0       Set algorithm, see below

    %!..+-f, --force%!0                  Overwrite destination file

Available decompression algorithms: %!..+%2%!0)", FelixTarget, FmtSpan(AvailableAlgorithms));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                dest_filename = opt.current_value;
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnum(CompressionTypeNames, opt.current_value, &compression_type) ||
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

        src_filename = opt.ConsumeNonOption();
    }

    if (!src_filename) {
        LogError("Missing input filename");
        return 1;
    }

    if (compression_type == CompressionType::None) {
        Span<const char> ext = GetPathExtension(src_filename, &compression_type);

        if (compression_type == CompressionType::None) {
            LogError("Cannot determine compression type from extension '%1'", ext);
            return 1;
        }
    }

    if (!dest_filename) {
        Span<const char> compression_ext = CompressionTypeExtensions[(int)compression_type];

        if (!compression_ext.ptr) {
            LogError("Cannot guess output filename");
            return 1;
        }
        if (compression_ext != GetPathExtension(src_filename)) {
            LogError("Cannot guess output filename");
            return 1;
        }

        Size src_len = strlen(src_filename);
        dest_filename = DuplicateString(MakeSpan(src_filename, src_len - compression_ext.len), &temp_alloc).ptr;
    }

    unsigned int write_flags = (int)StreamWriterFlag::Atomic |
                               (force ? 0 : (int)StreamWriterFlag::Exclusive);

    StreamReader reader(src_filename, compression_type);
    StreamWriter writer(dest_filename, write_flags);

    LogInfo("Decompressing...");
    if (!SpliceStream(&reader, -1, &writer))
        return 1;
    if (!writer.Close())
        return 1;

    LogInfo("Done!");
    return 0;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

Commands:
    %!..+compress%!0                     Compress file
    %!..+decompress%!0                   Decompress file

Use %!..+%1 help <command>%!0 or %!..+%1 <command> --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(stderr);
        PrintLn(stderr);
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
            print_usage(stdout);
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
int main(int argc, char **argv) { return RG::Main(argc, argv); }
