// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "src/drd/libdrd/libdrd.hh"

namespace K {

struct Config;
struct StructureSet;
struct UserSet;

extern Config thop_config;
extern bool thop_has_casemix;

extern StructureSet thop_structure_set;
extern UserSet thop_user_set;

extern char thop_etag[17];

}
