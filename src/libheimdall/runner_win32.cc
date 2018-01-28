// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../common/kutil.hh"
#include "../libheimdall/core.hh"
#include "../libheimdall/opengl.hh"
#include "runner.hh"

static GL_FUNCTION_PTR(HGLRC, wglCreateContextAttribsARB, HDC hDC, HGLRC hShareContext,
                       const int *attribList);
static GL_FUNCTION_PTR(BOOL, wglChoosePixelFormatARB, HDC hdc, const int *piAttribIList,
                       const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats,
                       UINT *nNumFormats);
static GL_FUNCTION_PTR(BOOL, wglSwapIntervalEXT, int interval);

#define WGL_DRAW_TO_WINDOW_ARB                    0x2001
#define WGL_ACCELERATION_ARB                      0x2003
#define WGL_SUPPORT_OPENGL_ARB                    0x2010
#define WGL_DOUBLE_BUFFER_ARB                     0x2011
#define WGL_FULL_ACCELERATION_ARB                 0x2027
#define WGL_CONTEXT_MAJOR_VERSION_ARB             0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB             0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

struct Win32Window {
    HWND hwnd;
    HDC hdc;
    HGLRC hgl;

    bool mouse_tracked = false;
};

static THREAD_LOCAL Win32Window *g_window;
THREAD_LOCAL RunIO *g_io;

static const char *GetWin32ErrorMessage(DWORD err)
{
    static char msg_buf[2048];

    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg_buf, SIZE(msg_buf), NULL)) {
        // FormatMessage adds newlines, remove them
        char *msg_end = msg_buf + strlen(msg_buf);
        while (msg_end > msg_buf && strchr("\r\n", msg_end[-1]))
            msg_end--;
        *msg_end = 0;
    } else {
        strcpy(msg_buf, "(unknown)");
    }

    return msg_buf;
}

static const char *GetWin32ErrorMessage()
{
    DWORD last_error = GetLastError();
    return GetWin32ErrorMessage(last_error);
}

