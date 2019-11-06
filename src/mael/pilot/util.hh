// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <Arduino.h>

#define STRINGIFY_(A) #A
#define STRINGIFY(A) STRINGIFY_(A)
#define CONCAT_(A, B) A ## B
#define CONCAT(A, B) CONCAT_(A, B)
#define UNIQUE_ID(Prefix) CONCAT(Prefix, __LINE__)

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
