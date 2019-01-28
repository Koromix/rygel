// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

void InitRandom(int seed);

bool RandomBool(double probability);
int RandomIntUniform(int min, int max);
double RandomDoubleUniform(double min, double max);
double RandomDoubleNormal(double mean, double sd);
