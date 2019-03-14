// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "simPL.hh"
#include "view.hh"
#include "../../vendor/imgui/imgui.h"

static void InitializeSimulation(Simulation *out_simulation)
{
    static int simulations_id = 0;

    Fmt(out_simulation->name, "Simulation #%1", ++simulations_id);
    out_simulation->pause = true;
#ifdef SIMPL_ENABLE_HOT_RELOAD
    out_simulation->auto_reset = true;
#endif

    InitializeConfig_(&out_simulation->config);
}

void RenderMainMenu(HeapArray<Simulation> *simulations)
{
    ImGui::BeginMainMenuBar();

    if (ImGui::MenuItem("New simulation")) {
        Simulation *simulation = simulations->AppendDefault();
        InitializeSimulation(simulation);
    }

    ImGui::EndMainMenuBar();
}

static void RenderAgeTableHeaders()
{
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
}

bool RenderSimulationWindow(HeapArray<Simulation> *simulations, Size idx)
{
    Simulation *simulation = &(*simulations)[idx];

    bool open = true;
    ImGui::Begin(simulation->name, &open, ImVec2(500, 500));

    ImGui::Columns(3, nullptr, false);
    ImGui::TextUnformatted(Fmt(&frame_alloc, "Humans: %1", simulation->humans.len).ptr); ImGui::NextColumn();
    ImGui::TextUnformatted(Fmt(&frame_alloc, "Alive: %1", simulation->alive_count).ptr); ImGui::NextColumn();
    ImGui::TextUnformatted(Fmt(&frame_alloc, "Iteration: %1", simulation->iteration).ptr); ImGui::NextColumn();
    ImGui::Columns(1);

    if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Count", &simulation->config.count, 10, 1000);
        ImGui::InputInt("Seed", &simulation->config.seed);
        ImGui::InputDouble("Discount rate", &simulation->config.discount_rate, 0.01, 0.05, "%.2f");

        int predict_cvd_mode = (int)simulation->config.predict_cvd;
        ImGui::Combo("Predict CVD", &predict_cvd_mode, [](void *, int idx, const char **str) {
            *str = PredictCvdModeNames[idx];
            return true;
        }, nullptr, ARRAY_SIZE(PredictCvdModeNames));
        simulation->config.predict_cvd = (PredictCvdMode)predict_cvd_mode;

        int predict_lung_cancer_mode = (int)simulation->config.predict_lung_cancer;
        ImGui::Combo("Predict Lung Cancer", &predict_lung_cancer_mode, [](void *, int idx, const char **str) {
            *str = PredictLungCancerModeNames[idx];
            return true;
        }, nullptr, ARRAY_SIZE(PredictLungCancerModeNames));
        simulation->config.predict_lung_cancer = (PredictLungCancerMode)predict_lung_cancer_mode;
    }

    if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Start")) {
            simulation->Reset();
            simulation->pause = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            simulation->Reset();
        }

        ImGui::Checkbox("Pause", &simulation->pause);
#ifdef SIMPL_ENABLE_HOT_RELOAD
        ImGui::SameLine();
        ImGui::Checkbox("Auto-reset", &simulation->auto_reset);
