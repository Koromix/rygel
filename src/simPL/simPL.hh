// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "simulate.hh"

extern BlockAllocator frame_alloc;

extern decltype(InitializeConfig) *InitializeConfig_;
extern decltype(InitializeHumans) *InitializeHumans_;
extern decltype(RunSimulationStep) *RunSimulationStep_;

struct Simulation {
    char name[32];

    // Controls
    bool pause;
#ifdef SIMPL_ENABLE_HOT_RELOAD
    bool auto_reset;
#endif

    // Configuration
    SimulationConfig config;

    // Core data
    HeapArray<Human> humans;
    int iteration;
    Size alive_count;

    // Control methods
    void Reset();
};
