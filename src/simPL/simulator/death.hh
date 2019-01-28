// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "human.hh"

enum class DeathFlag {
    Cardiovascular = 1 << 0,
    Others = 1 << 1
};

double GetDeathProbability(int age, Sex sex, unsigned int flags);
