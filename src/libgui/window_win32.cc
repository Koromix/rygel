// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

#include "../libcc/libcc.hh"
#include "window.hh"
#include "../wrappers/opengl.hh"

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
};

static RG_THREAD_LOCAL gui_Info *thread_info;
static RG_THREAD_LOCAL gui_Win32Window *thread_window;

static const char *GetWin32ErrorMessage(DWORD err)
{
    static char msg_buf[2048];

    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg_buf, RG_SIZE(msg_buf), NULL)) {
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
            thread_info->display.width = (int)(lparam & 0xFFFF);
            thread_info->display.height = (int)(lparam >> 16);
        } break;

        case WM_MOUSELEAVE: { thread_info->input.mouseover = false; } RG_FALLTHROUGH;
        case WM_KILLFOCUS: {
            thread_info->input.keys.Clear();
            thread_info->input.buttons = 0;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
#define HANDLE_KEY(VkCode, Code) \
                case (VkCode): { thread_info->input.keys.Set((Size)(Code), state); } break

            bool state = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
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
                HANDLE_KEY('C', gui_InputKey::C);
                HANDLE_KEY('V', gui_InputKey::V);
                HANDLE_KEY('X', gui_InputKey::X);
                HANDLE_KEY('Y', gui_InputKey::Y);
                HANDLE_KEY('Z', gui_InputKey::Z);
            }

#undef HANDLE_KEY
        } break;
        case WM_CHAR: {
            uint16_t c = (uint16_t)wparam;

            // TODO: Deal with supplementary planes
            if (c < 0x80 && RG_LIKELY(thread_info->input.text.Available() >= 1)) {
                thread_info->input.text.Append((char)c);
            } else if (c < 0x800 && RG_LIKELY(thread_info->input.text.Available() >= 2)) {
                thread_info->input.text.Append((char)(0xC0 | (c >> 6)));
                thread_info->input.text.Append((char)(0x80 | (c & 0x3F)));
            } else if (RG_LIKELY(thread_info->input.text.Available() >= 3)) {
                thread_info->input.text.Append((char)(0xE0 | (c >> 12)));
                thread_info->input.text.Append((char)(0x80 | ((c >> 6) & 0x3F)));
                thread_info->input.text.Append((char)(0x80 | (c & 0x3F)));
            } else {
                LogError("Dropping text events (buffer full)");
            }
        } break;

        case WM_MOUSEMOVE: {
            thread_info->input.x = (int16_t)(lparam & 0xFFFF);
            thread_info->input.y = (int16_t)(lparam >> 16);

            if (!thread_info->input.mouseover) {
                TRACKMOUSEEVENT tme = { RG_SIZE(tme) };
                tme.hwndTrack = thread_window->hwnd;
                tme.dwFlags = TME_LEAVE;
                TrackMouseEvent(&tme);

                thread_info->input.mouseover = true;
            }
        } break;
        case WM_LBUTTONDOWN: { thread_info->input.buttons |= MaskEnum(gui_InputButton::Left); } break;
        case WM_LBUTTONUP: { thread_window->released_buttons |= MaskEnum(gui_InputButton::Left); } break;
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

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HWND CreateMainWindow(const char *application_name)
{
    // Create Win32 main window class
    static char main_cls_name[256];
    static ATOM main_cls_atom;
    if (!main_cls_atom) {
        Fmt(main_cls_name, "%1_main", application_name);

        WNDCLASSEX gl_cls = { RG_SIZE(gl_cls) };
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
        rect.right = 1152;
        rect.bottom = 648;
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);

        main_wnd = CreateWindowEx(0, main_cls_name, application_name, WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  rect.right - rect.left, rect.bottom - rect.top,
                                  nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!main_wnd) {
            LogError("Failed to create Win32 window: %s", GetWin32ErrorMessage());
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

    // First, we need a dummy window handle to create OpenGL context (...). I know
    // it is ugly, but not my fault.

    char dummy_cls_name[256];
    Fmt(dummy_cls_name, "%1_init_gl", application_name);

    {
        WNDCLASSEX dummy_cls = { RG_SIZE(dummy_cls) };
        dummy_cls.hInstance = GetModuleHandle(nullptr);
        dummy_cls.lpszClassName = dummy_cls_name;
        dummy_cls.lpfnWndProc = DefWindowProc;
        if (!RegisterClassEx(&dummy_cls)) {
            LogError("Failed to register window class '%1': %2", dummy_cls_name,
                     GetWin32ErrorMessage());
            return false;
        }
    }
    RG_DEFER { UnregisterClass(dummy_cls_name, GetModuleHandle(nullptr)); };

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
    RG_DEFER { DestroyWindow(dummy_wnd); };

    {
        PIXELFORMATDESCRIPTOR pfd = { RG_SIZE(PIXELFORMATDESCRIPTOR) };
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
    RG_DEFER { wglDeleteContext(dummy_ctx); };

    if (!wglMakeCurrent(dummy_dc, dummy_ctx)) {
        LogError("Failed to change OpenGL context of dummy window: %1", GetWin32ErrorMessage());
        return false;
    }
    RG_DEFER { wglMakeCurrent(dummy_dc, nullptr); };

#define IMPORT_WGL_FUNCTION(Name) \
        do { \
            Name = (decltype(Name))wglGetProcAddress(#Name); \
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

bool gui_Window::Init(const char *application_name)
{
    RG_ASSERT_DEBUG(!window);

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

bool gui_Window::Prepare()
{
    thread_window = window;
    thread_info = &priv;

    // Reset relative inputs
    priv.input.text.Clear();
    priv.input.buttons &= ~window->released_buttons;
    window->released_buttons = 0;
    priv.input.wheel_x = 0;
    priv.input.wheel_y = 0;

    // Pump Win32 messages
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return false;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Append NUL byte to keyboard text
    if (!priv.input.text.Available()) {
        priv.input.text.len--;
    }
    priv.input.text.Append('\0');

    // Update monotonic clock
    {
        LARGE_INTEGER perf_freq, perf_counter;
        QueryPerformanceFrequency(&perf_freq);
        QueryPerformanceCounter(&perf_counter);

        double monotonic_time = (double)perf_counter.QuadPart / (double)perf_freq.QuadPart;
        priv.time.monotonic_delta = monotonic_time - priv.time.monotonic;
        priv.time.monotonic = monotonic_time;
    }

    // FIXME: Should we report an error instead?
    RG_ASSERT(SetGLContext(window->hdc, window->hgl));
    if (imgui_local) {
        StartImGuiFrame();
    }

    return true;
}

}

#endif
