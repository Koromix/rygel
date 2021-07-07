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
#include "../core/libsqlite/libsqlite.hh"

namespace RG {

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> src_filenames;
    const char *dest_directory = nullptr;
    bool force = false;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 [options] <snapshot...>%!0

Options:
    %!..+-O, --output_dir <dir>%!0       Restore inside this directory (instead of real path)
    %!..+-f, --force%!0                  Overwrite exisiting databases

As a precaution, you need to use %!..+--force%!0 if you don't use %!..+--output_dir%!0.)", FelixTarget);
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
            } else if (opt.Test("-O", "--output_dir", OptionType::Value)) {
                dest_directory = opt.current_value;
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
        LogError("No snapshot filename provided");
        return 1;
    }
    if (!dest_directory && !force) {
        LogError("No destination filename provided (and -f was not specified)");
        return 1;
    }

    sq_SnapshotSet snapshot_set;
    if (!sq_CollectSnapshots(src_filenames, &snapshot_set))
        return 1;

    bool complete = true;
    for (const sq_SnapshotInfo &snapshot: snapshot_set.snapshots) {
        const char *dest_filename;
        if (dest_directory) {
            HeapArray<char> buf(&temp_alloc);

            Fmt(&buf, "%1%/", dest_directory);
            for (Size i = 0; snapshot.orig_filename[i]; i++) {
                char c = snapshot.orig_filename[i];
                buf.Append(IsAsciiAlphaOrDigit(c) || c == '.' ? c : '_');
            }
            buf.Append(0);

            dest_filename = buf.TrimAndLeak().ptr;
        } else {
            dest_filename = snapshot.orig_filename;
        }

        complete &= sq_RestoreSnapshot(snapshot, dest_filename, force);
    }

    return !complete;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
