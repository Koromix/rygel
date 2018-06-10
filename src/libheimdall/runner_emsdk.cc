// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <emscripten.h>
#include <emscripten/html5.h>

#include "../common/kutil.hh"
#include "../libheimdall/core.hh"
#include "../libheimdall/opengl.hh"
#include "runner.hh"

THREAD_LOCAL RunIO *g_io;

void SwapGLBuffers()
{
    // The browser does this automatically, we don't have control over it
}

bool Run(const EntitySet &entity_set, Span<const ConceptSet> concept_sets,
         bool *run_flag, std::mutex *lock)
{
    DEFER_C(prev_io = g_io) { g_io = prev_io; };

    RunIO io = {};
    g_io = &io;

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

    {
        EmscriptenFullscreenStrategy strat = {};
        strat.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
        strat.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
        emscripten_enter_soft_fullscreen("canvas", &strat);
    }

    // Activate mouse tracking, we'll use emscripten_get_mouse_status()
    emscripten_set_mousedown_callback("canvas", nullptr, 0,
                                    [](int, const EmscriptenMouseEvent *, void *) { return 1; });
    emscripten_set_mouseup_callback("canvas", nullptr, 0,
                                    [](int, const EmscriptenMouseEvent *, void *) { return 1; });
    emscripten_set_mousemove_callback("canvas", nullptr, 0,
                                    [](int, const EmscriptenMouseEvent *, void *) { return 1; });

    struct RunContext {
        InterfaceState render_state;
        const EntitySet *entity_set;
        Span<const ConceptSet> concept_sets;
        bool *run_flag;
        std::mutex *lock;
    };

    RunContext ctx = {};
    ctx.entity_set = &entity_set;
    ctx.concept_sets = concept_sets;
    ctx.run_flag = run_flag;
    ctx.lock = lock;

    io.main.run = true;
    emscripten_set_main_loop_arg([](void *udata) {
        RunContext *ctx = (RunContext *)udata;

        if (ctx->run_flag) {
            g_io->main.run = *ctx->run_flag;
        }

        // Get current viewport size
        {
            double width = 0.0;
            double height = 0.0;
            emscripten_get_element_css_size("canvas", &width, &height);

            LogError("SIZE: %1x%2", width, height);

            g_io->display.width = (int)width;
            g_io->display.height = (int)height;
        }

        // Reset relative inputs
        g_io->input.text.Clear();
        g_io->input.wheel_x = 0;
        g_io->input.wheel_y = 0;

        // Handle mouse events
        {
            EmscriptenMouseEvent ev;
            emscripten_get_mouse_status(&ev);

            g_io->input.x = ev.targetX;
            g_io->input.y = ev.targetY;

            g_io->input.buttons = 0;
            if (ev.buttons & 0x1) {
                g_io->input.buttons |= MaskEnum(RunIO::Button::Left);
            }
            if (ev.buttons & 0x2) {
                g_io->input.buttons |= MaskEnum(RunIO::Button::Middle);
            }
            if (ev.buttons & 0x4) {
                g_io->input.buttons |= MaskEnum(RunIO::Button::Right);
            }

            LogError("%1x%2: %3", g_io->input.x, g_io->input.y, FmtHex(g_io->input.buttons));
        }

        // Append NUL byte to keyboard text
        if (!g_io->input.text.Available()) {
            g_io->input.text.len--;
        }
        g_io->input.text.Append('\0');

        // Update monotonic clock
        {
            struct timespec ts = {};
            clock_gettime(CLOCK_MONOTONIC, &ts);

            double monotonic_time = (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
            g_io->time.monotonic_delta = monotonic_time - g_io->time.monotonic;
            g_io->time.monotonic = monotonic_time;
        }

        // Run the real code
        if (ctx->lock) {
            std::lock_guard<std::mutex> locker(*ctx->lock);
            if (!Step(ctx->render_state, *ctx->entity_set, ctx->concept_sets))
                return; // TODO: Abort somehow
        } else {
            if (!Step(ctx->render_state, *ctx->entity_set, ctx->concept_sets))
                return;
        }

        g_io->main.iteration_count++;
    }, &ctx, 0, 1);

    return true;
}
