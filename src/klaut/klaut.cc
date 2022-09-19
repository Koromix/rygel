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
#include "chunker.hh"
#include "disk.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#include "vendor/curl/include/curl/curl.h"

namespace RG {

static int RunStore(Span<const char *> arguments)
{
    // Options
    const char *dest_directory = nullptr;
    const char *encrypt_key = nullptr;
    bool verbose = false;
    const char *filename = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 store <filename> [-O <dir>]%!0

Options:
    %!..+-O, --output_dir <dir>%!0       Put file fragments in <dir>
    %!..+-k, --encrypt_key <key>%!0      Set encryption key

    %!..+-v, --verbose%!0                Show precise chunk size

If no output directory is provided, the chunks are simply detected.)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-O", "--output_dir", OptionType::Value)) {
                dest_directory = opt.current_value;
            } else if (opt.Test("-k", "--encrypt_key", OptionType::Value)) {
                encrypt_key = opt.current_value;
            } else if (opt.Test("-v", "--verbose")) {
                verbose = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        filename = opt.ConsumeNonOption();

        if (!filename) {
            LogError("No filename provided");
            return 1;
        }

        if (dest_directory && !encrypt_key) {
            LogError("Missing encryption key");
            return 1;
        } else if (encrypt_key && !dest_directory) {
            LogError("Missing destination directory");
            return 1;
        }
    }

    kt_Disk *disk = dest_directory ? kt_OpenLocalDisk(dest_directory, encrypt_key) : nullptr;
    if (dest_directory && !disk)
        return 1;
    RG_DEFER { delete disk; };

    // Now, split the file
    Size written = 0;
    {
        StreamReader st(filename);
        kt_Chunker chunker(Kibibytes(256), Kibibytes(128), Kibibytes(768));
        HeapArray<uint8_t> buf;

        do {
            Size processed = 0;

            do {
                buf.Grow(Mebibytes(1));

                Size read = st.Read(buf.TakeAvailable());
                if (read < 0)
                    return 1;
                buf.len += read;

                processed = chunker.Process(buf, st.IsEOF(), [&](Size total, Span<const uint8_t> chunk) {
                    kt_Hash id = {};
                    crypto_generichash_blake2b(id.hash, RG_SIZE(id.hash), chunk.ptr, chunk.len, nullptr, 0);

                    if (verbose) {
                        LogInfo("Chunk %!..+%1%!0 [0x%2, %3]", id, FmtHex(total).Pad0(-8), chunk.len);
                    } else {
                        LogInfo("Chunk %!..+%1%!0 (%2)", id, verbose ? FmtArg(chunk.len) : FmtDiskSize(chunk.len));
                    }

                    if (disk) {
                        Size ret = disk->WriteChunk(id, chunk);
                        if (ret < 0)
                            return false;
                        written += ret;
                    }

                    return true;
                });
                if (processed < 0)
                    return 1;
            } while (!processed);

            memmove_safe(buf.ptr, buf.ptr + processed, buf.len - processed);
            buf.len -= processed;
        } while (!st.IsEOF());
    }

    if (disk) {
        LogInfo("Total written: %!..+%1%!0", verbose ? FmtArg(written) : FmtDiskSize(written));
    }

    return 0;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

Commands:
    %!..+store%!0                        Store file in deduplicated chunks

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

    if (TestStr(cmd, "store")) {
        return RunStore(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
