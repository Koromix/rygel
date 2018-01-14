// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "main.hh"
#include "d_desc.hh"
#include "d_tables.hh"

HeapArray<const char *> main_data_directories;
HeapArray<const char *> main_table_directories;
HeapArray<const char *> main_price_filenames;
const char *main_authorization_filename;
HeapArray<const char *> main_catalog_directories;

bool InitTableSet(Span<const char *const> data_directories,
                  Span<const char *const> table_directories,
                  Span<const char *const> price_filenames,
                  TableSet *out_set)
{
    Allocator temp_alloc;

    HeapArray<const char *> tab_filenames2;
    HeapArray<const char *> price_filenames2;
    {
        bool success = true;
        for (const char *data_dir: data_directories) {
            const char *tab_dir = Fmt(&temp_alloc, "%1%/tables", data_dir).ptr;
            if (TestPath(tab_dir, FileType::Directory)) {
                success &= EnumerateDirectoryFiles(tab_dir, "*.tab*", &temp_alloc,
                                                   &tab_filenames2, 1024);
            }

            const char *price_filename = Fmt(&temp_alloc, "%1%/tables%/prices.json", data_dir).ptr;
            if (TestPath(price_filename, FileType::File)) {
                price_filenames2.Append(price_filename);
            }
        }
        for (const char *dir: table_directories) {
            success &= EnumerateDirectoryFiles(dir, "*.tab*", &temp_alloc, &tab_filenames2, 1024);
        }
        price_filenames2.Append(price_filenames);
        if (!success)
            return false;
    }

    if (!price_filenames2.len) {
        LogError("No price file specified or found");
    }
    if (!tab_filenames2.len) {
        LogError("No table specified or found");
    }

    {
        TableSetBuilder table_set_builder;
        if (!table_set_builder.LoadFiles(tab_filenames2, price_filenames2))
            return false;
        if (!table_set_builder.Finish(out_set))
            return false;
    }

    return true;
}

bool InitAuthorizationSet(Span<const char *const> data_directories,
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

    if (filename && filename[0]) {
        if (!LoadAuthorizationFile(filename, out_set))
            return false;
    } else {
        LogError("No authorization file specified or found");
    }

    return true;
}

bool InitCatalogSet(Span<const char *const> data_directories,
                    Span<const char *const> catalog_directories,
                    CatalogSet *out_set)
{
    Allocator temp_alloc;

    HeapArray<const char *> directories;
    {
        for (const char *data_dir: data_directories) {
            const char *dir = Fmt(&temp_alloc, "%1%/catalogs", data_dir).ptr;
            directories.Append(dir);
        }
        directories.Append(catalog_directories);
    }

    bool success = true;
    for (Size i = directories.len - 1; i >= 0; i--) {
        if (!out_set->ghm_roots.len) {
            const char *filename = Fmt(&temp_alloc, "%1%/ghm_roots.json", directories[i]).ptr;
            if (TestPath(filename, FileType::File)) {
                success &= LoadGhmRootCatalog(filename, &out_set->str_alloc, &out_set->ghm_roots,
                                              &out_set->ghm_roots_map);
            }
        }
    }
    if (!success)
        return false;

    if (!out_set->ghm_roots.len) {
        LogError("No catalog specified or found");
    }

    return true;
}

const TableSet *GetMainTableSet()
{
    static TableSet table_set;
    static bool loaded = false;

    if (!loaded) {
        if (!InitTableSet(main_data_directories, main_table_directories, main_price_filenames,
                          &table_set))
            return nullptr;
        loaded = true;
    }

    return &table_set;
}

const AuthorizationSet *GetMainAuthorizationSet()
{
    static AuthorizationSet authorization_set;
    static bool loaded = false;

    if (!loaded) {
        if (!InitAuthorizationSet(main_data_directories, main_authorization_filename,
                                  &authorization_set))
            return nullptr;
        loaded = true;
    }

    return &authorization_set;
}

const CatalogSet *GetMainCatalogSet()
{
    static CatalogSet catalog_set;
    static bool loaded = false;

    if (!loaded) {
        if (!InitCatalogSet(main_data_directories, main_catalog_directories,
                            &catalog_set))
            return nullptr;
        loaded = true;
    }

    return &catalog_set;
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
    }  else if (opt_parser.TestOption("--price-file")) {
        if (!opt_parser.RequireOptionValue(usage_func))
            return false;

        main_price_filenames.Append(opt_parser.current_value);
        return true;
    } else if (opt_parser.TestOption("--auth-file")) {
        if (!opt_parser.RequireOptionValue(usage_func))
            return false;

        main_authorization_filename = opt_parser.current_value;
        return true;
    } else if (opt_parser.TestOption("--catalog-dir")) {
        if (!opt_parser.RequireOptionValue(usage_func))
            return false;

        main_catalog_directories.Append(opt_parser.current_value);
        return true;
    } else {
        PrintLn(stderr, "Unknown option '%1'", opt_parser.current_option);
        usage_func(stderr);
        return false;
    }
}
