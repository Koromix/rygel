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

    // Death histogram and table
    if (ImGui::TreeNode("Deaths")) {
        // It's actually off by one, because 0 is 'All'
        static int histogram_type = 0;
        ImGui::Combo("Type", &histogram_type, [](void *, int idx, const char **str) {
            *str = idx ? DeathTypeNames[idx - 1] : "All";
            return true;
        }, nullptr, ARRAY_SIZE(DeathTypeNames) + 1);

        float deaths[ARRAY_SIZE(DeathTypeNames) + 1][6] = {};
        for (const Human &human: simulation->humans) {
            if (!human.alive) {
                int age_cat = 0;
                if (human.age < 45) continue;
                else if (human.age < 55) age_cat = 0;
                else if (human.age < 65) age_cat = 1;
                else if (human.age < 75) age_cat = 2;
                else if (human.age < 85) age_cat = 3;
                else if (human.age < 95) age_cat = 4;
                else age_cat = 5;

                deaths[0][age_cat] += 1.0f;
                deaths[(int)human.death_type + 1][age_cat] += 1.0f;
            }
        }

        ImGui::PlotHistogram("Histogram", deaths[histogram_type], ARRAY_SIZE(deaths[histogram_type]), 0,
                             nullptr, 0.0f, (float)simulation->humans.len, ImVec2(0, 80));

        ImGui::Columns(7, "Table");
        ImGui::Separator();
        ImGui::Text("Cause"); ImGui::NextColumn();
        ImGui::Text("45-54"); ImGui::NextColumn();
        ImGui::Text("55-64"); ImGui::NextColumn();
        ImGui::Text("65-74"); ImGui::NextColumn();
        ImGui::Text("75-84"); ImGui::NextColumn();
        ImGui::Text("85-94"); ImGui::NextColumn();
        ImGui::Text("95+"); ImGui::NextColumn();
        ImGui::Separator();
        for (Size i = 0; i < ARRAY_SIZE(DeathTypeNames); i++) {
            ImGui::TextUnformatted(DeathTypeNames[i]); ImGui::NextColumn();
            for (Size j = 0; j < 6; j++) {
                if (deaths[0][j]) {
                    float proportion = (deaths[i + 1][j] / deaths[0][j]) * 100.0f;
                    const char *str = Fmt(&frame_alloc, "%1 (%2%%)",
                                          deaths[i + 1][j], FmtDouble(proportion, 1)).ptr;

                    ImGui::TextUnformatted(str); ImGui::NextColumn();
                } else {
                    ImGui::TextUnformatted("-"); ImGui::NextColumn();
                }
            }
        }
        ImGui::Columns(1);
        ImGui::Separator();

        ImGui::TreePop();
    }

    ImGui::End();

    return open;
}
