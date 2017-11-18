/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "kutil.hh"
#include "data.hh"
#include "main.hh"
#include "pricing.hh"
#include "tables.hh"

HeapArray<const char *> main_data_directories;
HeapArray<const char *> main_table_directories;
HeapArray<const char *> main_table_filenames;
const char *main_pricing_filename;
const char *main_authorization_filename;

static TableSet main_table_set;
static PricingSet main_pricing_set;
static AuthorizationSet main_authorization_set;

bool InitTableSet(ArrayRef<const char *> data_directories,
                  ArrayRef<const char *> table_directories,
                  ArrayRef<const char *> table_filenames,
                  TableSet *out_set)
{
    Allocator temp_alloc;

    HeapArray<const char *> filenames;
    {
        bool success = true;
        for (const char *data_dir: data_directories) {
            const char *dir = Fmt(&temp_alloc, "%1%/tables", data_dir).ptr;
            if (TestPath(dir, FileType::Directory)) {
                success &= EnumerateDirectoryFiles(dir, "*.tab", &temp_alloc,
                                                   &filenames, 1024);
            }
        }
        for (const char *dir: table_directories) {
            success &= EnumerateDirectoryFiles(dir, "*.tab", &temp_alloc,
                                               &filenames, 1024);
        }
        filenames.Append(table_filenames);
        if (!success)
            return false;
    }

    if (!filenames.len) {
        LogError("No table specified or found");
        return true;
    }

    LoadTableFiles(filenames, out_set);
    if (!out_set->indexes.len)
        return false;

    return true;
}

bool InitPricingSet(ArrayRef<const char *> data_directories,
                    const char *pricing_filename,
                    PricingSet *out_set)
{
    Allocator temp_alloc;

    const char *filename = nullptr;
    {
        if (pricing_filename) {
            filename = pricing_filename;
        } else {
            for (Size i = data_directories.len; i-- > 0;) {
                const char *data_dir = data_directories[i];

                const char *test_filename = Fmt(&temp_alloc, "%1%/pricing.nx", data_dir).ptr;
                if (TestPath(test_filename, FileType::File)) {
                    filename = test_filename;
                    break;
                }
            }
        }
    }

    if (!filename || !filename[0]) {
        LogError("No pricing file specified or found");
        return true;
    }

    if (!LoadPricingFile(filename, out_set))
        return false;

    return true;
}

bool InitAuthorizationSet(ArrayRef<const char *> data_directories,
                          const char *authorization_filename,
                          AuthorizationSet *out_set)
{
    Allocator temp_alloc;

    const char *filename = nullptr;
    {
        if (authorization_filename) {
            filename = authorization_filename;
        } else {
            for (Size i = data_directories.len; i-- > 0;) {
                const char *data_dir = data_directories[i];

                const char *test_filename = Fmt(&temp_alloc, "%1%/authorizations.json", data_dir).ptr;
                if (TestPath(test_filename, FileType::File)) {
                    filename = test_filename;
                    break;
                }
            }
        }
    }

    if (!filename || !filename[0]) {
        LogError("No authorization file specified or found");
        return true;
    }

    if (!LoadAuthorizationFile(filename, out_set))
        return false;

    return true;
}

const TableSet *GetMainTableSet()
{
    if (!main_table_set.indexes.len) {
        if (!InitTableSet(main_data_directories, main_table_directories,
                          main_table_filenames, &main_table_set))
            return nullptr;
    }

    return &main_table_set;
}

const PricingSet *GetMainPricingSet()
{
    if (!main_pricing_set.ghs_pricings.len) {
        if (!InitPricingSet(main_data_directories, main_pricing_filename,
                            &main_pricing_set))
            return nullptr;
    }

    return &main_pricing_set;
}

const AuthorizationSet *GetMainAuthorizationSet()
{
    if (!main_authorization_set.authorizations.len) {
        if (!InitAuthorizationSet(main_data_directories, main_authorization_filename,
                                  &main_authorization_set))
            return nullptr;
    }

    return &main_authorization_set;
}

bool HandleMainOption(OptionParser &opt_parser, void (*usage_func)(FILE *fp))
{
    if (opt_parser.TestOption("-O", "--output")) {
        const char *filename = opt_parser.RequireOptionValue(usage_func);
        if (!filename)
            return false;

        if (!freopen(filename, "w", stdout)) {
            LogError("Cannot open '%1': %2", filename, strerror(errno));
            return false;
        }
        return true;
    } else if (opt_parser.TestOption("-D", "--data-dir")) {
        if (!opt_parser.RequireOptionValue(usage_func))
            return false;

        main_data_directories.Append(opt_parser.current_value);
        return true;
    } else if (opt_parser.TestOption("--table-dir")) {
        if (!opt_parser.RequireOptionValue(usage_func))
            return false;

        main_table_directories.Append(opt_parser.current_value);
        return true;
    } else if (opt_parser.TestOption("--table-file")) {
        if (!opt_parser.RequireOptionValue(usage_func))
            return false;

        main_table_filenames.Append(opt_parser.current_value);
        return true;
    } else if (opt_parser.TestOption("--pricing-file")) {
        if (!opt_parser.RequireOptionValue(usage_func))
            return false;

        main_pricing_filename = opt_parser.current_value;
        return true;
    } else if (opt_parser.TestOption("--auth-file")) {
        if (!opt_parser.RequireOptionValue(usage_func))
            return false;

        main_authorization_filename = opt_parser.current_value;
        return true;
    } else {
        PrintLn(stderr, "Unknown option '%1'", opt_parser.current_option);
        usage_func(stderr);
        return false;
    }
}
