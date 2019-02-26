// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "../../lib/pcg/pcg_basic.h"

enum class Sex {
    Male,
    Female
};
static const char *const SexNames[] = {
    "Male",
    "Female"
};

enum class DeathType {
    CardiacIschemia,
    LungCancer,
    OtherCauses
};
static const char *const DeathTypeNames[] = {
    "CardiacIschemia",
    "LungCancer",
    "OtherCauses"
};

struct Human {
    pcg32_random_t rand_evolution;
    pcg32_random_t rand_therapy;

    int age;
    Sex sex;

    bool smoking_status;
    int smoking_cessation_age;

    double SystolicPressure() const { return 0.0; }
    double TotalCholesterol() const { return 0.0; }

    bool alive;
    DeathType death_type;
};

extern "C" Size InitializeHumans(Size count, int seed, HeapArray<Human> *out_humans);
extern "C" Size RunSimulationStep(Span<const Human> humans, HeapArray<Human> *out_humans);
