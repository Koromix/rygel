// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"

namespace RG {

class sq_Database;

extern const char *const DefaultConfig;
extern const int DatabaseVersion;

bool MigrateDatabase(sq_Database &database, int version);

}
