// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/sqlite/sqlite.hh"

namespace K {

struct Config;

extern const int DatabaseVersion;

bool AddDatabaseFunctions(sq_Database *db);

int GetDatabaseVersion(sq_Database *db);
bool MigrateDatabase(sq_Database *db);
bool MigrateDatabase(const Config &config);

}
