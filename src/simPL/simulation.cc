// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "simulation.hh"

static void InitializeHuman(Size idx, Human *out_human)
{
    pcg32_srandom_r(&out_human->rand_evolution, 1, idx);
    pcg32_srandom_r(&out_human->rand_therapy, 1, idx);

    out_human->age = 45;
}

void InitializeHumans(Size count, HeapArray<Human> *out_humans)
{
    for (Size i = 0; i < count; i++) {
        Human *new_human = out_humans->AppendDefault();
        InitializeHuman(i, new_human);
    }
}

static void SimulateYear(const Human &human, Human *out_human)
{
    out_human->age = human.age + 1;
}

void RunSimulationStep(Span<const Human> humans, HeapArray<Human> *out_humans)
{
    // Loop with copying, because we want to support overwriting a human
    for (const Human human: humans) {
        Human *new_human = out_humans->AppendDefault();
        SimulateYear(human, new_human);
    }
}
