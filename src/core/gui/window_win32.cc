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

#if defined(_WIN32)

#include "src/core/base/base.hh"
#include "window.hh"
#include "src/core/wrap/opengl.hh"

#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RG {

static OGL_FUNCTION_PTR(HGLRC, wglCreateContextAttribsARB, HDC hDC, HGLRC hShareContext,
                        const int *attribList);
static OGL_FUNCTION_PTR(BOOL, wglChoosePixelFormatARB, HDC hdc, const int *piAttribIList,
                        const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats,
                        UINT *nNumFormats);
static OGL_FUNCTION_PTR(BOOL, wglSwapIntervalEXT, int interval);

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

struct gui_Win32Window {
    HWND hwnd;
    HDC hdc;
    HGLRC hgl;

    // Apply mouse up events next frame, or some clicks will fail (such as touchpads)
    // because the DOWN and UP events will be detected in the same frame.
    unsigned int released_buttons;

    uint32_t surrogate_buf;
};

static thread_local gui_State *thread_info;
static thread_local gui_Win32Window *thread_window;

static LRESULT __stdcall MainWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_SIZE: {
            thread_info->display.width = (int)(lparam & 0xFFFF);
            thread_info->display.height = (int)(lparam >> 16);
        } break;

        case WM_MOUSELEAVE: { thread_info->input.mouseover = false; } [[fallthrough]];
        case WM_KILLFOCUS: {
            thread_info->input.events.Clear();
            thread_info->input.keys.Clear();
            thread_info->input.buttons = 0;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
#define HANDLE_KEY(VkCode, Code) \
                case (VkCode): { \
                    if (thread_info->input.events.Available()) [[likely]] { \
                        gui_KeyEvent evt = { (uint8_t)(Code), down }; \
                        thread_info->input.events.Append(evt); \
                    } \
                    thread_info->input.keys.Set((Size)(Code), down); \
                } break

            bool down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);

            switch (wparam) {
                HANDLE_KEY(VK_CONTROL, gui_InputKey::Control);
                HANDLE_KEY(VK_MENU, gui_InputKey::Alt);
                HANDLE_KEY(VK_SHIFT, gui_InputKey::Shift);
                HANDLE_KEY(VK_TAB, gui_InputKey::Tab);
                HANDLE_KEY(VK_DELETE, gui_InputKey::Delete);
                HANDLE_KEY(VK_BACK, gui_InputKey::Backspace);
                HANDLE_KEY(VK_RETURN, gui_InputKey::Enter);
                HANDLE_KEY(VK_ESCAPE, gui_InputKey::Escape);
                HANDLE_KEY(VK_HOME, gui_InputKey::Home);
                HANDLE_KEY(VK_END, gui_InputKey::End);
                HANDLE_KEY(VK_PRIOR, gui_InputKey::PageUp);
                HANDLE_KEY(VK_NEXT, gui_InputKey::PageDown);
                HANDLE_KEY(VK_LEFT, gui_InputKey::Left);
                HANDLE_KEY(VK_RIGHT, gui_InputKey::Right);
                HANDLE_KEY(VK_UP, gui_InputKey::Up);
                HANDLE_KEY(VK_DOWN, gui_InputKey::Down);
                HANDLE_KEY('A', gui_InputKey::A);
                HANDLE_KEY('B', gui_InputKey::B);
                HANDLE_KEY('C', gui_InputKey::C);
                HANDLE_KEY('D', gui_InputKey::D);
                HANDLE_KEY('E', gui_InputKey::E);
                HANDLE_KEY('F', gui_InputKey::F);
                HANDLE_KEY('G', gui_InputKey::G);
                HANDLE_KEY('H', gui_InputKey::H);
                HANDLE_KEY('I', gui_InputKey::I);
                HANDLE_KEY('J', gui_InputKey::J);
                HANDLE_KEY('K', gui_InputKey::K);
                HANDLE_KEY('L', gui_InputKey::L);
                HANDLE_KEY('M', gui_InputKey::M);
                HANDLE_KEY('N', gui_InputKey::N);
                HANDLE_KEY('O', gui_InputKey::O);
                HANDLE_KEY('P', gui_InputKey::P);
                HANDLE_KEY('Q', gui_InputKey::Q);
                HANDLE_KEY('R', gui_InputKey::R);
                HANDLE_KEY('S', gui_InputKey::S);
                HANDLE_KEY('T', gui_InputKey::T);
                HANDLE_KEY('U', gui_InputKey::U);
                HANDLE_KEY('V', gui_InputKey::V);
                HANDLE_KEY('W', gui_InputKey::W);
                HANDLE_KEY('X', gui_InputKey::X);
                HANDLE_KEY('Y', gui_InputKey::Y);
                HANDLE_KEY('Z', gui_InputKey::Z);
                HANDLE_KEY('0', gui_InputKey::Key0);
                HANDLE_KEY('1', gui_InputKey::Key1);
                HANDLE_KEY('2', gui_InputKey::Key2);
                HANDLE_KEY('3', gui_InputKey::Key3);
                HANDLE_KEY('4', gui_InputKey::Key4);
                HANDLE_KEY('5', gui_InputKey::Key5);
                HANDLE_KEY('6', gui_InputKey::Key6);
                HANDLE_KEY('7', gui_InputKey::Key7);
                HANDLE_KEY('8', gui_InputKey::Key8);
                HANDLE_KEY('9', gui_InputKey::Key9);
            }

