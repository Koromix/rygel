// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "src/core/sqlite/sqlite.hh"

namespace K {

extern const int DatabaseVersion;

int GetDatabaseVersion(sq_Database *db);
bool MigrateDatabase(sq_Database *db);

}
