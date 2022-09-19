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

static int RunPutFile(Span<const char *> arguments)
{
    // Options
    const char *repo_directory = nullptr;
    const char *encrypt_key = nullptr;
    int verbose = 0;
    const char *filename = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 put_file <filename> [-O <dir>]%!0

Options:
    %!..+-R, --repository_dir <dir>%!0   Set repository directory
    %!..+-k, --encrypt_key <key>%!0      Set encryption key

    %!..+-v, --verbose%!0                Increase verbosity level (repeat for more)

If no output directory is provided, the chunks are simply detected.)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-R", "--repository_dir", OptionType::Value)) {
                repo_directory = opt.current_value;
            } else if (opt.Test("-k", "--encrypt_key", OptionType::Value)) {
                encrypt_key = opt.current_value;
            } else if (opt.Test("-v", "--verbose")) {
                verbose++;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        filename = opt.ConsumeNonOption();
    }

    if (!filename) {
        LogError("No filename provided");
        return 1;
    }
    if (!repo_directory) {
        LogError("Missing repository directory");
        return 1;
    }
    if (!encrypt_key) {
        LogError("Missing encryption key");
        return 1;
    }

    kt_Disk *disk = kt_OpenLocalDisk(repo_directory, kt_DiskMode::WriteOnly, encrypt_key);
    if (!disk)
        return 1;
    RG_DEFER { delete disk; };

    // Now, split the file
    HeapArray<uint8_t> summary;
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

                processed = chunker.Process(buf, st.IsEOF(), [&](Size idx, Size total, Span<const uint8_t> chunk) {
                    kt_Hash id = {};
                    crypto_generichash_blake2b(id.hash, RG_SIZE(id.hash), chunk.ptr, chunk.len, nullptr, 0);

                    if (verbose >= 2) {
                        LogInfo("Chunk %1: %!..+%2%!0 [0x%3, %4]", idx, id, FmtHex(total).Pad0(-8), chunk.len);
                    } else if (verbose) {
                        LogInfo("Chunk %1: %!..+%2%!0 (%3)", idx, id, FmtDiskSize(chunk.len));
                    }

                    Size ret = disk->WriteChunk(id, chunk);
                    if (ret < 0)
                        return false;
                    written += ret;

                    summary.Append(MakeSpan((const uint8_t *)&id, RG_SIZE(id)));

                    return true;
                });
                if (processed < 0)
                    return 1;
            } while (!processed);

            memmove_safe(buf.ptr, buf.ptr + processed, buf.len - processed);
            buf.len -= processed;
        } while (!st.IsEOF());
    }

    // Write list of chunks
    {
        kt_Hash id = {};
        crypto_generichash_blake2b(id.hash, RG_SIZE(id.hash), summary.ptr, summary.len, nullptr, 0);

        Size ret = disk->WriteChunk(id, summary);
        if (ret < 0)
            return 1;
        written += ret;

        LogInfo("Destination: %!..+%1%!0", id);
        LogInfo("Total written: %!..+%1%!0", verbose >= 2 ? FmtArg(written) : FmtDiskSize(written));
    }

    return 0;
}

static inline int ParseHexadecimalChar(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        return -1;
    }
}

static int RunGetFile(Span<const char *> arguments)
{
    // Options
    const char *repo_directory = nullptr;
    const char *dest_filename = nullptr;
    const char *decrypt_key = nullptr;
    int verbose = 0;
    const char *name = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 get_file <name> [-O <file>]%!0

Options:
    %!..+-R, --repository_dir <dir>%!0   Set repository directory
    %!..+-k, --decrypt_key <key>%!0      Set decryption key

    %!..+-O, --output_file <dir>%!0      Restore file to <file>

    %!..+-v, --verbose%!0                Increase verbosity level (repeat for more)

If no output directory is provided, the chunks are simply detected.)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-R", "--repository_dir", OptionType::Value)) {
                repo_directory = opt.current_value;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                dest_filename = opt.current_value;
            } else if (opt.Test("-k", "--decrypt_key", OptionType::Value)) {
                decrypt_key = opt.current_value;
            } else if (opt.Test("-v", "--verbose")) {
                verbose++;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        name = opt.ConsumeNonOption();
    }

    if (!repo_directory) {
        LogError("Missing repository directory");
        return 1;
    }
    if (!name) {
        LogError("No name provided");
        return 1;
    }
    if (!dest_filename) {
        LogError("Missing destination file");
        return 1;
    }
    if (!decrypt_key) {
        LogError("Missing decryption key");
        return 1;
    }

    kt_Disk *disk = kt_OpenLocalDisk(repo_directory, kt_DiskMode::ReadWrite, decrypt_key);
    if (!disk)
        return 1;
    RG_DEFER { delete disk; };

    // Open destination file
    StreamWriter writer(dest_filename);
    if (!writer.IsValid())
        return 1;

    // Read file summary
    HeapArray<uint8_t> summary;
    {
        kt_Hash id = {};

        for (Size i = 0, j = 0; name[j]; i++, j += 2) {
            int high = ParseHexadecimalChar(name[j]);
            int low = (high >= 0) ? ParseHexadecimalChar(name[j + 1]) : -1;

            if (low < 0) {
                LogError("Malformed object name '%1'", name);
                return 1;
            }

            id.hash[i] = (uint8_t)((high << 4) | low);
        }

        if (!disk->ReadChunk(id, &summary))
            return 1;
        if (summary.len % RG_SIZE(kt_Hash)) {
            LogError("Malformed file summary '%1'", name);
            return 1;
        }
    }

    // Write unencrypted file
    for (Size idx = 0, offset = 0; offset < summary.len; idx++, offset += RG_SIZE(kt_Hash)) {
        kt_Hash id = {};
        memcpy(&id, summary.ptr + offset, RG_SIZE(id));

        if (verbose) {
            LogInfo("Chunk %1: %!..+%2%!0", idx, id);
        }

        HeapArray<uint8_t> buf;
        if (!disk->ReadChunk(id, &buf))
            return 1;
        if (!writer.Write(buf))
            return 1;
    }

    if (!writer.Close())
        return 1;

    Size file_len = writer.GetRawWritten();
    LogInfo("Restored file: %!..+%1%!0 (%2)", dest_filename, verbose ? FmtArg(file_len) : FmtDiskSize(file_len));

    return 0;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

Commands:
    %!..+put_file%!0                     Store encrypted file to storage
    %!..+get_file%!0                     Get and decrypt file from storage

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

    if (TestStr(cmd, "put_file")) {
        return RunPutFile(arguments);
    } else if (TestStr(cmd, "get_file")) {
        return RunGetFile(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
