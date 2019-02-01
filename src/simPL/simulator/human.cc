// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "human.hh"
#include "cardiovascular.hh"
#include "death.hh"
#include "exam.hh"
#include "flags.hh"
#include "random.hh"
#include "utility.hh"

Human CreateHuman(int id)
{
    Human human = {};

    // Identifier
    human.id = id;

    // Socio-demographic
    human.sex = rand_human.Bool(0.5) ? Sex::Male : Sex::Female;
    human.age = 44;
    human.sociocultural_level = rand_human.IntUniform(1, 4);

    // CV risk factors
    human.smoking_status = !rand_human.Bool(SmokingGetPrevalence(human.age, human.sex));
    human.bmi_base = BmiGetFirst(human.age, human.sex);
    human.systolic_pressure_base = SystolicPressureGetFirst(human.age, human.sex);
    human.total_cholesterol_base = CholesterolGetFirst(human.age, human.sex);

    // PL checkup
    human.checkup_age = rand_human.IntUniform(45, 75);

    // Death
    human.death_happened = false;

    return human;
}

Human SimulateOneYear(const Human &prev, uint64_t flags)
{
    DebugAssert(!prev.death_happened);

    Human next = {};

    // Identifier
    next.id = prev.id;

    // Socio-demographic
    next.age = prev.age + 1;
    next.sex = prev.sex;
    next.sociocultural_level = prev.sociocultural_level;

    // CV risk factors
    if (prev.smoking_status && !rand_human.Bool(SmokingGetCessationProbability(prev.age, prev.sex))) {
        next.smoking_status = false;
        next.smoking_cessation_age = next.age;
    } else {
        next.smoking_status = prev.smoking_status;
        next.smoking_cessation_age = 0;
    }
    next.bmi_base = BmiGetNext(prev.bmi_base, prev.age, prev.sex);
    next.systolic_pressure_base = SystolicPressureGetNext(prev.systolic_pressure_base, prev.age, prev.sex);
    next.total_cholesterol_base = CholesterolGetNext(prev.total_cholesterol_base, prev.age, prev.sex);

    // Drugs
    next.bmi_therapy = prev.bmi_therapy;
    next.systolic_pressure_therapy = prev.systolic_pressure_therapy;
    next.total_cholesterol_therapy = prev.total_cholesterol_therapy;

    // PL checkup
    next.checkup_age = prev.checkup_age;
    if ((flags & (uint64_t)SimulationFlag::EnablePL) && next.checkup_age == next.age) {
        RunLongevityCheckUp(&next);
    }

    // Death
    {
        unsigned int death_flags = UINT_MAX &
                                   ~(int)DeathFlag::Cardiovascular;
        double death_probability = ScoreComputeProbability(next) +
                                   GetDeathProbability(next.age, next.sex, death_flags);

        next.death_happened = !rand_human.Bool(death_probability);
    }

    // Utility
    next.utility = UtilityCompute(next);

    return next;
}
