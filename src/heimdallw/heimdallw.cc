// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libheimdall/libheimdall.hh"

int main(void)
{
    InterfaceState render_state = {};
    HeapArray<ConceptSet> concept_sets;
    EntitySet entity_set = {};

    return !RunGuiApp(HEIMDALL_NAME, [&]() {
        return Step(render_state, concept_sets, entity_set);
    });
}
