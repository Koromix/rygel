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

#include "../../core/libcc/libcc.hh"
#include "../libheimdall/libheimdall.hh"
#include "../../../vendor/imgui/imgui.h"

namespace RG {

int Main(int, char **)
{
    InterfaceState render_state = {};
    HeapArray<ConceptSet> concept_sets;
    EntitySet entity_set = {};

    ImFontAtlas font_atlas;
    {
        const AssetInfo *font = FindPackedAsset("Roboto-Medium.ttf");
        RG_ASSERT(font);
        RG_ASSERT(font->data.len <= INT_MAX);

        ImFontConfig font_config;
        font_config.FontDataOwnedByAtlas = false;

        font_atlas.AddFontFromMemoryTTF((void *)font->data.ptr, (int)font->data.len,
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
int main(int argc, char **argv) { return RG::Main(argc, argv); }
