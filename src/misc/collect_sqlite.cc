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
#include "../core/libwrap/sqlite.hh"

namespace RG {

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Options
    const char *src_filename = nullptr;
    const char *dest_filename = nullptr;
    bool force = false;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 [options] <database>%!0

Options:
    %!..+-O, --output_file <file>%!0     Change final database filename
    %!..+-f, --force%!0                  Allow overwrite of destination file)", FelixTarget);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %2", FelixTarget, FelixVersion);
        return 0;
    }

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                dest_filename = opt.current_value;
            } else if (opt.Test("-f", "--force")) {
                force = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        src_filename = opt.ConsumeNonOption();
        if (!src_filename) {
            LogError("No database filename provided");
            return 1;
        }
        dest_filename = dest_filename ? dest_filename : src_filename;
    }

    if (!force && TestFile(dest_filename)) {
        LogError("Refusing to overwrite '%1'", dest_filename);
        return 1;
    }
    if (!sq_RestoreDatabase(src_filename, dest_filename)) {
        UnlinkFile(dest_filename);
        return 1;
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
