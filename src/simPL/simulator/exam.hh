// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "human.hh"

enum class ExamResult {
    Normal,
    Fragile,
    Diseased
};

ExamResult ExamineCognition(const Human &human);
ExamResult ExamineAnxietyDepression(const Human &human);
ExamResult ExamineSleep(const Human &human);
ExamResult ExamineNutrition(const Human &human);
ExamResult ExaminePhysicalActivity(const Human &human);

void RunLongevityCheckUp(Human *human);

void RunCoachingCognitiveSession(Human *human);
void RunCoachingLifestyleSession(Human *human);
