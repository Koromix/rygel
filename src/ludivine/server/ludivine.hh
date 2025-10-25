// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "config.hh"
#include "src/core/sqlite/sqlite.hh"

namespace K {

extern Config config;
extern sq_Database db;

}
