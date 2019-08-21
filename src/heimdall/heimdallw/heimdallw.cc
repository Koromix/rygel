// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/imgui/imgui.h"
#include "../../libcc/libcc.hh"
#include "../libheimdall/libheimdall.hh"

namespace RG {

extern "C" const AssetInfo *const pack_asset_Roboto_Medium_ttf;

int RunHeimdallW(int, char **)
{
    InterfaceState render_state = {};
    HeapArray<ConceptSet> concept_sets;
    EntitySet entity_set = {};

    ImFontAtlas font_atlas;
    {
        const AssetInfo &font = *pack_asset_Roboto_Medium_ttf;
        RG_ASSERT_DEBUG(font.data.len <= INT_MAX);

        ImFontConfig font_config;
        font_config.FontDataOwnedByAtlas = false;

        font_atlas.AddFontFromMemoryTTF((void *)font.data.ptr, (int)font.data.len,
                                        16, &font_config);
    }

    gui_Window window;
    if (!window.Init(RG_HEIMDALL_NAME))
        return 1;
    if (!window.InitImGui(&font_atlas))
        return 1;

    for (;;) {
        if (!window.ProcessEvents(render_state.idle))
            return 0;
        if (!StepHeimdall(window, render_state, concept_sets, entity_set))
            return 0;
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunHeimdallW(argc, argv); }
