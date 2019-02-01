// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "random.hh"

Random rand_human;
Random rand_therapy;

void Random::Init(int seed)
{
    random_generator.seed(seed);
}

bool Random::Bool(double probability)
{
    return std::uniform_real_distribution(0.0, 1.0)(random_generator) >= probability;
}

int Random::IntUniform(int min, int max)
{
    return std::uniform_int_distribution(min, max)(random_generator);
}

double Random::DoubleUniform(double min, double max)
{
    return std::uniform_real_distribution(min, max)(random_generator);
}

double Random::DoubleNormal(double mean, double sd)
{
     return std::normal_distribution(mean, sd)(random_generator);
}
