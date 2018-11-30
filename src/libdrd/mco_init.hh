// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_authorization.hh"
#include "mco_tables.hh"

bool mco_InitTableSet(Span<const char *const> table_directories,
                      Span<const char *const> table_filenames,
                      mco_TableSet *out_set);

bool mco_InitAuthorizationSet(const char *config_directory,
                              const char *authorization_filename,
                              mco_AuthorizationSet *out_set);
