// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../lib/sqlite/sqlite3.h"
#include "../../libcc/libcc.hh"

sqlite3 *OpenDatabase(const char *filename, unsigned int flags);

bool InitDatabase(sqlite3 *db);
