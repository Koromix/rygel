// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "human.hh"
#include "exam.hh"

ExamResult ExamineCognition(const Human &)
{
    return ExamResult::Normal;
}

ExamResult ExamineAnxietyDepression(const Human &)
{
    return ExamResult::Normal;
}

ExamResult ExamineSleep(const Human &)
{
    return ExamResult::Normal;
}

ExamResult ExamineNutrition(const Human &)
{
    return ExamResult::Normal;
}

ExamResult ExaminePhysicalActivity(const Human &)
{
    return ExamResult::Normal;
}

void RunCoachingCognitiveSession(Human *)
{
    // TODO: When we known what it's supposed to do
}

void RunCoachingLifestyleSession(Human *)
{
    // TODO: When we known what it's supposed to do
}
