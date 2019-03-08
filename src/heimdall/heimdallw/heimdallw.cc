// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "../libheimdall/libheimdall.hh"

int main(void)
{
    InterfaceState render_state = {};
    HeapArray<ConceptSet> concept_sets;
    EntitySet entity_set = {};

    gui_Window window;

    if (!window.Init(HEIMDALL_NAME))
        return 1;
    if (!window.InitImGui())
        return 1;

    for (;;) {
        if (!window.Prepare())
            return 0;
        if (!StepHeimdall(window, render_state, concept_sets, entity_set))
            return 0;
    }

    return 0;
}
