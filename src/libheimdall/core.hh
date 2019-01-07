// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/util.hh"
#include "data.hh"
#include "animation.hh"

#define HEIMDALL_NAME "heimdall"

enum class InterpolationMode {
    Linear,
    LOCF,
    Spline,
    Disable
};
static const char *const interpolation_mode_names[] = {
    "Linear",
    "LOCF",
    "Spline",
    "Disable"
};

struct InterfaceSettings {
    enum class HighlightMode {
        Never,
        Deployed,
        Always
    };

    bool light_theme = false;
    float tree_width = 300.0f;
    bool plot_measures = true;
    float deployed_alpha = 0.05f;
    float plot_height = 50.0f;
    InterpolationMode interpolation = InterpolationMode::Linear;
    float grid_alpha = 0.04f;
    HighlightMode highlight_mode = HighlightMode::Deployed;
};

struct InterfaceState {
    // TODO: Separate deploy_paths set for each concept set
    HashSet<Span<const char>> deploy_paths;

    AnimatedValue<float, double> time_zoom = NAN;
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    float imgui_scroll_delta_x = 0.0f;

    bool show_settings = false;
    InterfaceSettings settings;
    InterfaceSettings new_settings;

    int concept_set_idx = 0;
    const ConceptSet *prev_concept_set = nullptr;

    bool size_cache_valid = false;
    HeapArray<float> lines_top;
    float minimum_x_unscaled;
    float total_width_unscaled;
    float total_height;
    Size visible_entities = 0;

    Size scroll_to_idx = 0;
    float scroll_offset_y;
    Size highlight_idx = -1;

    bool grab_canvas = false;
    float grab_canvas_x;
    float grab_canvas_y;

    HashSet<Span<const char>> select_concepts;
    HashSet<Span<const char>> align_concepts;
};

bool Step(InterfaceState &state, HeapArray<ConceptSet> &concept_sets, const EntitySet &entity_set);
