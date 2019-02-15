// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <emscripten.h>
#include <emscripten/html5.h>

#include "../libcc/libcc.hh"
#include "../libcc/opengl.hh"
#include "../libheimdall/core.hh"
#include "libgui.hh"

THREAD_LOCAL gui_Interface *gui_api;

void ogl_SwapGLBuffers()
{
    // The browser does this automatically, we don't have control over it
}

struct RunContext {
    Bitset<256> keys;
    int wheel_y;

    std::function<bool()> *step_func;
    const EntitySet *entity_set;
    Span<const ConceptSet> concept_sets;
    bool *run_flag;
    std::mutex *lock;
};

bool gui_RunApplication(std::function<bool()> step_func, bool *run_flag, std::mutex *lock)
{
    DEFER_C(prev_api = gui_api) { gui_api = prev_api; };

    gui_Interface io = {};
    gui_api = &io;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl;
    {
        EmscriptenWebGLContextAttributes attr;
        emscripten_webgl_init_context_attributes(&attr);
        attr.enableExtensionsByDefault = true;
        attr.depth = 1;
        attr.stencil = 1;
        attr.antialias = 1;
        attr.majorVersion = 2;
        attr.minorVersion = 0;

        webgl = emscripten_webgl_create_context("canvas", &attr);
    }

    emscripten_webgl_make_context_current(webgl);
    if (!ogl_InitGLFunctions())
        return false;

    {
        EmscriptenFullscreenStrategy strat = {};
        strat.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
        strat.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
        emscripten_enter_soft_fullscreen("canvas", &strat);
    }

    RunContext ctx = {};
    ctx.step_func = &step_func;
    ctx.entity_set = &entity_set;
    ctx.concept_sets = concept_sets;
    ctx.run_flag = run_flag;
    ctx.lock = lock;

    // Activate mouse tracking, we'll use emscripten_get_mouse_status()
    emscripten_set_mousedown_callback("canvas", nullptr, 0,
                                      [](int, const EmscriptenMouseEvent *, void *) { return 1; });
    emscripten_set_mouseup_callback("canvas", nullptr, 0,
                                    [](int, const EmscriptenMouseEvent *, void *) { return 1; });
    emscripten_set_mousemove_callback("canvas", nullptr, 0,
                                      [](int, const EmscriptenMouseEvent *, void *) { return 1; });
    emscripten_set_wheel_callback("canvas", &ctx.wheel_y, 0,
                                  [](int, const EmscriptenWheelEvent *ev, void *udata) {
        int *wheel_y = (int *)udata;
        *wheel_y = ev->deltaY;
        return 1;
    });

    // Follow keyboard events
    emscripten_set_keydown_callback(0, &ctx.keys, 1,
                                    [](int, const EmscriptenKeyboardEvent *ev, void *udata) {
        Bitset<256> *keys = (Bitset<256> *)udata;
        keys->Set((int)gui_Interface::Key::Control, ev->ctrlKey);
        keys->Set((int)gui_Interface::Key::Shift, ev->shiftKey);
        keys->Set((int)gui_Interface::Key::Alt, ev->altKey);
        return 1;
    });
    emscripten_set_keyup_callback(0, &ctx.keys, 1,
                                    [](int, const EmscriptenKeyboardEvent *ev, void *udata) {
        Bitset<256> *keys = (Bitset<256> *)udata;
        keys->Set((int)gui_Interface::Key::Control, ev->ctrlKey);
        keys->Set((int)gui_Interface::Key::Shift, ev->shiftKey);
        keys->Set((int)gui_Interface::Key::Alt, ev->altKey);
        return 1;
    });

    io.main.run = true;
    emscripten_set_main_loop_arg([](void *udata) {
        RunContext *ctx = (RunContext *)udata;

        if (ctx->run_flag) {
            gui_api->main.run = *ctx->run_flag;
        }

        // Get current viewport size
        {
            double width = 0.0;
            double height = 0.0;
            emscripten_get_element_css_size("canvas", &width, &height);

            gui_api->display.width = (int)width;
            gui_api->display.height = (int)height;
        }

        // Reset relative inputs
        gui_api->input.text.Clear();
        gui_api->input.wheel_x = 0;
        gui_api->input.wheel_y = 0;

        // Handle input events
        {
            EmscriptenMouseEvent ev;
            emscripten_get_mouse_status(&ev);

            gui_api->input.x = ev.targetX;
            gui_api->input.y = ev.targetY;
            gui_api->input.buttons = 0;
            if (ev.buttons & 0x1) {
                gui_api->input.buttons |= MaskEnum(gui_Interface::Button::Left);
            }
            if (ev.buttons & 0x2) {
                gui_api->input.buttons |= MaskEnum(gui_Interface::Button::Middle);
            }
            if (ev.buttons & 0x4) {
                gui_api->input.buttons |= MaskEnum(gui_Interface::Button::Right);
            }
            gui_api->input.wheel_y = ctx->wheel_y;
            ctx->wheel_y = 0;

            gui_api->input.keys = ctx->keys;
        }

        // Append NUL byte to keyboard text
        if (!gui_api->input.text.Available()) {
            gui_api->input.text.len--;
        }
        gui_api->input.text.Append('\0');

        // Update monotonic clock
        {
            struct timespec ts = {};
            clock_gettime(CLOCK_MONOTONIC, &ts);

            double monotonic_time = (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
            gui_api->time.monotonic_delta = monotonic_time - gui_api->time.monotonic;
            gui_api->time.monotonic = monotonic_time;
        }

        // Run the real code
        if (ctx->lock) {
            std::lock_guard<std::mutex> locker(*ctx->lock);
            if (!(*ctx->step_func)())
                return; // TODO: Abort somehow
        } else {
            if (!(*ctx->step_func)())
                return;
        }

        gui_api->main.iteration_count++;
    }, &ctx, 0, 1);

    return true;
}