static LRESULT __stdcall MainWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_SIZE: {
            g_io->display.width = (int)(lparam & 0xFFFF);
            g_io->display.height = (int)(lparam >> 16);
        } break;

        case WM_MOUSELEAVE: { g_window->mouse_tracked = false; } // fallthrough
        case WM_KILLFOCUS: {
            g_io->input.keys.Clear();
            g_io->input.buttons = 0;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
#define HANDLE_KEY(VkCode, Code) \
                case (VkCode): { g_io->input.keys.Set((Size)(Code), state); } break

            bool state = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            switch (wparam) {
                HANDLE_KEY(VK_CONTROL, RunIO::Key::Control);
                HANDLE_KEY(VK_MENU, RunIO::Key::Alt);
                HANDLE_KEY(VK_SHIFT, RunIO::Key::Shift);
                HANDLE_KEY(VK_TAB, RunIO::Key::Tab);
                HANDLE_KEY(VK_DELETE, RunIO::Key::Delete);
                HANDLE_KEY(VK_BACK, RunIO::Key::Backspace);
                HANDLE_KEY(VK_RETURN, RunIO::Key::Enter);
                HANDLE_KEY(VK_ESCAPE, RunIO::Key::Escape);
                HANDLE_KEY(VK_HOME, RunIO::Key::Home);
                HANDLE_KEY(VK_END, RunIO::Key::End);
                HANDLE_KEY(VK_PRIOR, RunIO::Key::PageUp);
                HANDLE_KEY(VK_NEXT, RunIO::Key::PageDown);
                HANDLE_KEY(VK_LEFT, RunIO::Key::Left);
                HANDLE_KEY(VK_RIGHT, RunIO::Key::Right);
                HANDLE_KEY(VK_UP, RunIO::Key::Up);
                HANDLE_KEY(VK_DOWN, RunIO::Key::Down);
                HANDLE_KEY('A', RunIO::Key::A);
                HANDLE_KEY('C', RunIO::Key::C);
                HANDLE_KEY('V', RunIO::Key::V);
                HANDLE_KEY('X', RunIO::Key::X);
                HANDLE_KEY('Y', RunIO::Key::Y);
                HANDLE_KEY('Z', RunIO::Key::Z);
            }

#undef HANDLE_KEY
        } break;
        case WM_CHAR: {
            uint16_t c = (uint16_t)wparam;

            // TODO: Deal with supplementary planes
            if (c < 0x80 && LIKELY(g_io->input.text.Available() >= 1)) {
                g_io->input.text.Append((char)c);
            } else if (c < 0x800 && LIKELY(g_io->input.text.Available() >= 2)) {
                g_io->input.text.Append((char)(0xC0 | (c >> 6)));
                g_io->input.text.Append((char)(0x80 | (c & 0x3F)));
            } else if (LIKELY(g_io->input.text.Available() >= 3)) {
                g_io->input.text.Append((char)(0xE0 | (c >> 12)));
                g_io->input.text.Append((char)(0x80 | ((c >> 6) & 0x3F)));
                g_io->input.text.Append((char)(0x80 | (c & 0x3F)));
            } else {
                LogError("Dropping text events (buffer full)");
            }
        } break;

        case WM_MOUSEMOVE: {
            g_io->input.x = (int16_t)(lparam & 0xFFFF);
            g_io->input.y = (int16_t)(lparam >> 16);

            if (!g_window->mouse_tracked) {
                TRACKMOUSEEVENT tme = { SIZE(tme) };
                tme.hwndTrack = g_window->hwnd;
                tme.dwFlags = TME_LEAVE;
                TrackMouseEvent(&tme);

                g_window->mouse_tracked = true;
            }
        } break;
        case WM_LBUTTONDOWN: { g_io->input.buttons |= MaskEnum(RunIO::Button::Left); } break;
        case WM_LBUTTONUP: { g_io->input.buttons &= ~MaskEnum(RunIO::Button::Left); } break;
        case WM_MBUTTONDOWN: { g_io->input.buttons |= MaskEnum(RunIO::Button::Middle); } break;
        case WM_MBUTTONUP: { g_io->input.buttons &= ~MaskEnum(RunIO::Button::Middle); } break;
        case WM_RBUTTONDOWN: { g_io->input.buttons |= MaskEnum(RunIO::Button::Right); } break;
        case WM_RBUTTONUP: { g_io->input.buttons &= ~MaskEnum(RunIO::Button::Right); } break;
        case WM_XBUTTONDOWN: {
            uint16_t button = (uint16_t)(2 + (wparam >> 16));
            g_io->input.buttons |= (unsigned int)(1 << button);
        } break;
        case WM_XBUTTONUP: {
            uint16_t button = (uint16_t)(2 + (wparam >> 16));
            g_io->input.buttons &= ~(unsigned int)(1 << button);
        } break;
        case WM_MOUSEWHEEL: {
            g_io->input.wheel_y += (int16_t)(wparam >> 16) / WHEEL_DELTA;
        } break;
        case WM_MOUSEHWHEEL: {
            g_io->input.wheel_x += (int16_t)(wparam >> 16) / WHEEL_DELTA;
        } break;

        case WM_CLOSE: {
            g_io->main.run = false;
            return 0;
        } break;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HWND CreateMainWindow()
{
    // Create Win32 main window class
    static const char *const main_cls_name = APPLICATION_NAME "_main";
    static ATOM main_cls_atom;
    if (!main_cls_atom) {
        WNDCLASSEX gl_cls = { SIZE(gl_cls) };
        gl_cls.hInstance = GetModuleHandle(nullptr);
        gl_cls.lpszClassName = main_cls_name;
        gl_cls.lpfnWndProc = MainWindowProc;
        gl_cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
        gl_cls.style = CS_OWNDC;

        main_cls_atom = RegisterClassEx(&gl_cls);
        if (!main_cls_atom) {
            LogError("Failed to register window class '%1': %2", main_cls_name,
                     GetWin32ErrorMessage());
            return nullptr;
        }

        atexit([]() {
            UnregisterClass(main_cls_name, GetModuleHandle(nullptr));
        });
    }

    // Create Win32 main window
    HWND main_wnd;
    {
        RECT rect = {};
        rect.right = 1024;
        rect.bottom = 768;
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);

        main_wnd = CreateWindowEx(0, main_cls_name, APPLICATION_TITLE, WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  rect.right - rect.left, rect.bottom - rect.top,
                                  nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!main_wnd) {
            LogError("Failed to create Win32 window: %s", GetWin32ErrorMessage());
            return nullptr;
        }

        ShowWindow(main_wnd, SW_SHOW);
    }
    DEFER_N(gl_wnd_guard) { DestroyWindow(main_wnd); };

    gl_wnd_guard.disable();
    return main_wnd;
}
static void DeleteMainWindow(HWND wnd)
{
    DestroyWindow(wnd);
}

