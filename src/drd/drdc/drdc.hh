// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../libdrd/libdrd.hh"

namespace RG {

struct Config;

extern const char *const CommonOptions;

extern Config drdc_config;

bool HandleCommonOption(OptionParser &opt);

}
