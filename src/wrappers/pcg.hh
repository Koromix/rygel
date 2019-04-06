// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <random>

#include "../libcc/libcc.hh"
#include "../../vendor/pcg/pcg_basic.h"

namespace RG {

// Adapt PCG to bullshit std::random API
class pcg_Generator {
    pcg32_random_t *rand;

public:
    typedef uint32_t result_type;

    pcg_Generator(pcg32_random_t *rand) : rand(rand) {}

    static uint32_t min() { return 0; }
    static uint32_t max() { return UINT32_MAX; }

    uint32_t operator()() { return pcg32_random_r(rand); }
};

static inline bool pcg_RandomBool(pcg32_random_t *rand, double probability)
{
    pcg_Generator generator(rand);
    return std::uniform_real_distribution(0.0, 1.0)(generator) < probability;
}

static inline int pcg_RandomUniform(pcg32_random_t *rand, int min, int max)
{
    pcg_Generator generator(rand);
    return std::uniform_int_distribution(min, max - 1)(generator);
}

static inline double pcg_RandomUniform(pcg32_random_t *rand, double min, double max)
{
    pcg_Generator generator(rand);
    return std::uniform_real_distribution(min, max)(generator);
}

static inline double pcg_RandomNormal(pcg32_random_t *rand, double mean, double sd)
{
    pcg_Generator generator(rand);
    return std::normal_distribution(mean, sd)(generator);
}

}
