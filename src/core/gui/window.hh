// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "src/core/base/base.hh"

struct ImFontAtlas;
struct GLFWwindow;

namespace K {

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
    Z,
    Key0,
    Key1,
    Key2,
    Key3,
    Key4,
    Key5,
    Key6,
    Key7,
    Key8,
    Key9
};

enum class gui_InputButton {
    Left,
    Right,
    Middle
};

struct gui_KeyEvent {
    uint8_t key;
    bool down;
};

struct gui_State {
    struct {
        double monotonic;
        double monotonic_delta;
    } time;

    struct {
        int width;
        int height;
    } display;

    struct {
        LocalArray<gui_KeyEvent, 64> events;
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
    K_DELETE_COPY(gui_Window)

    gui_State priv = {};

#if defined(_WIN32)
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
