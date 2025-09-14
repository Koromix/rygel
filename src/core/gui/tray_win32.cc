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
#include "tray.hh"
#include "vendor/stb/stb_image.h"
#include "vendor/stb/stb_image_resize2.h"

#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x0A000005
#define _WIN32_WINNT 0x0A000005
#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

namespace K {

#define WM_APP_TRAY (WM_APP + 1)
#define WM_APP_UPDATE (WM_APP + 2)

struct IconSet {
    LocalArray<Span<const uint8_t>, 8> pixmaps;
    BlockAllocator allocator;
};

struct MenuItem {
    const char *label;
    int check;

    std::function<void()> func;
};

static const Vec2<int> IconSizes[] = {
    { 16, 16 },
    { 20, 20 },
    { 24, 24 },
    { 28, 28 },
    { 32, 32 },
    { 40, 40 },
    { 48, 48 },
    { 64, 64 }
};
static_assert(K_LEN(IconSizes) <= K_LEN(IconSet::pixmaps.data));

class WinTray: public gui_TrayIcon {
    HWND hwnd = nullptr;
    NOTIFYICONDATAA notify = {};

    IconSet icons;
    std::function<void()> activate;
    std::function<void()> context;
    BucketArray<MenuItem> items;

public:
    ~WinTray();

    bool Init();

    bool SetIcon(Span<const uint8_t> png) override;

    void OnActivation(std::function<void()> func) override;
    void OnContext(std::function<void()> func) override;

    void AddAction(const char *label, int check, std::function<void()> func) override;
    void AddSeparator() override;
    void ClearMenu() override;

    WaitSource GetWaitSource() override;
    bool ProcessEvents() override;

private:
    void RunMessageThread();

    static LRESULT __stdcall TrayProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    bool UpdateIcon();
};

static bool PrepareIcons(Span<const uint8_t> png, IconSet *out_set)
{
    IconSet set;

    int width, height, channels;
    uint8_t *img = stbi_load_from_memory(png.ptr, (int)png.len, &width, &height, &channels, 4);
    if (!img) {
        LogError("Failed to load PNG tray icon");
        return false;
    }
    K_DEFER { free(img); };

    for (Size i = 0; i < K_LEN(IconSizes); i++) {
        const Vec2<int> size = IconSizes[i];

        Size len = 4 * size.x * size.y;
        Span<uint8_t> pixmap = AllocateSpan<uint8_t>(&set.allocator, len);

        if (!stbir_resize_uint8_linear(img, width, height, 0, pixmap.ptr, size.x, size.y, 0, STBIR_RGBA)) {
            LogError("Failed to resize tray icon");
            return false;
        }

        // Convert from RGBA32 (Big Endian) to BGRA32 (Big Endian)
        for (Size j = 0; j < len; j += 4) {
            uint32_t pixel = BigEndian(*(uint32_t *)(pixmap.ptr + j));

            pixmap[j + 0] = (uint8_t)((pixel >> 8) & 0xFF);
            pixmap[j + 1] = (uint8_t)((pixel >> 16) & 0xFF);
            pixmap[j + 2] = (uint8_t)((pixel >> 24) & 0xFF);
            pixmap[j + 3] = (uint8_t)((pixel >> 0) & 0xFF);
        }

        set.pixmaps.Append(pixmap);
    }

    std::swap(*out_set, set);
    return true;
}

WinTray::~WinTray()
{
    if (notify.hIcon) {
        Shell_NotifyIconA(NIM_DELETE, &notify);
        DestroyIcon(notify.hIcon);
    }

    if (hwnd) {
        DestroyWindow(hwnd);
    }
}

bool WinTray::Init()
{
    K_ASSERT(!hwnd);

    static std::once_flag flag;
    static const char *ClassName = "TrayClass";
    static const char *WindowName = "TrayWindow";

    HINSTANCE module = GetModuleHandle(nullptr);

    // Register window class (once)
    // It is local to the process, we can leak it. Windows will clean it up.
    std::call_once(flag, [&]() {
        WNDCLASSEXA wc = {};

        wc.cbSize = K_SIZE(wc);
        wc.hInstance = module;
        wc.lpszClassName = ClassName;
        wc.lpfnWndProc = &WinTray::TrayProc;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        if (!RegisterClassExA(&wc)) {
            LogError("Failed to register window class '%1': %2", ClassName, GetWin32ErrorString());
            return;
        }
    });

    // Create hidden window
    hwnd = CreateWindowExA(0, ClassName, WindowName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, module, nullptr);
    if (!hwnd) {
        LogError("Failed to create window named '%1': %2", WindowName, GetLastError(), GetWin32ErrorString());
        return false;
    }
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)this);

    // Prepare tray data
    notify.cbSize = K_SIZE(notify);
    notify.hWnd = hwnd;
    notify.uID = 0xA56B96F2u;
    notify.uCallbackMessage = WM_APP_TRAY;
    CopyString(FelixTarget, notify.szTip);
    notify.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;

    return true;
}

