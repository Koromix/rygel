// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "simulate.hh"
#include "tables.hh"
#include "../wrappers/pcg.hh"

static void InitializeHuman(int seed, Size idx, Human *out_human)
{
    pcg32_srandom_r(&out_human->rand_evolution, seed, idx);
    pcg32_srandom_r(&out_human->rand_therapy, seed, idx);

    out_human->alive = true;

    out_human->age = 45;
    out_human->sex = pcg_RandomBool(&out_human->rand_evolution, 0.5) ? Sex::Male : Sex::Female;
}

Size InitializeHumans(Size count, int seed, HeapArray<Human> *out_humans)
{
    for (Size i = 0; i < count; i++) {
        Human *new_human = out_humans->AppendDefault();
        InitializeHuman(seed, i, new_human);
    }

    return count;
}

static bool SimulateYear(const Human &human, Human *out_human)
{
    *out_human = human;

    if (!human.alive)
        return false;

    out_human->age++;

    // Death?
    if (double p = pcg_RandomUniform(&out_human->rand_evolution, 0.0, 1.0);
            p < GetDeathProbability(human.age, human.sex, UINT_MAX)) {
        out_human->alive = false;

        // Assign OtherCauses in case the loop fails due to rounding
        out_human->death_type = DeathType::OtherCauses;
        for (Size i = 0; i < ARRAY_SIZE(DeathTypeNames); i++) {
            p -= GetDeathProbability(human.age, human.sex, 1 << i);
            if (p <= 0.0) {
                out_human->death_type = (DeathType)i;
                break;
            }
        }
    }

    return true;
}

Size RunSimulationStep(Span<const Human> humans, HeapArray<Human> *out_humans)
{
    Size alive_count = 0;

    // Loop with copying, because we want to support overwriting a human
    for (const Human human: humans) {
        Human *new_human = out_humans->AppendDefault();
        alive_count += SimulateYear(human, new_human);
    }

    // Return alive count (everyone for now)
    return alive_count;
}
