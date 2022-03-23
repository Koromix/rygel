// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#ifdef _WIN32

#include "src/core/libcc/libcc.hh"
#include "lights.hh"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <wchar.h>

namespace RG {

#define WM_USER_TRAY (WM_USER + 1)
#define WM_USER_TOGGLE (WM_USER + 2)

static HWND hwnd;
static LightSettings settings;

static void ShowAboutDialog()
{
    HINSTANCE module = GetModuleHandle(nullptr);

    TASKDIALOGCONFIG config = {};

    wchar_t title[2048];
    wchar_t main[2048];
    wchar_t content[2048];
    ConvertUtf8ToWin32Wide(FelixTarget, title);
    {
        char buf[2048];
        Fmt(buf, "%1 %2", FelixTarget, FelixVersion);
        ConvertUtf8ToWin32Wide(buf, main);
    }
    ConvertUtf8ToWin32Wide(R"(<a href="https://koromix.dev/misc#meestic">https://koromix.dev/</a>)", content);

    config.cbSize = RG_SIZE(config);
    config.hwndParent = hwnd;
    config.hInstance = module;
    config.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_USE_HICON_MAIN | TDF_SIZE_TO_CONTENT;
    config.dwCommonButtons = TDCBF_OK_BUTTON;
    config.pszWindowTitle = title;
    config.hMainIcon = LoadIcon(module, MAKEINTRESOURCE(1));
    config.pszMainInstruction = main;
    config.pszContent = content;

    config.pfCallback = [](HWND hwnd, UINT msg, WPARAM, LPARAM lparam, LONG_PTR) {
        if (msg == TDN_HYPERLINK_CLICKED) {
            const wchar_t *url = (const wchar_t *)lparam;
            ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);

            // Close the dialog by simulating a button click
            PostMessageW(hwnd, TDM_CLICK_BUTTON, IDOK, 0);
        }

        return (HRESULT)S_OK;
    };

    TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
}

static LRESULT __stdcall MainWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_USER_TRAY: {
            int button = LOWORD(lparam);

            if (button == WM_RBUTTONDOWN) {
                POINT click;
                GetCursorPos(&click);

                HMENU menu = CreatePopupMenu();
                RG_DEFER { DestroyMenu(menu); };

                AppendMenuA(menu, MF_STRING, 1, "&Enable");
                AppendMenuA(menu, MF_STRING, 2, "&Disable");
                AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(menu, MF_STRING, 3, "&About");
                AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(menu, MF_STRING, 4, "E&xit");

                int align = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
                int action = (int)TrackPopupMenu(menu, align | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
                                                 click.x, click.y, 0, hwnd, nullptr);

                switch (action) {
                    case 1: {
                        settings.mode = LightMode::Static;
                        ApplyLight(settings);
                    } break;
                    case 2: {
                        settings.mode = LightMode::Disabled;
                        ApplyLight(settings);
                    } break;
                    case 3: { ShowAboutDialog(); } break;
                    case 4: { PostQuitMessage(0); } break;
                }

                return TRUE;
            }
        } break;

        case WM_USER_TOGGLE: {
            settings.mode = (settings.mode == LightMode::Disabled) ? LightMode::Static : LightMode::Disabled;
            ApplyLight(settings);
        } break;

        case WM_CLOSE: {
            PostQuitMessage(0);
            return 0;
        } break;
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static LRESULT __stdcall LowLevelKeyboardProc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == 0) {
        const KBDLLHOOKSTRUCT *kbd = (const KBDLLHOOKSTRUCT *)lparam;

        if (wparam == WM_KEYDOWN && kbd->vkCode == 255 && kbd->scanCode == 14) {
            PostMessageA(hwnd, WM_USER_TOGGLE, 0, 0);
        }
    }

    return CallNextHookEx(nullptr, code, wparam, lparam);
}

int Main(int argc, char **argv)
{
    InitCommonControls();

    HINSTANCE module = GetModuleHandle(nullptr);

    const char *cls_name = FelixTarget;
    const char *win_name = FelixTarget;

    // Redirect log when /subsystem:windows is used
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN) {
        if (!RedirectLogToWindowsEvents(FelixTarget))
            return 1;
    }

    // Register window class
    {
        WNDCLASSEXA wc = {};

        wc.cbSize = RG_SIZE(wc);
        wc.hInstance = module;
        wc.lpszClassName = cls_name;
        wc.lpfnWndProc = MainWindowProc;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        if (!RegisterClassExA(&wc)) {
            LogError("Failed to register window class '%1': %2", cls_name, GetWin32ErrorString());
            return 1;
        }
    }
    RG_DEFER { UnregisterClassA(cls_name, module); };

    // Create hidden window
    hwnd = CreateWindowExA(0, cls_name, win_name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, nullptr, module, nullptr);
    if (!hwnd) {
        LogError("Failed to create window named '%1': %2", win_name, GetWin32ErrorString());
        return 1;
    }
    RG_DEFER { DestroyWindow(hwnd); };

    // We want to intercept Fn+F8, and this is not possible with RegisterHotKey because
    // it is not mapped to a virtual key. We want the raw scan code.
    HHOOK hook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
    if (!hook) {
        LogError("Failed to insert low-level keyboard hook: %1", GetWin32ErrorString());
        return 1;
    }
    RG_DEFER { UnhookWindowsHookEx(hook); };

    // Create tray icon
    NOTIFYICONDATAA notify = {};
    {
        notify.cbSize = RG_SIZE(notify);
        notify.hWnd = hwnd;
        notify.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        notify.uID = 0xA56B96F2u;
        notify.hIcon = LoadIcon(module, MAKEINTRESOURCE(1));
        notify.uCallbackMessage = WM_USER_TRAY;
        CopyString(FelixTarget, notify.szTip);

        if (!Shell_NotifyIconA(NIM_ADD, &notify)) {
            LogError("Failed to register tray icon: %1", GetWin32ErrorString());
            return 1;
        }
    }
    RG_DEFER { Shell_NotifyIconA(NIM_DELETE, &notify); };

    // Check that it works once, at least
    if (!ApplyLight(settings))
        return 1;

    // Run main message loop
    {
        MSG msg;

        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }

#endif
