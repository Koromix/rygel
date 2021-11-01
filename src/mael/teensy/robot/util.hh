// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <Arduino.h>
#include <assert.h>

#define DEG2RAD 0.01745329251
#define RAD2DEG 57.2957795457

#define STRINGIFY_(A) #A
#define STRINGIFY(A) STRINGIFY_(A)
#define CONCAT_(A, B) A ## B
#define CONCAT(A, B) CONCAT_(A, B)
#define UNIQUE_ID(Prefix) CONCAT(Prefix, __LINE__)

#define STATIC_ASSERT(Cond) \
    static_assert((Cond), STRINGIFY(Cond))

template <typename T, unsigned N>
char (&ComputeArraySize(T const (&)[N]))[N];
#define LEN(Array) sizeof(ComputeArraySize(Array))

class WaitFor {
    unsigned long delay;
    unsigned long last_execution;

public:
    WaitFor(long delay_us) : delay((unsigned long)delay_us) {}

    bool Test()
    {
        unsigned long now = micros();

        if (last_execution < now - delay) {
            last_execution += delay;
            return true;
        } else {
            return false;
        }
    };
};

#define PROCESS_EVERY(DelayUs) \
    static WaitFor UNIQUE_ID(wf_)(DelayUs); \
    if (!UNIQUE_ID(wf_).Test()) \
        return;
