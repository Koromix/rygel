// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/libcc/libcc.hh"

struct ImFontAtlas;
struct GLFWwindow;

namespace RG {

enum class gui_InputKey {
    Control,
    Alt,
    Shift,
    Tab,
    Delete,
    Backspace,
    Enter,
    Escape,
    Home,
    End,
    PageUp,
    PageDown,
    Left,
    Right,
    Up,
    Down,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z
};

enum class gui_InputButton {
    Left,
    Right,
    Middle
};

struct gui_State {
    struct {
        double monotonic;
        double monotonic_delta;
    } time;

    struct {
        int width, height;
    } display;

    struct {
        Bitset<256> keys;
        LocalArray<char, 256> text;

        bool mouseover;
        int x, y;
        unsigned int buttons;
        int wheel_x, wheel_y;

        double interaction_time;
    } input;
};

class gui_Window {
    RG_DELETE_COPY(gui_Window)

    gui_State priv = {};

#ifdef _WIN32
    struct gui_Win32Window *window = nullptr;
#else
    GLFWwindow *window = nullptr;
    unsigned int released_buttons = 0;
#endif

    bool imgui_local = false;
    static bool imgui_ready; // = false

public:
    const gui_State &state = priv;

    gui_Window() = default;
    ~gui_Window() { Release(); }

    bool Create(const char *application_name);
    bool InitImGui(ImFontAtlas *font_atlas = nullptr);
    void Release();

    bool ProcessEvents(bool wait = false);

    void RenderImGui();
    void SwapBuffers();

private:
    void StartImGuiFrame();
    void ReleaseImGui();
};

}
