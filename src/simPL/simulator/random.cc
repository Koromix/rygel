// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "random.hh"

#include <random>

static std::mt19937 random_generator(0);

void InitRandom(int seed)
{
    random_generator.seed(seed);
}

bool RandomBool(double probability)
{
    return std::uniform_real_distribution(0.0, 1.0)(random_generator) >= probability;
}

int RandomIntUniform(int min, int max)
{
    return std::uniform_int_distribution(min, max)(random_generator);
}

double RandomDoubleUniform(double min, double max)
{
    return std::uniform_real_distribution(min, max)(random_generator);
}

double RandomDoubleNormal(double mean, double sd)
{
     return std::normal_distribution(mean, sd)(random_generator);
}