#endif

        if (ImGui::Button("Copy")) {
            Simulation *copy = simulations->AppendDefault();
            simulation = &(*simulations)[idx]; // May have been reallocated

            InitializeSimulation(copy);
            copy->config = simulation->config;
        }
    }

    if (ImGui::CollapsingHeader("Results", ImGuiTreeNodeFlags_DefaultOpen) &&
            ImGui::BeginTabBar("ResultTabs")) {
        float population[6] = {};
        float time[6] = {};
        for (const Human &human: simulation->humans) {
            if (human.age >= 95) {
                population[5] += 1.0f;
                time[5] += human.age - 95;
            }
            if (human.age >= 85) {
                population[4] += 1.0f;
                time[4] += std::min(human.age, 95) - 85;
            }
            if (human.age >= 75) {
                population[3] += 1.0f;
                time[3] += std::min(human.age, 85) - 75;
            }
            if (human.age >= 65) {
                population[2] += 1.0f;
                time[2] += std::min(human.age, 75) - 65;
            }
            if (human.age >= 55) {
                population[1] += 1.0f;
                time[1] += std::min(human.age, 65) - 55;
            }
            if (human.age >= 45) {
                population[0] += 1.0f;
                time[0] += std::min(human.age, 55) - 45;
            }
        }

        if (ImGui::BeginTabItem("Deaths")) {
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

            RenderAgeTableHeaders();
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

            // It's actually off by one, because 0 is 'All'
            static int histogram_type = 0;
            ImGui::Combo("Type", &histogram_type, [](void *, int idx, const char **str) {
                *str = idx ? DeathTypeNames[idx - 1] : "All";
                return true;
            }, nullptr, ARRAY_SIZE(DeathTypeNames) + 1);
            ImGui::PlotHistogram("Histogram", deaths[histogram_type], ARRAY_SIZE(deaths[histogram_type]), 0,
                                 nullptr, 0.0f, (float)simulation->humans.len, ImVec2(0, 80));

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Risk factors")) {
            float smokers[6] = {};
            for (const Human &human: simulation->humans) {
                if (human.smoking_start_age) {
                    int age = human.smoking_cessation_age ? human.smoking_cessation_age : human.age;

                    if (age >= 95) smokers[5] += 1.0f;
                    if (age >= 85) smokers[4] += 1.0f;
                    if (age >= 75) smokers[3] += 1.0f;
                    if (age >= 65) smokers[2] += 1.0f;
                    if (age >= 55) smokers[1] += 1.0f;
                    if (age >= 45) smokers[0] += 1.0f;
                }
            }

            RenderAgeTableHeaders();
            ImGui::Text("Smokers"); ImGui::NextColumn();
            for (Size i = 0; i < ARRAY_SIZE(smokers); i++) {
                if (population[i]) {
                    float proportion = (smokers[i] / population[i]) * 100.0f;
                    const char *str = Fmt(&frame_alloc, "%1 (%2%%)",
                                          smokers[i], FmtDouble(proportion, 1)).ptr;
                    ImGui::TextUnformatted(str); ImGui::NextColumn();
                } else {
                    ImGui::Text("-"); ImGui::NextColumn();
                }
            }
            ImGui::Columns(1);
            ImGui::Separator();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Diseases")) {
            static const char *DiseaseNames[] = {
                "CardiacIschemia",
                "Stroke",
                "LungCancer"
            };

#define COMPUTE_DISEASE_AGE_PREVALENCE(DiseaseIdx, VarName) \
                do { \
                    if (human.VarName) { \
                        if (human.age >= 95) prevalences[(DiseaseIdx)][5] += 1.0f; \
                        if (human.VarName < 95 && human.age >= 85) prevalences[(DiseaseIdx)][4] += 1.0f; \
                        if (human.VarName < 85 && human.age >= 75) prevalences[(DiseaseIdx)][3] += 1.0f; \
                        if (human.VarName < 75 && human.age >= 65) prevalences[(DiseaseIdx)][2] += 1.0f; \
                        if (human.VarName < 65 && human.age >= 55) prevalences[(DiseaseIdx)][1] += 1.0f; \
                        if (human.VarName < 55) prevalences[(DiseaseIdx)][0] += 1.0f; \
                    } \
                } while (false)
#define COMPUTE_DISEASE_AGE_INCIDENCE(DiseaseIdx, VarName) \
                do { \
                    if (human.VarName) { \
                        if (human.VarName >= 95) incidences[(DiseaseIdx)][5] += 1.0f; \
                        else if (human.VarName >= 85) incidences[(DiseaseIdx)][4] += 1.0f; \
                        else if (human.VarName >= 75) incidences[(DiseaseIdx)][3] += 1.0f; \
                        else if (human.VarName >= 65) incidences[(DiseaseIdx)][2] += 1.0f; \
                        else if (human.VarName >= 55) incidences[(DiseaseIdx)][1] += 1.0f; \
                        else incidences[(DiseaseIdx)][0] += 1.0f; \
                    } \
                } while (false)

            float prevalences[3][6] = {};
            float incidences[3][6] = {};
            for (const Human &human: simulation->humans) {
                COMPUTE_DISEASE_AGE_PREVALENCE(0, cardiac_ischemia_age);
                COMPUTE_DISEASE_AGE_PREVALENCE(1, stroke_age);
                COMPUTE_DISEASE_AGE_PREVALENCE(2, lung_cancer_age);

                COMPUTE_DISEASE_AGE_INCIDENCE(0, cardiac_ischemia_age);
                COMPUTE_DISEASE_AGE_INCIDENCE(1, stroke_age);
                COMPUTE_DISEASE_AGE_INCIDENCE(2, lung_cancer_age);
            }

#undef COMPUTE_DISEASE_AGE_INCIDENCE
#undef COMPUTE_DISEASE_AGE_PREVALENCE

            ImGui::Text("Prevalence");
            RenderAgeTableHeaders();
            for (Size i = 0; i < ARRAY_SIZE(DiseaseNames); i++) {
                ImGui::TextUnformatted(DiseaseNames[i]); ImGui::NextColumn();
                for (Size j = 0; j < 6; j++) {
                    if (population[j]) {
                        float proportion = (prevalences[i][j] / population[j]) * 100.0f;
                        const char *str = Fmt(&frame_alloc, "%1 (%2%%)",
                                              prevalences[i][j], FmtDouble(proportion, 1)).ptr;
                        ImGui::TextUnformatted(str); ImGui::NextColumn();
                    } else {
                        ImGui::Text("-"); ImGui::NextColumn();
                    }
                }
            }
            ImGui::Columns(1);
            ImGui::Separator();

            ImGui::Text("Incidence");
            RenderAgeTableHeaders();
            for (Size i = 0; i < ARRAY_SIZE(DiseaseNames); i++) {
                ImGui::TextUnformatted(DiseaseNames[i]); ImGui::NextColumn();
                for (Size j = 0; j < 6; j++) {
                    if (time[j]) {
                        float proportion = (incidences[i][j] / time[j]) * 100000.0f;
                        const char *str = Fmt(&frame_alloc, "%1 (%2)",
                                              incidences[i][j], FmtDouble(proportion, 1)).ptr;
                        ImGui::TextUnformatted(str); ImGui::NextColumn();
                    } else {
                        ImGui::Text("-"); ImGui::NextColumn();
                    }
                }
            }
            ImGui::Columns(1);
            ImGui::Separator();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Economics")) {
            double utility = 0.0;
            double cost = 0.0;
            for (const Human &human: simulation->humans) {
                utility += human.utility;
                cost += human.cost;
            }

            ImGui::TextUnformatted(Fmt(&frame_alloc, "QALY: %1", FmtDouble(utility, 1)).ptr);
            ImGui::TextUnformatted(Fmt(&frame_alloc, "Cost: %1", FmtDouble(cost, 1)).ptr);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    return open;
}
