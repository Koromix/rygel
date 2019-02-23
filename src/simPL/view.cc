// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "simPL.hh"
#include "view.hh"
#include "../../lib/imgui/imgui.h"

void RenderControlWindow(HeapArray<Simulation> *simulations)
{
    static int simulations_id = 0;

    static int count = 20000;
    static int seed = 0;

    ImGui::Begin("Control");
    ImGui::InputInt("Count", &count);
    ImGui::InputInt("Seed", &seed);
    if (ImGui::Button("Run!")) {
        Simulation *simulation = simulations->AppendDefault();

        Fmt(simulation->name, "Simulation #%1", ++simulations_id);
        simulation->count = count;
        simulation->seed = seed;
#ifdef SIMPL_ENABLE_HOT_RELOAD
        simulation->auto_restart = true;
#endif

        simulation->Start();
    }
    ImGui::End();
}

bool RenderSimulationWindow(Simulation *simulation)
{
    bool open = true;

    ImGui::Begin(simulation->name, &open);
    if (ImGui::Button("Restart")) {
        simulation->Start();
    }
#ifdef SIMPL_ENABLE_HOT_RELOAD
    ImGui::SameLine(); ImGui::Checkbox("Auto-restart", &simulation->auto_restart);
#endif
    ImGui::TextUnformatted(Fmt(&frame_alloc, "Count: %1", simulation->humans.len).ptr);
    ImGui::TextUnformatted(Fmt(&frame_alloc, "Iteration: %1", simulation->iteration).ptr);
    ImGui::TextUnformatted(Fmt(&frame_alloc, "Alive: %1", simulation->alive_count).ptr);
    ImGui::End();

    return open;
}
