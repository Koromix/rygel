#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../heimdall/kutil.hh"
#include "../heimdall/main.hh"
#include "../heimdall/opengl.hh"
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
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hgl = nullptr;

    bool mouse_tracked = false;
};

static Win32Window main_window;

static MainInfo sys_main_priv;
const MainInfo *const sys_main = &sys_main_priv;
static DisplayInfo sys_display_priv;
const DisplayInfo *const sys_display = &sys_display_priv;
static MouseInfo sys_mouse_priv;
const MouseInfo *const sys_mouse = &sys_mouse_priv;

static const char *GetWin32ErrorMessage(DWORD err)
{
    static char msg_buf[2048];

    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg_buf, sizeof(msg_buf), NULL)) {
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
            sys_display_priv.width = (int)(lparam & 0xFFFF);
            sys_display_priv.height = (int)(lparam >> 16);
        } break;

        case WM_MOUSELEAVE: {
            main_window.mouse_tracked = false;
        } // fallthrough
        case WM_KILLFOCUS: {
            sys_mouse_priv.buttons = 0;
        } break;

        case WM_MOUSEMOVE: {
            sys_mouse_priv.x = (int16_t)(lparam & 0xFFFF);
            sys_mouse_priv.y = (int16_t)(lparam >> 16);

            if (!main_window.mouse_tracked) {
                TRACKMOUSEEVENT tme = { sizeof(tme) };
                tme.hwndTrack = main_window.hwnd;
                tme.dwFlags = TME_LEAVE;
                TrackMouseEvent(&tme);

                main_window.mouse_tracked = true;
            }
        } break;
        case WM_LBUTTONDOWN: {
            sys_mouse_priv.buttons |= MaskEnum(MouseInfo::Left);
        } break;
        case WM_LBUTTONUP: {
            sys_mouse_priv.buttons &= ~MaskEnum(MouseInfo::Left);
        } break;
        case WM_MBUTTONDOWN: {
            sys_mouse_priv.buttons |= MaskEnum(MouseInfo::Middle);
        } break;
        case WM_MBUTTONUP: {
            sys_mouse_priv.buttons &= ~MaskEnum(MouseInfo::Middle);
        } break;
        case WM_RBUTTONDOWN: {
            sys_mouse_priv.buttons |= MaskEnum(MouseInfo::Right);
        } break;
        case WM_RBUTTONUP: {
            sys_mouse_priv.buttons &= ~MaskEnum(MouseInfo::Right);
        } break;
        case WM_XBUTTONDOWN: {
            uint16_t button = (uint16_t)(2 + (wparam >> 16));
            sys_mouse_priv.buttons |= (unsigned int)(1 << button);
        } break;
        case WM_XBUTTONUP: {
            uint16_t button = (uint16_t)(2 + (wparam >> 16));
            sys_mouse_priv.buttons &= ~(unsigned int)(1 << button);
        } break;
        case WM_MOUSEWHEEL: {
            sys_mouse_priv.wheel_y += (int16_t)(wparam >> 16) / WHEEL_DELTA;
        } break;
        case WM_MOUSEHWHEEL: {
            sys_mouse_priv.wheel_x += (int16_t)(wparam >> 16) / WHEEL_DELTA;
        } break;

        case WM_CLOSE: {
            sys_main_priv.run = false;
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
        WNDCLASSEX gl_cls = { sizeof(gl_cls) };
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
        rect.right = 800;
        rect.bottom = 600;
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
        WNDCLASSEX dummy_cls = { sizeof(dummy_cls) };
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
        PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR) };
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
        DescribePixelFormat(dc, pixel_fmt_index, sizeof(pixel_fmt_desc), &pixel_fmt_desc);
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

void SwapGLBuffers()
{
    SwapBuffers(main_window.hdc);
}

void StopMainLoop() {
    sys_main_priv.run = false;
}

int main(int, char **)
{
    main_window.hwnd = CreateMainWindow();
    if (!main_window.hwnd)
        return 1;
    DEFER { DeleteMainWindow(main_window.hwnd); };

    main_window.hdc = GetDC(main_window.hwnd);
    main_window.hgl = CreateGLContext(main_window.hdc);
    if (!main_window.hgl)
        return 1;
    DEFER { DeleteGLContext(main_window.hgl); };
    if (!SetGLContext(main_window.hdc, main_window.hgl))
        return 1;

    sys_main_priv.run = true;
    while (sys_main_priv.run) {
        // Reset relative inputs
        sys_mouse_priv.wheel_x = 0;
        sys_mouse_priv.wheel_y = 0;

        // Pump Win32 messages
        {
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    sys_main_priv.run = false;
                    break;
                }

                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        // Update monotonic clock
        {
            LARGE_INTEGER perf_freq, perf_counter;
            QueryPerformanceFrequency(&perf_freq);
            QueryPerformanceCounter(&perf_counter);

            double monotonic_time = (double)perf_counter.QuadPart / (double)perf_freq.QuadPart;
            sys_main_priv.monotonic_delta = monotonic_time - sys_main_priv.monotonic_time;
            sys_main_priv.monotonic_time = monotonic_time;
        }

        // Run the real code
        if (!Run())
            return 1;

        sys_main_priv.iteration_count++;
    }

    return 0;
}

#endif
