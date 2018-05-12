// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_authorizations.hh"
#include "mco_tables.hh"

static const char *const mco_options_usage =
R"(Common options:
    -D, --resource_dir <dir>     Add resource directory
                                 (default: <executable_dir>%/resources)
        --table_dir <dir>        Add table directory
                                 (default: <resource_dir>%/tables)
        --table_file <path>      Add table file
        --auth_file <path>       Set authorization file
                                 (default: <resource_dir>%/config/authorizations.ini)

    -O, --output <path>          Dump information to file
                                 (default: stdout))";

extern HeapArray<const char *> mco_resource_directories;
extern HeapArray<const char *> mco_table_directories;
extern HeapArray<const char *> mco_table_filenames;
extern const char *mco_authorization_filename;

bool mco_InitTableSet(Span<const char *const> resource_directories,
                      Span<const char *const> table_directories,
                      Span<const char *const> table_filenames,
                      mco_TableSet *out_set);
bool mco_InitAuthorizationSet(Span<const char *const> resource_directories,
                              const char *authorization_filename,
                              mco_AuthorizationSet *out_set);

const mco_TableSet *mco_GetMainTableSet();
const mco_AuthorizationSet *mco_GetMainAuthorizationSet();

bool mco_HandleMainOption(OptionParser &opt_parser, void (*usage_func)(FILE *fp));
