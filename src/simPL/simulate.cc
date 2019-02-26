// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "economics.hh"
#include "simulate.hh"
#include "tables.hh"
#include "../wrappers/pcg.hh"

void InitializeConfig(int count, int seed, SimulationConfig *out_config)
{
    out_config->count = count;
    out_config->seed = seed;
}

static void InitializeHuman(const SimulationConfig &config, Size idx, Human *out_human)
{
    pcg32_srandom_r(&out_human->rand_evolution, config.seed, idx);
    pcg32_srandom_r(&out_human->rand_therapy, config.seed, idx);

    out_human->alive = true;

    out_human->age = 45;
    out_human->sex = pcg_RandomBool(&out_human->rand_evolution, 0.5) ? Sex::Male : Sex::Female;

    out_human->smoking_status = pcg_RandomBool(&out_human->rand_evolution,
                                               GetSmokingPrevalence(out_human->age, out_human->sex));
}

Size InitializeHumans(const SimulationConfig &config, HeapArray<Human> *out_humans)
{
    for (Size i = 0; i < config.count; i++) {
        Human *new_human = out_humans->AppendDefault();
        InitializeHuman(config, i, new_human);
    }

    return config.count;
}

static bool SimulateYear(const SimulationConfig &, const Human &human, Human *out_human)
{
    *out_human = human;

    if (!human.alive)
        return false;

    out_human->age++;

    // Smoking
    if (human.smoking_status &&
            pcg_RandomBool(&out_human->rand_evolution,
                           GetSmokingStopTrialProbability(human.age))) {
        double stop_probability = 0.04;
        {
            double p = pcg_RandomUniform(&out_human->rand_evolution, 0.0, 1.0);

            if ((p -= 0.269) < 0.0) {
                stop_probability *= 2.29;
            } else if ((p -= 0.208) < 0.0) {
                stop_probability *= 1.5;
            }
        }

        if (pcg_RandomBool(&out_human->rand_evolution, stop_probability)) {
            out_human->smoking_status = false;
            out_human->smoking_cessation_age = human.age;
        }
    }

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

    // Economics
    out_human->utility += ComputeUtility(human);

    return true;
}

Size RunSimulationStep(const SimulationConfig &config, Span<const Human> humans,
                       HeapArray<Human> *out_humans)
{
    Size alive_count = 0;

    // Loop with copying, because we want to support overwriting a human
    for (Human human: humans) {
        Human *new_human = out_humans->AppendDefault();
        alive_count += SimulateYear(config, human, new_human);
    }

    // Return alive count (everyone for now)
    return alive_count;
}
