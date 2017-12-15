#include "../../common/kutil.hh"
#include "core.hh"
#include "opengl.hh"
#include "../runner/runner.hh"
#include "render.hh"

#include "../../../lib/imgui/imgui.h"

bool Step()
{
    if (!StartRender())
        return false;

    ImGui::ShowTestWindow();

    Render();
    SwapGLBuffers();

    if (!sys_main->run) {
        ReleaseRender();
    }

    return true;
}
