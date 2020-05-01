// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

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
    C,
    V,
    X,
    Y,
    Z
};

enum class gui_InputButton {
    Left,
    Right,
    Middle
};

struct gui_Info {
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

    gui_Info priv = {};

#ifdef _WIN32
    struct gui_Win32Window *window = nullptr;
#else
    GLFWwindow *window = nullptr;
    unsigned int released_buttons = 0;
#endif

    bool imgui_local = false;
    static bool imgui_ready; // = false

public:
    const gui_Info &info = priv;

    gui_Window() = default;
    ~gui_Window() { Release(); }

    bool Init(const char *application_name);
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
