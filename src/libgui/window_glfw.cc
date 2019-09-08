// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WIN32

#include "../../vendor/glfw/include/GLFW/glfw3.h"

#include "../libcc/libcc.hh"
#include "window.hh"

namespace RG {

static std::mutex init_mutex;
static Size windows_count = 0;

// Including wrappers/opengl.hh goes wrong (duplicate prototypes with GLFW stuff)
bool ogl_InitFunctions(void *(*get_proc_address)(const char *name));

static bool InitGLFW()
{
    std::lock_guard<std::mutex> lock(init_mutex);

    if (!windows_count) {
        if (!glfwInit()) {
            LogError("glfwInit() failed");
            return false;
        }

        glfwSetErrorCallback([](int code, const char* description) {
            LogError("GLFW: %1", description);
        });
    }
    windows_count++;

    return true;
}

static void TerminateGLFW()
{
    std::lock_guard<std::mutex> lock(init_mutex);

    if (!--windows_count) {
        glfwTerminate();
    }
}

bool gui_Window::Init(const char *application_name)
{
    if (!InitGLFW())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Create window
    window = glfwCreateWindow(1152, 648, application_name, nullptr, nullptr);
    if (!window) {
        LogError("glfwCreateWindow() failed");
        return false;
    }
    glfwSetWindowUserPointer(window, this);

    // Mouse callbacks
    glfwSetCursorPosCallback(window, [](GLFWwindow *window, double x, double y) {
        gui_Window *self = (gui_Window *)glfwGetWindowUserPointer(window);

        self->priv.input.x = (int)x;
        self->priv.input.y = (int)y;

        self->priv.input.interaction_time = self->priv.time.monotonic;
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int) {
        gui_Window *self = (gui_Window *)glfwGetWindowUserPointer(window);

        if (action == GLFW_PRESS) {
            self->priv.input.buttons |= 1u << button;
        } else {
            self->released_buttons |= 1u << button;
        }

        self->priv.input.interaction_time = self->priv.time.monotonic;
    });
    glfwSetScrollCallback(window, [](GLFWwindow *window, double xoffset, double yoffset) {
        gui_Window *self = (gui_Window *)glfwGetWindowUserPointer(window);

        self->priv.input.wheel_x = (int)xoffset;
        self->priv.input.wheel_y = (int)yoffset;

        self->priv.input.interaction_time = self->priv.time.monotonic;
    });

    // Keyboard callacks
    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int, int action, int) {
        gui_Window *self = (gui_Window *)glfwGetWindowUserPointer(window);

#define HANDLE_KEY(GlfwCode, Code) \
            case (GlfwCode): { self->priv.input.keys.Set((Size)(Code), action != GLFW_RELEASE); } break

        switch (key) {
            HANDLE_KEY(GLFW_KEY_LEFT_CONTROL, gui_InputKey::Control);
            HANDLE_KEY(GLFW_KEY_LEFT_ALT, gui_InputKey::Alt);
            HANDLE_KEY(GLFW_KEY_LEFT_SHIFT, gui_InputKey::Shift);
            HANDLE_KEY(GLFW_KEY_TAB, gui_InputKey::Tab);
            HANDLE_KEY(GLFW_KEY_DELETE, gui_InputKey::Delete);
            HANDLE_KEY(GLFW_KEY_BACKSPACE, gui_InputKey::Backspace);
            HANDLE_KEY(GLFW_KEY_ENTER, gui_InputKey::Enter);
            HANDLE_KEY(GLFW_KEY_ESCAPE, gui_InputKey::Escape);
            HANDLE_KEY(GLFW_KEY_HOME, gui_InputKey::Home);
            HANDLE_KEY(GLFW_KEY_END, gui_InputKey::End);
            HANDLE_KEY(GLFW_KEY_PAGE_UP, gui_InputKey::PageUp);
            HANDLE_KEY(GLFW_KEY_PAGE_DOWN, gui_InputKey::PageDown);
            HANDLE_KEY(GLFW_KEY_LEFT, gui_InputKey::Left);
            HANDLE_KEY(GLFW_KEY_RIGHT, gui_InputKey::Right);
            HANDLE_KEY(GLFW_KEY_UP, gui_InputKey::Up);
            HANDLE_KEY(GLFW_KEY_DOWN, gui_InputKey::Down);
            HANDLE_KEY(GLFW_KEY_A, gui_InputKey::A);
            HANDLE_KEY(GLFW_KEY_C, gui_InputKey::C);
            HANDLE_KEY(GLFW_KEY_V, gui_InputKey::V);
            HANDLE_KEY(GLFW_KEY_X, gui_InputKey::X);
            HANDLE_KEY(GLFW_KEY_Y, gui_InputKey::Y);
            HANDLE_KEY(GLFW_KEY_Z, gui_InputKey::Z);
        }

#undef HANDLE_KEY

        self->priv.input.interaction_time = self->priv.time.monotonic;
    });
    glfwSetCharCallback(window, [](GLFWwindow *window, unsigned int c) {
        gui_Window *self = (gui_Window *)glfwGetWindowUserPointer(window);

        // TODO: Deal with supplementary planes
        if (c < 0x80 && RG_LIKELY(self->priv.input.text.Available() >= 1)) {
            self->priv.input.text.Append((char)c);
        } else if (c < 0x800 && RG_LIKELY(self->priv.input.text.Available() >= 2)) {
            self->priv.input.text.Append((char)(0xC0 | (c >> 6)));
            self->priv.input.text.Append((char)(0x80 | (c & 0x3F)));
        } else if (RG_LIKELY(self->priv.input.text.Available() >= 3)) {
            self->priv.input.text.Append((char)(0xE0 | (c >> 12)));
            self->priv.input.text.Append((char)(0x80 | ((c >> 6) & 0x3F)));
            self->priv.input.text.Append((char)(0x80 | (c & 0x3F)));
        } else {
            LogError("Dropping text events (buffer full)");
        }

        self->priv.input.interaction_time = self->priv.time.monotonic;
    });

    // Set GL context
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!ogl_InitFunctions([](const char *name) { return (void *)glfwGetProcAddress(name); }))
        return false;

    return true;
}

void gui_Window::Release()
{
    if (imgui_local) {
        ReleaseImGui();
    }

    glfwDestroyWindow(window);
    TerminateGLFW();
}

void gui_Window::SwapBuffers()
{
    glfwSwapBuffers(window);
}

bool gui_Window::ProcessEvents(bool wait)
{
    // Update monotonic clock
    {
        double monotonic_time = glfwGetTime();

        priv.time.monotonic_delta = monotonic_time - priv.time.monotonic;
        priv.time.monotonic = monotonic_time;
    }

    // Reset relative inputs
    priv.input.text.Clear();
    priv.input.buttons &= ~released_buttons;
    released_buttons = 0;
    priv.input.wheel_x = 0;
    priv.input.wheel_y = 0;

    // Process GLFW events
    if (wait) {
        glfwWaitEvents();
    } else {
        glfwPollEvents();
    }
    if (glfwWindowShouldClose(window))
        return false;

    // Update window size and focus
    glfwGetFramebufferSize(window, &priv.display.width, &priv.display.height);
    priv.input.mouseover = glfwGetWindowAttrib(window, GLFW_HOVERED);

    // Append NUL byte to keyboard text
    if (!priv.input.text.Available()) {
        priv.input.text.len--;
    }
    priv.input.text.Append('\0');

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (imgui_local) {
        StartImGuiFrame();
    }

    return true;
}

}

#endif
