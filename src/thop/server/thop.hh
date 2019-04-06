// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "../../drd/libdrd/libdrd.hh"

namespace RG {

struct Config;
struct StructureSet;
struct UserSet;

extern Config thop_config;
extern bool thop_has_casemix;

extern StructureSet thop_structure_set;
extern UserSet thop_user_set;

}
