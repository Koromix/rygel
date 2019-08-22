// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "data.hh"
#include "animation.hh"
#include "../../libgui/libgui.hh"

namespace RG {

#define RG_HEIMDALL_NAME "heimdall"

enum class InterpolationMode {
    Linear,
    LOCF,
    Spline,
    Disable
};
static const char *const InterpolationModeNames[] = {
    "Linear",
    "LOCF",
    "Spline",
    "Disable"
};

enum class TimeUnit {
    Unknown,
    Milliseconds,
    Seconds,
    Minutes,
    Hours,
    Days,
    Months,
    Years
};
static const char *const TimeUnitNames[] = {
    "Unknown",
    "Milliseconds",
    "Seconds",
    "Minutes",
    "Hours",
    "Days",
    "Months",
    "Year"
};

struct InterfaceSettings {
    bool dark_theme = false;
    float tree_width = 300.0f;
    bool plot_measures = true;
    float deployed_alpha = 0.05f;
    float plot_height = 50.0f;
    InterpolationMode interpolation = InterpolationMode::Linear;
    float grid_alpha = 0.04f;
    TimeUnit time_unit = TimeUnit::Unknown;
    bool natural_time = false;
};

struct InterfaceState {
    enum class HighlightMode {
        Never,
        Deployed,
        Always
    };

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

    Size render_idx = 0;
    float render_offset = 0.0f;
    bool autozoom = false;

    HighlightMode highlight_mode = HighlightMode::Deployed;
    Size scroll_to_idx = 0;
    float scroll_offset_y;
    Size highlight_idx = -1;

    bool grab_canvas = false;
    float grab_canvas_x;
    float grab_canvas_y;

    HashMap<Span<const char>, Span<const char>> select_concepts;
    HashSet<Span<const char>> align_concepts;
    char filter_text[256] = {};

    bool idle = false;
};

bool StepHeimdall(gui_Window &window, InterfaceState &state, HeapArray<ConceptSet> &concept_sets,
                  const EntitySet &entity_set);

}
