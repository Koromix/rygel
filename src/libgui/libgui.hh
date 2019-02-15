// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

struct gui_Interface {
    enum class Key {
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

    enum class Button: unsigned int {
        Left,
        Right,
        Middle
    };

    struct {
        bool run;
        int instance_count;
        int64_t iteration_count;
    } main;

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
    } input;
};

extern THREAD_LOCAL gui_Interface *gui_api;

void *gui_GetProcAddress(const char *name);
void gui_SwapBuffers();

bool gui_RunApplication(const char *application_name, std::function<bool()> step_func,
                        bool *run_flag = nullptr, std::mutex *lock = nullptr);
