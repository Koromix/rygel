// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "../../core/libwrap/sqlite.hh"

namespace RG {

struct InstanceData {
    Config config;
    sq_Database db;
};

extern InstanceData *instance;

}
