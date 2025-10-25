// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"

namespace K {

struct Config;
class sq_Database;
class DomainHolder;
class InstanceHolder;

extern const int64_t FullSnapshotDelay;

extern Config gp_config;
extern sq_Database gp_db;

}