bool WinTray::SetIcon(Span<const uint8_t> png)
{
    K_ASSERT(hwnd);

    if (!PrepareIcons(png, &icons))
        return false;
    PostMessageA(hwnd, WM_APP_UPDATE, 0, 0);

    return true;
}

void WinTray::OnActivation(std::function<void()> func)
{
    activate = func;
}

void WinTray::OnContext(std::function<void()> func)
{
    context = func;
}

void WinTray::AddAction(const char *label, int check, std::function<void()> func)
{
    K_ASSERT(label);
    K_ASSERT(check <= 1);

    Allocator *alloc;
    MenuItem *item = items.AppendDefault(&alloc);

    item->label = DuplicateString(label, alloc).ptr;
    item->check = check;
    item->func = func;
}

void WinTray::AddSeparator()
{
    items.AppendDefault();
}

void WinTray::ClearMenu()
{
    items.Clear();
}

WaitSource WinTray::GetWaitSource()
{
    // We need to run the Win32 message pump, not a specific HANDLE
    WaitSource src = { nullptr, -1 };
    return src;
}

bool WinTray::ProcessEvents()
{
    MSG msg;
    while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}

LRESULT __stdcall WinTray::TrayProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    static UINT TaskbarCreated = RegisterWindowMessageA("TaskbarCreated");

    WinTray *self = (WinTray *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    if (msg == WM_APP_TRAY) {
        int button = LOWORD(lparam);

        if (button == WM_LBUTTONDOWN) {
            if (self->activate) {
                self->activate();
            }
        } else if (button == WM_RBUTTONDOWN) {
            if (self->context) {
                self->context();
            }

            POINT click;
            GetCursorPos(&click);

            HMENU menu = CreatePopupMenu();
            K_DEFER { DestroyMenu(menu); };

            {
                // Keep separate counter because BucketArray iterator is faster than direct indexing
                Size idx = 0;

                for (const MenuItem &item: self->items) {
                    if (item.label) {
                        int flags = MF_STRING | (item.check > 0);
                        AppendMenuA(menu, flags, idx + 1, item.label);
                    } else {
                        AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
                    }

                    idx++;
                }
            }

            int align = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
            int action = (int)TrackPopupMenu(menu, align | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
                                             click.x, click.y, 0, hwnd, nullptr);

            if (action > 0 && action <= self->items.count) {
                const MenuItem &item = self->items[action - 1];

                if (item.func) {
                    // Copy std::function (hopefully this is not expensive) because the handler might destroy it
                    // if it uses ClearMenu(), for example. Unlikely but let's play it safe!
                    std::function<void()> func = item.func;
                    func();
                }
            }
        }
    } else if (msg == WM_APP_UPDATE || msg == WM_DPICHANGED || msg == TaskbarCreated) {
        self->UpdateIcon();
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static HICON CreateAlphaIcon(Span<const uint8_t> pixmap, int width, int height)
{
    ICONINFO info = {};

    info.fIcon = TRUE;
    info.hbmColor = CreateBitmap(width, height, 1, 32, pixmap.ptr);
    info.hbmMask = CreateBitmap(width, height, 1, 1, nullptr);
    K_DEFER {
        DeleteObject(info.hbmColor);
        DeleteObject(info.hbmMask);
    };

    HICON icon = CreateIconIndirect(&info);
    if (!icon) {
        LogError("Failed to create tray icon: %1", GetWin32ErrorString());
        return nullptr;
    }

    return icon;
}

bool WinTray::UpdateIcon()
{
    int dpi = GetDpiForWindow(hwnd);
    int width = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    int height = GetSystemMetricsForDpi(SM_CYSMICON, dpi);

    Size idx;
    {
        const Vec2<int> *size = std::find_if(std::begin(IconSizes), std::end(IconSizes),
                                             [&](const Vec2<int> &size) { return size.x == width && size.y == height; });
        if (size == std::end(IconSizes)) {
            LogError("Cannot find appropriate icon size for tray icon");
            return false;
        }
        idx = size - IconSizes;
    }

    HICON icon = CreateAlphaIcon(icons.pixmaps[idx], width, height);
    if (!icon)
        return false;

    if (notify.hIcon) {
        DestroyIcon(notify.hIcon);
    }
    notify.hIcon = icon;

    if (!Shell_NotifyIconA(NIM_MODIFY, &notify) && !Shell_NotifyIconA(NIM_ADD, &notify)) {
        LogError("Failed to restore tray icon");
        return false;
    }

    return true;
}

std::unique_ptr<gui_TrayIcon> gui_CreateTrayIcon(Span<const uint8_t> png)
{
    std::unique_ptr<WinTray> tray = std::make_unique<WinTray>();

    if (!tray->Init())
        return nullptr;
    if (!tray->SetIcon(png))
        return nullptr;

    return tray;
}

}

#endif