static bool InitWGL()
{
    if (wglCreateContextAttribsARB)
        return true;

    // First, we need a dummy window handle to create OpenGL context (...). I know
    // it is ugly, but not my fault.

    static const char *const dummy_cls_name = APPLICATION_NAME "_init_gl";
    {
        WNDCLASSEX dummy_cls = { SIZE(dummy_cls) };
        dummy_cls.hInstance = GetModuleHandle(nullptr);
        dummy_cls.lpszClassName = dummy_cls_name;
        dummy_cls.lpfnWndProc = DefWindowProc;
        if (!RegisterClassEx(&dummy_cls)) {
            LogError("Failed to register window class '%1': %2", dummy_cls_name,
                     GetWin32ErrorMessage());
            return false;
        }
    }
    DEFER { UnregisterClass(dummy_cls_name, GetModuleHandle(nullptr)); };

    HWND dummy_wnd;
    HDC dummy_dc;
    {
        dummy_wnd = CreateWindowEx(0, dummy_cls_name, dummy_cls_name, 0, 0, 0, 0, 0,
                                   nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        dummy_dc = GetDC(dummy_wnd);
        if (!dummy_wnd || !dummy_dc) {
            LogError("Failed to create dummy window for OpenGL context: %1",
                     GetWin32ErrorMessage());
            return false;
        }
    }
    DEFER { DestroyWindow(dummy_wnd); };

    {
        PIXELFORMATDESCRIPTOR pfd = { SIZE(PIXELFORMATDESCRIPTOR) };
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        int suggested_pixel_fmt = ChoosePixelFormat(dummy_dc, &pfd);
        if (!SetPixelFormat(dummy_dc, suggested_pixel_fmt, &pfd)) {
            LogError("Failed to set pixel format for dummy window: %1", GetWin32ErrorMessage());
            return false;
        }
    }

    HGLRC dummy_ctx = wglCreateContext(dummy_dc);
    if (!dummy_ctx) {
        LogError("Failed to create OpenGL context for dummy window: %1", GetWin32ErrorMessage());
        return false;
    }
    DEFER { wglDeleteContext(dummy_ctx); };

    if (!wglMakeCurrent(dummy_dc, dummy_ctx)) {
        LogError("Failed to change OpenGL context of dummy window: %1", GetWin32ErrorMessage());
        return false;
    }
    DEFER { wglMakeCurrent(dummy_dc, nullptr); };

#define IMPORT_WGL_FUNCTION(Name) \
        do { \
            Name = (decltype(Name))GetGLProcAddress(#Name); \
            if (!Name) { \
                LogError("Required WGL function '%1' is not available",  #Name); \
                return false; \
            } \
        } while (false)

    IMPORT_WGL_FUNCTION(wglCreateContextAttribsARB);
    IMPORT_WGL_FUNCTION(wglChoosePixelFormatARB);
    IMPORT_WGL_FUNCTION(wglSwapIntervalEXT);

#undef IMPORT_WGL_FUNCTION

    return true;
}

void *GetGLProcAddress(const char *name)
{
    return (void *)wglGetProcAddress(name);
}

static HGLRC CreateGLContext(HDC dc)
{
    if (!InitWGL())
        return nullptr;

    // Find GL-compatible pixel format
    int pixel_fmt_index;
    {
        static const int pixel_fmt_attr[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            // WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            // WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
            0,
        };

        GLuint num_formats;
        if (!wglChoosePixelFormatARB(dc, pixel_fmt_attr, nullptr, 1,
                                     &pixel_fmt_index, &num_formats)) {
            LogError("Cannot find GL-compatible pixel format");
            return nullptr;
        }
    }

    // Set GL-compatible pixel format
    {
        PIXELFORMATDESCRIPTOR pixel_fmt_desc;
        DescribePixelFormat(dc, pixel_fmt_index, SIZE(pixel_fmt_desc), &pixel_fmt_desc);
        if (!SetPixelFormat(dc, pixel_fmt_index, &pixel_fmt_desc)) {
            LogError("Cannot set pixel format on GL window: %1", GetWin32ErrorMessage());
            return nullptr;
        }
    }

    // Create GL context with wanted OpenGL version
    HGLRC gl;
    {
        static const int gl_version[2] = { 3, 3 };
        static const int gl_attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, gl_version[0],
            WGL_CONTEXT_MINOR_VERSION_ARB, gl_version[1],
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };

        gl = wglCreateContextAttribsARB(dc, 0, gl_attribs);
        if (!gl) {
            switch (GetLastError()) {
                case 0xC0072095: {
                    LogError("OpenGL version %1.%2 is not supported on this system",
                             gl_version[0], gl_version[1]);
                } break;
                case 0xC0072096: {
                    LogError("Requested OpenGL profile is not supported on this system");
                } break;

                default: {
                    LogError("Failed to create OpenGL context");
                } break;
            }
            return nullptr;
        }
    }
    DEFER_N(gl_guard) { wglDeleteContext(gl); };

    gl_guard.disable();
    return gl;
}
static void DeleteGLContext(HGLRC gl)
{
    wglDeleteContext(gl);
}

static bool SetGLContext(HDC dc, HGLRC gl)
{
    if (!wglMakeCurrent(dc, gl))
        return false;

    if (gl) {
        // FIXME: Transiently disable V-sync for demo
        if (!wglSwapIntervalEXT(0)) {
            static bool vsync_error_warned;
            if (!vsync_error_warned) {
                LogError("Failed to enable V-sync, ignoring");
                vsync_error_warned = true;
            }
        }
    }

    return true;
}

void SwapGLBuffers()
{
    SwapBuffers(g_window->hdc);
}

bool Run(const EntitySet &entity_set, bool *run_flag, std::mutex *lock)
{
    Win32Window window = {};
    RunIO io = {};
    DEFER_C(prev_window = g_window, prev_io = g_io) {
        g_window = prev_window;
        g_io = prev_io;
    };
    g_window = &window;
    g_io = &io;

    window.hwnd = CreateMainWindow();
    if (!window.hwnd)
        return false;
    DEFER { DeleteMainWindow(window.hwnd); };
    window.hdc = GetDC(window.hwnd);
    window.hgl = CreateGLContext(window.hdc);
    if (!window.hgl)
        return false;
    DEFER { DeleteGLContext(window.hgl); };
    if (!SetGLContext(window.hdc, window.hgl))
        return false;

    InterfaceState render_state = {};

    io.main.run = true;
    while (io.main.run) {
        if (run_flag) {
            io.main.run = *run_flag;
        }

        // Reset relative inputs
        io.input.text.Clear();
        io.input.wheel_x = 0;
        io.input.wheel_y = 0;

        // Pump Win32 messages
        {
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    io.main.run = false;
                    break;
                }

                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        // Append NUL byte to keyboard text
        if (!io.input.text.Available()) {
            io.input.text.len--;
        }
        io.input.text.Append('\0');

        // Update monotonic clock
        {
            LARGE_INTEGER perf_freq, perf_counter;
            QueryPerformanceFrequency(&perf_freq);
            QueryPerformanceCounter(&perf_counter);

            double monotonic_time = (double)perf_counter.QuadPart / (double)perf_freq.QuadPart;
            io.time.monotonic_delta = monotonic_time - io.time.monotonic;
            io.time.monotonic = monotonic_time;
        }

        // Run the real code
        if (lock) {
            std::lock_guard<std::mutex> locker(*lock);
            if (!Step(render_state, entity_set))
                return false;
        } else {
            if (!Step(render_state, entity_set))
                return false;
        }

        io.main.iteration_count++;
    }

    return true;
}
