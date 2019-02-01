// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

#include <random>

class Random {
    std::mt19937 random_generator;

public:
    void Init(int seed);

    bool Bool(double probability);
    int IntUniform(int min, int max);
    double DoubleUniform(double min, double max);
    double DoubleNormal(double mean, double sd);
};

extern Random rand_human;
extern Random rand_therapy;
