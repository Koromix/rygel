#pragma once

#include "../heimdall/kutil.hh"

struct MainInfo {
    bool run;
    int instance_count;
    int64_t iteration_count;

    double monotonic_time;
    double monotonic_delta;
};

struct DisplayInfo {
    int width, height;
};

struct MouseInfo {
    // Follows ImGui ordering
    enum Button: unsigned int {
        Left = 0,
        Right,
        Middle
    };

    int x, y;
    unsigned int buttons;
    int wheel_x, wheel_y;
};

extern const MainInfo *const sys_main;
extern const DisplayInfo *const sys_display;
extern const MouseInfo *const sys_mouse;

bool Run();
