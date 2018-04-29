// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_authorizations.hh"
#include "mco_tables.hh"

static const char *const mco_options_usage =
R"(Common options:
    -D, --data-dir <dir>         Add data directory
                                 (default: <executable_dir>%/data)
        --table-dir <dir>        Add table directory
                                 (default: <data_dir>%/tables)
        --price-file <path>      Set price file
                                 (default: <data_dir>%/tables%/prices.json)
        --auth-file <path>       Set authorization file
                                 (default: <data_dir>%/authorizations.json)

    -O, --output <path>          Dump information to file
                                 (default: stdout))";

extern HeapArray<const char *> mco_data_directories;
extern HeapArray<const char *> mco_table_directories;
extern HeapArray<const char *> mco_price_filenames;
extern const char *mco_authorization_filename;

bool mco_InitTableSet(Span<const char *const> data_directories,
                      Span<const char *const> table_directories,
                      Span<const char * const> price_filenames,
                      mco_TableSet *out_set);
bool mco_InitAuthorizationSet(Span<const char *const> data_directories,
                              const char *authorization_filename,
                              mco_AuthorizationSet *out_set);

const mco_TableSet *mco_GetMainTableSet();
const mco_AuthorizationSet *mco_GetMainAuthorizationSet();

bool mco_HandleMainOption(OptionParser &opt_parser, void (*usage_func)(FILE *fp));
