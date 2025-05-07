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

#if !defined(_WIN32)

#include "src/core/base/base.hh"
#include "window.hh"
#include "vendor/glfw/include/GLFW/glfw3.h"

namespace RG {

static std::mutex init_mutex;
static Size windows_count = 0;

// Including libwrap/opengl.hh goes wrong (duplicate prototypes with GLFW stuff)
bool ogl_InitFunctions(void *(*get_proc_address)(const char *name));

static bool InitGLFW()
{
    std::lock_guard<std::mutex> lock(init_mutex);

    if (!windows_count) {
        if (!glfwInit()) {
            LogError("glfwInit() failed");
            return false;
        }

        glfwSetErrorCallback([](int, const char* description) {
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

bool gui_Window::Create(const char *application_name)
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
            case (GlfwCode): { \
                bool down = (action != GLFW_RELEASE); \
                if (self->priv.input.events.Available()) [[likely]] { \
                    gui_KeyEvent evt = { (uint8_t)(Code), down }; \
                    self->priv.input.events.Append(evt); \
                } \
                self->priv.input.keys.Set((Size)(Code), down); \
            } break

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
            HANDLE_KEY(GLFW_KEY_B, gui_InputKey::B);
            HANDLE_KEY(GLFW_KEY_C, gui_InputKey::C);
            HANDLE_KEY(GLFW_KEY_D, gui_InputKey::D);
            HANDLE_KEY(GLFW_KEY_E, gui_InputKey::E);
            HANDLE_KEY(GLFW_KEY_F, gui_InputKey::F);
            HANDLE_KEY(GLFW_KEY_G, gui_InputKey::G);
            HANDLE_KEY(GLFW_KEY_H, gui_InputKey::H);
            HANDLE_KEY(GLFW_KEY_I, gui_InputKey::I);
            HANDLE_KEY(GLFW_KEY_J, gui_InputKey::J);
            HANDLE_KEY(GLFW_KEY_K, gui_InputKey::K);
            HANDLE_KEY(GLFW_KEY_L, gui_InputKey::L);
            HANDLE_KEY(GLFW_KEY_M, gui_InputKey::M);
            HANDLE_KEY(GLFW_KEY_N, gui_InputKey::N);
            HANDLE_KEY(GLFW_KEY_O, gui_InputKey::O);
            HANDLE_KEY(GLFW_KEY_P, gui_InputKey::P);
            HANDLE_KEY(GLFW_KEY_Q, gui_InputKey::Q);
            HANDLE_KEY(GLFW_KEY_R, gui_InputKey::R);
            HANDLE_KEY(GLFW_KEY_S, gui_InputKey::S);
            HANDLE_KEY(GLFW_KEY_T, gui_InputKey::T);
            HANDLE_KEY(GLFW_KEY_U, gui_InputKey::U);
            HANDLE_KEY(GLFW_KEY_V, gui_InputKey::V);
            HANDLE_KEY(GLFW_KEY_W, gui_InputKey::W);
            HANDLE_KEY(GLFW_KEY_X, gui_InputKey::X);
            HANDLE_KEY(GLFW_KEY_Y, gui_InputKey::Y);
            HANDLE_KEY(GLFW_KEY_Z, gui_InputKey::Z);
            HANDLE_KEY(GLFW_KEY_0, gui_InputKey::Key0);
            HANDLE_KEY(GLFW_KEY_1, gui_InputKey::Key1);
            HANDLE_KEY(GLFW_KEY_2, gui_InputKey::Key2);
            HANDLE_KEY(GLFW_KEY_3, gui_InputKey::Key3);
            HANDLE_KEY(GLFW_KEY_4, gui_InputKey::Key4);
            HANDLE_KEY(GLFW_KEY_5, gui_InputKey::Key5);
            HANDLE_KEY(GLFW_KEY_6, gui_InputKey::Key6);
            HANDLE_KEY(GLFW_KEY_7, gui_InputKey::Key7);
            HANDLE_KEY(GLFW_KEY_8, gui_InputKey::Key8);
            HANDLE_KEY(GLFW_KEY_9, gui_InputKey::Key9);
        }

#undef HANDLE_KEY

        self->priv.input.interaction_time = self->priv.time.monotonic;
    });
    glfwSetCharCallback(window, [](GLFWwindow *window, unsigned int c) {
        gui_Window *self = (gui_Window *)glfwGetWindowUserPointer(window);

        if (self->priv.input.text.Available() >= 5) [[likely]] {
            self->priv.input.text.len += EncodeUtf8(c, self->priv.input.text.end());
            self->priv.input.text.data[self->priv.input.text.len] = 0;
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
    priv.input.events.Clear();
    priv.input.text.Clear();
    priv.input.text.data[priv.input.text.len] = 0;
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

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (imgui_local) {
        StartImGuiFrame();
    }

    return true;
}

}

#endif