#undef HANDLE_KEY
        } break;
        case WM_CHAR: {
            uint32_t uc = (uint32_t)wparam;

            if ((uc - 0xD800u) < 0x800u) {
                if ((uc & 0xFC00) == 0xD800) {
                    thread_window->surrogate_buf = uc;
                    break;
                } else if (thread_window->surrogate_buf && (uc & 0xFC00) == 0xDC00) {
                    uc = (thread_window->surrogate_buf << 10) + uc - 0x35FDC00;
                    thread_window->surrogate_buf = 0;
                } else {
                    // Yeah something is up. Give up on this character.
                    thread_window->surrogate_buf = 0;
                    break;
                }
            }

            if (thread_info->input.text.Available() >= 5) [[likely]] {
                thread_info->input.text.len += EncodeUtf8(uc, thread_info->input.text.end());
                thread_info->input.text.data[thread_info->input.text.len] = 0;
            } else {
                LogError("Dropping text events (buffer full)");
            }
        } break;

        case WM_MOUSEMOVE: {
            thread_info->input.x = (int16_t)(lparam & 0xFFFF);
            thread_info->input.y = (int16_t)(lparam >> 16);

            if (!thread_info->input.mouseover) {
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = RG_SIZE(tme);
                tme.hwndTrack = thread_window->hwnd;
                tme.dwFlags = TME_LEAVE;
                TrackMouseEvent(&tme);

                thread_info->input.mouseover = true;
            }
        } break;
        case WM_LBUTTONDOWN: {
            thread_info->input.buttons |= MaskEnum(gui_InputButton::Left);
            SetCapture(thread_window->hwnd);
        } break;
        case WM_LBUTTONUP: {
            thread_window->released_buttons |= MaskEnum(gui_InputButton::Left);
            ReleaseCapture();
        } break;
        case WM_MBUTTONDOWN: { thread_info->input.buttons |= MaskEnum(gui_InputButton::Middle); } break;
        case WM_MBUTTONUP: { thread_window->released_buttons |= MaskEnum(gui_InputButton::Middle); } break;
        case WM_RBUTTONDOWN: { thread_info->input.buttons |= MaskEnum(gui_InputButton::Right); } break;
        case WM_RBUTTONUP: { thread_window->released_buttons |= MaskEnum(gui_InputButton::Right); } break;
        case WM_XBUTTONDOWN: {
            uint16_t button = (uint16_t)(2 + (wparam >> 16));
            thread_info->input.buttons |= (unsigned int)(1 << button);
        } break;
        case WM_XBUTTONUP: {
            uint16_t button = (uint16_t)(2 + (wparam >> 16));
            thread_window->released_buttons |= (unsigned int)(1 << button);
        } break;
        case WM_MOUSEWHEEL: {
            thread_info->input.wheel_y += (int16_t)(wparam >> 16) / WHEEL_DELTA;
        } break;
        case WM_MOUSEHWHEEL: {
            thread_info->input.wheel_x += (int16_t)(wparam >> 16) / WHEEL_DELTA;
        } break;

        case WM_CLOSE: {
            PostQuitMessage(0);
            return 0;
        } break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static HWND CreateMainWindow(const char *application_name)
{
    static wchar_t application_name_w[256];
    static ATOM main_cls_atom;

    // Create Win32 main window class
    if (!main_cls_atom) {
        if (ConvertUtf8ToWin32Wide(application_name, application_name_w) < -1)
            return nullptr;

        WNDCLASSEXW gl_cls = {};
        gl_cls.cbSize = RG_SIZE(gl_cls);
        gl_cls.hInstance = GetModuleHandle(nullptr);
        gl_cls.lpszClassName = application_name_w;
        gl_cls.lpfnWndProc = MainWindowProc;
        gl_cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
        gl_cls.style = CS_OWNDC;

        main_cls_atom = RegisterClassExW(&gl_cls);
        if (!main_cls_atom) {
            LogError("Failed to register window class '%1': %2", application_name,
                     GetWin32ErrorString());
            return nullptr;
        }

        atexit([]() {
            UnregisterClassW(application_name_w, GetModuleHandle(nullptr));
        });
    }

    // Create Win32 main window
    HWND main_wnd;
    {
        RECT rect = {};
        rect.right = 1152;
        rect.bottom = 648;
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);

        main_wnd = CreateWindowExW(0, application_name_w, application_name_w, WS_OVERLAPPEDWINDOW,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   rect.right - rect.left, rect.bottom - rect.top,
                                   nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!main_wnd) {
            LogError("Failed to create Win32 window: %s", GetWin32ErrorString());
            return nullptr;
        }

        ShowWindow(main_wnd, SW_SHOW);
    }
    RG_DEFER_N(gl_wnd_guard) { DestroyWindow(main_wnd); };

    gl_wnd_guard.Disable();
    return main_wnd;
}
static void DeleteMainWindow(HWND wnd)
{
    DestroyWindow(wnd);
}

