// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "../../lib/pcg/pcg_basic.h"

struct Human {
    pcg32_random_t rand_evolution;
    pcg32_random_t rand_therapy;

    int age;
};

extern "C" Size InitializeHumans(Size count, int seed, HeapArray<Human> *out_humans);
extern "C" Size RunSimulationStep(Span<const Human> humans, HeapArray<Human> *out_humans);
