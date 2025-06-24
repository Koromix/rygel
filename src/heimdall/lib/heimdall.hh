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

#include "src/core/base/base.hh"
#include "data.hh"
#include "animation.hh"
#include "src/core/gui/gui.hh"

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
    float tree_width = 250.0f;
    bool plot_measures = true;
    float deployed_alpha = 0.05f;
    float plot_height = 50.0f;
    InterpolationMode interpolation = InterpolationMode::Linear;
    bool plot_labels = true;
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

    // XXX: Separate deploy_paths set for each concept set
    HashSet<Span<const char>> deploy_paths;

    AnimatedValue<double, double> time_zoom = NAN;
    double scroll_x = 0.0;
    double scroll_y = 0.0;
    double imgui_scroll_delta_x = 0.0;

    bool show_settings = false;
    InterfaceSettings settings;
    InterfaceSettings new_settings;

    int concept_set_idx = 0;
    const ConceptSet *prev_concept_set = nullptr;

    bool size_cache_valid = false;
    HeapArray<double> lines_top;
    double minimum_x_unscaled;
    double total_width_unscaled;
    double total_height;
    double imgui_height;
    Size visible_entities = 0;

    Size render_idx = 0;
    double render_offset = 0.0;
    bool autozoom = false;

    HighlightMode highlight_mode = HighlightMode::Always;
    Size scroll_to_idx = 0;
    double scroll_offset_y;
    Size highlight_idx = -1;

    bool grab_canvas = false;
    double grab_canvas_x;
    double grab_canvas_y;

    HashMap<Span<const char>, Span<const char>> select_concepts;
    HashSet<Span<const char>> align_concepts;
    char filter_text[256] = {};

    bool idle = false;
};

bool StepHeimdall(gui_Window &window, InterfaceState &state, HeapArray<ConceptSet> &concept_sets,
                  const EntitySet &entity_set);

}