static bool InitWGL(const char *application_name)
{
    if (wglCreateContextAttribsARB)
        return true;

    // First, we need a dummy window handle to create OpenGL context (...).
    // I know it is ugly, but not my fault.
    static wchar_t dummy_cls_name_w[256];
    static ATOM dummy_cls_atom;
    static char dummy_cls_name[256];

    // Register it
    if (!dummy_cls_atom) {
        Fmt(dummy_cls_name, "%1_init_gl", application_name);
        if (ConvertUtf8ToWin32Wide(dummy_cls_name, dummy_cls_name_w) < 0)
            return false;

        WNDCLASSEXW dummy_cls = {};
        dummy_cls.cbSize = RG_SIZE(dummy_cls);
        dummy_cls.hInstance = GetModuleHandle(nullptr);
        dummy_cls.lpszClassName = dummy_cls_name_w;
        dummy_cls.lpfnWndProc = DefWindowProcW;

        dummy_cls_atom = RegisterClassExW(&dummy_cls);
        if (!dummy_cls_atom) {
            LogError("Failed to register window class '%1': %2", dummy_cls_name,
                     GetWin32ErrorString());
            return false;
        }
    }
    RG_DEFER { UnregisterClassW(dummy_cls_name_w, GetModuleHandle(nullptr)); };

    HWND dummy_wnd;
    HDC dummy_dc;
    {
        dummy_wnd = CreateWindowExW(0, dummy_cls_name_w, dummy_cls_name_w, 0, 0, 0, 0, 0,
                                    nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        dummy_dc = GetDC(dummy_wnd);
        if (!dummy_wnd || !dummy_dc) {
            LogError("Failed to create dummy window for OpenGL context: %1",
                     GetWin32ErrorString());
            return false;
        }
    }
    RG_DEFER { DestroyWindow(dummy_wnd); };

    {
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = RG_SIZE(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        int suggested_pixel_fmt = ChoosePixelFormat(dummy_dc, &pfd);
        if (!SetPixelFormat(dummy_dc, suggested_pixel_fmt, &pfd)) {
            LogError("Failed to set pixel format for dummy window: %1", GetWin32ErrorString());
            return false;
        }
    }

    HGLRC dummy_ctx = wglCreateContext(dummy_dc);
    if (!dummy_ctx) {
        LogError("Failed to create OpenGL context for dummy window: %1", GetWin32ErrorString());
        return false;
    }
    RG_DEFER { wglDeleteContext(dummy_ctx); };

    if (!wglMakeCurrent(dummy_dc, dummy_ctx)) {
        LogError("Failed to change OpenGL context of dummy window: %1", GetWin32ErrorString());
        return false;
    }
    RG_DEFER { wglMakeCurrent(dummy_dc, nullptr); };

#define IMPORT_WGL_FUNCTION(Name) \
        do { \
            Name = (decltype(Name))(void *)wglGetProcAddress(#Name); \
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

static HGLRC CreateGLContext(const char *application_name, HDC dc)
{
    if (!InitWGL(application_name))
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
            0
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
        DescribePixelFormat(dc, pixel_fmt_index, RG_SIZE(pixel_fmt_desc), &pixel_fmt_desc);
        if (!SetPixelFormat(dc, pixel_fmt_index, &pixel_fmt_desc)) {
            LogError("Cannot set pixel format on GL window: %1", GetWin32ErrorString());
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
    RG_DEFER_N(gl_guard) { wglDeleteContext(gl); };

    gl_guard.Disable();
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
        if (!wglSwapIntervalEXT(1)) {
            static bool vsync_error_warned;
            if (!vsync_error_warned) {
                LogError("Failed to enable V-sync, ignoring");
                vsync_error_warned = true;
            }
        }
    }

    return true;
}

bool gui_Window::Create(const char *application_name)
{
    RG_ASSERT(!window);

    window = new gui_Win32Window();
    RG_DEFER_N(out_guard) { Release(); };

    thread_window = window;
    thread_info = &priv;
    priv = {};

    window->hwnd = CreateMainWindow(application_name);
    if (!window->hwnd)
        return false;

    window->hdc = GetDC(window->hwnd);
    window->hgl = CreateGLContext(application_name, window->hdc);
    if (!window->hgl)
        return false;
    if (!SetGLContext(window->hdc, window->hgl))
        return false;

    if (!ogl_InitFunctions([](const char *name) { return (void *)wglGetProcAddress(name); }))
        return false;

    out_guard.Disable();
    return true;
}

void gui_Window::Release()
{
    if (imgui_local) {
        ReleaseImGui();
    }

    if (window) {
        if (window->hgl) {
            DeleteGLContext(window->hgl);
        }
        if (window->hwnd) {
            DeleteMainWindow(window->hwnd);
        }

        delete window;
        window = nullptr;
    }
}

void gui_Window::SwapBuffers()
{
    ::SwapBuffers(window->hdc);
}

bool gui_Window::ProcessEvents(bool wait)
{
    thread_window = window;
    thread_info = &priv;

    // Update monotonic clock
    {
        LARGE_INTEGER perf_freq, perf_counter;
        QueryPerformanceFrequency(&perf_freq);
        QueryPerformanceCounter(&perf_counter);

        double monotonic_time = (double)perf_counter.QuadPart / (double)perf_freq.QuadPart;
        priv.time.monotonic_delta = monotonic_time - priv.time.monotonic;
        priv.time.monotonic = monotonic_time;
    }

    // Reset relative inputs
    priv.input.events.Clear();
    priv.input.text.Clear();
    priv.input.text.data[priv.input.text.len] = 0;
    priv.input.buttons &= ~window->released_buttons;
    window->released_buttons = 0;
    priv.input.wheel_x = 0;
    priv.input.wheel_y = 0;

    // Pump Win32 messages
    {
        MSG msg;
        while (wait ? GetMessage(&msg, nullptr, 0, 0)
                    : PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return false;

            TranslateMessage(&msg);
            DispatchMessage(&msg);

            priv.input.interaction_time = priv.time.monotonic;
            wait = false;
        }
    }

    // XXX: Should we report an error instead?
    bool success = SetGLContext(window->hdc, window->hgl);
    RG_ASSERT(success);

    if (imgui_local) {
        StartImGuiFrame();
    }

    return true;
}

}

#endif
