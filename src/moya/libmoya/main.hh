/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "../../common/kutil.hh"
#include "d_authorizations.hh"
#include "d_desc.hh"
#include "d_prices.hh"
#include "d_tables.hh"

static const char *const main_options_usage =
R"(Common options:
    -D, --data-dir <dir>         Add data directory
                                 (default: <executable_dir>/data)
        --table-dir <dir>        Add table directory
                                 (default: <data_dir>/tables)
        --pricing-file <path>    Set pricing file
                                 (default: <data_dir>/prices.nx)
        --auth-file <path>       Set authorization file
                                 (default: <data_dir>/authorizations.json)
        --catalog-dir <path>     Add catalog directory
                                 (default: <data_dir>/catalogs)

    -O, --output <path>          Dump information to file
                                 (default: stdout))";

extern HeapArray<const char *> main_data_directories;
extern HeapArray<const char *> main_table_directories;
extern const char *main_pricing_filename;
extern const char *main_authorization_filename;
extern HeapArray<const char *> main_catalog_directories;

bool InitTableSet(Span<const char *const> data_directories,
                  Span<const char *const> table_directories,
                  TableSet *out_set);
bool InitPricingSet(Span<const char *const> data_directories,
                    const char *pricing_filename,
                    PricingSet *out_set);
bool InitAuthorizationSet(Span<const char *const> data_directories,
                          const char *authorization_filename,
                          AuthorizationSet *out_set);
bool InitCatalogSet(Span<const char *const> data_directories,
                    Span<const char *const> catalog_directories,
                    CatalogSet *out_set);

const TableSet *GetMainTableSet();
const PricingSet *GetMainPricingSet();
const AuthorizationSet *GetMainAuthorizationSet();
const CatalogSet *GetMainCatalogSet();

bool HandleMainOption(OptionParser &opt_parser, void (*usage_func)(FILE *fp));
