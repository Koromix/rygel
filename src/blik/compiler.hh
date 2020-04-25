// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

struct TokenSet;

bool Compile(const TokenSet &set, const char *filename, Program *out_program);

}
