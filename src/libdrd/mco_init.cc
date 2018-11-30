// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "mco_init.hh"

bool mco_InitTableSet(Span<const char *const> table_directories,
                      Span<const char *const> table_filenames,
                      mco_TableSet *out_set)
{
    LogInfo("Load tables");

    BlockAllocator temp_alloc(Kibibytes(8));

    HeapArray<const char *> filenames;
    {
        const auto enumerate_directory_files = [&](const char *dir) {
            EnumStatus status = EnumerateDirectory(dir, nullptr, 1024,
                                                   [&](const char *filename, const FileInfo &info) {
                CompressionType compression_type;
                const char *ext = GetPathExtension(filename, &compression_type).ptr;

                if (info.type == FileType::File &&
                        (TestStr(ext, ".tab") || TestStr(ext, ".dpri"))) {
                    filenames.Append(Fmt(&temp_alloc, "%1%/%2", dir, filename).ptr);
                }

                return true;
            });

            return status != EnumStatus::Error;
        };

        bool success = true;
        for (const char *resource_dir: table_directories) {
            const char *tab_dir = Fmt(&temp_alloc, "%1%/mco_tables", resource_dir).ptr;
            if (TestPath(tab_dir, FileType::Directory)) {
                success &= enumerate_directory_files(tab_dir);
            }
        }
        filenames.Append(table_filenames);
        if (!success)
            return false;
    }

    if (!filenames.len) {
        LogError("No table specified or found");
    }

    // Load tables
    {
        mco_TableSetBuilder table_set_builder;
        if (!table_set_builder.LoadFiles(filenames))
            return false;
        if (!table_set_builder.Finish(out_set))
            return false;
    }

    return true;
}

bool mco_InitAuthorizationSet(const char *config_directory,
                              const char *authorization_filename,
                              mco_AuthorizationSet *out_set)
{
    LogInfo("Load authorizations");

    static const char *const default_names[] = {
        "mco_authorizations.ini",
        "mco_authorizations.txt"
    };

    BlockAllocator temp_alloc(Kibibytes(8));

    const char *filename = nullptr;
    {
        if (authorization_filename) {
            filename = authorization_filename;
        } else {
            for (const char *default_name: default_names) {
                const char *test_filename = Fmt(&temp_alloc, "%1%/%2",
                                                config_directory, default_name).ptr;
                if (TestPath(test_filename, FileType::File)) {
                    filename = test_filename;
                    break;
                }
            }
        }
    }

    if (filename && filename[0]) {
        mco_AuthorizationSetBuilder authorization_set_builder;
        if (!authorization_set_builder.LoadFiles(filename))
            return false;
        authorization_set_builder.Finish(out_set);
    } else {
        LogError("No authorization file specified or found");
    }

    return true;
}
