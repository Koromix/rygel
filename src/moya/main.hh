/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "data.hh"
#include "pricing.hh"
#include "tables.hh"

static const char *const main_options_usage =
R"(Common options:
    -D, --data-dir <dir>         Add data directory
                                 (default: <executable_dir>/data)
        --table-dir <dir>        Add table directory
                                 (default: <data_dir>/tables)
        --table-file <path>      Add table file
        --pricing-file <path>    Set pricing file
                                 (default: <data_dir>/pricing.nx)
        --auth-file <path>       Set authorization file
                                 (default: <data_dir>/authorizations.json)

    -O, --output <path>          Dump information to file
                                 (default: stdout))";

extern HeapArray<const char *> main_data_directories;
extern HeapArray<const char *> main_table_directories;
extern HeapArray<const char *> main_table_filenames;
extern const char *main_pricing_filename;
extern const char *main_authorization_filename;

bool InitTableSet(ArrayRef<const char *> data_directories,
                  ArrayRef<const char *> table_directories,
                  ArrayRef<const char *> table_filenames,
                  TableSet *out_set);
bool InitPricingSet(ArrayRef<const char *> data_directories,
                    const char *pricing_filename,
                    PricingSet *out_set);
bool InitAuthorizationSet(ArrayRef<const char *> data_directories,
                          const char *authorization_filename,
                          AuthorizationSet *out_set);

const TableSet *GetMainTableSet();
const PricingSet *GetMainPricingSet();
const AuthorizationSet *GetMainAuthorizationSet();

bool HandleMainOption(OptionParser &opt_parser, void (*usage_func)(FILE *fp));
