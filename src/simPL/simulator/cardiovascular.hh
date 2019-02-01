// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "human.hh"

double SmokingGetPrevalence(int age, Sex sex);
double SmokingGetCessationProbability(int age, Sex sex);

double SystolicPressureGetFirst(int age, Sex sex);
double SystolicPressureGetNext(double value, int age, Sex sex);

double BmiGetFirst(int age, Sex sex);
double BmiGetNext(double value, int age, Sex sex);

double CholesterolGetFirst(int age, Sex sex);
double CholesterolGetNext(double value, int age, Sex sex);

double ScoreComputeProbability(const Human &human);
