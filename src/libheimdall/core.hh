// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "data.hh"

#define APPLICATION_NAME "heimdall"
#define APPLICATION_TITLE "Heimdall"

struct InterfaceState {
    HashSet<Span<const char>> deploy_paths;

    float time_zoom = 1.0f;
    bool plot_measures = true;
    bool keep_deployed = false;

    bool size_cache_valid = false;
    HeapArray<float> lines_top;
    float total_width_unscaled;
    float total_height;

    Size scroll_to_idx = 0;
    float scroll_offset_y;
};

bool Step(InterfaceState &state, const EntitySet &entity_set);
