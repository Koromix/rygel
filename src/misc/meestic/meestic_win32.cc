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
#include "config.hh"
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

static Config config;
static Size profile_idx = 0;

static HWND hwnd;
static NOTIFYICONDATAA notify;

static void ShowAboutDialog()
{
    HINSTANCE module = GetModuleHandle(nullptr);

    TASKDIALOGCONFIG dialog = {};

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

    dialog.cbSize = RG_SIZE(dialog);
    dialog.hwndParent = hwnd;
    dialog.hInstance = module;
    dialog.dwCommonButtons = TDCBF_OK_BUTTON;
    dialog.pszWindowTitle = title;
    dialog.hMainIcon = LoadIcon(module, MAKEINTRESOURCE(1));
    dialog.pszMainInstruction = main;
    dialog.pszContent = content;
    dialog.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT | (dialog.hMainIcon ? TDF_USE_HICON_MAIN : 0);

    dialog.pfCallback = [](HWND hwnd, UINT msg, WPARAM, LPARAM lparam, LONG_PTR) {
        if (msg == TDN_HYPERLINK_CLICKED) {
            const wchar_t *url = (const wchar_t *)lparam;
            ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);

            // Close the dialog by simulating a button click
            PostMessageW(hwnd, TDM_CLICK_BUTTON, IDOK, 0);
        }

        return (HRESULT)S_OK;
    };

    TaskDialogIndirect(&dialog, nullptr, nullptr, nullptr);
}

static LRESULT __stdcall MainWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    static UINT taskbar_created = RegisterWindowMessageA("TaskbarCreated");

    switch (msg) {
        case WM_USER_TRAY: {
            int button = LOWORD(lparam);

            if (button == WM_RBUTTONDOWN) {
                POINT click;
                GetCursorPos(&click);

                HMENU menu = CreatePopupMenu();
                RG_DEFER { DestroyMenu(menu); };

                for (Size i = 0; i < config.profiles.len; i++) {
                    const ConfigProfile &profile = config.profiles[i];

                    int flags = MF_STRING | (i == profile_idx ? MF_CHECKED : 0);
                    AppendMenuA(menu, flags, i + 10, profile.name);
                }
                AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(menu, MF_STRING, 1, "&About");
                AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(menu, MF_STRING, 2, "&Exit");

                int align = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
                int action = (int)TrackPopupMenu(menu, align | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
                                                 click.x, click.y, 0, hwnd, nullptr);

                switch (action) {
                    case 0: {} break;

                    case 1: { ShowAboutDialog(); } break;
                    case 2: { PostQuitMessage(0); } break;

                    default: {
                        Size idx = action - 10;

                        if (idx >= 0 && idx < config.profiles.len) {
                            profile_idx = idx;

                            const ConfigProfile &profile = config.profiles[idx];
                            ApplyLight(profile.settings);
                        }
                    } break;
                }

                return TRUE;
            }
        } break;

        case WM_USER_TOGGLE: {
            do {
                profile_idx++;
                if (profile_idx >= config.profiles.len) {
                    profile_idx = 0;
                }
            } while (config.profiles[profile_idx].manual);

            ApplyLight(config.profiles[profile_idx].settings);
        } break;

        case WM_CLOSE: {
            PostQuitMessage(0);
            return 0;
        } break;

        default: {
            if (msg == taskbar_created && !Shell_NotifyIconA(NIM_ADD, &notify)) {
                LogError("Failed to restore tray icon: %1", GetWin32ErrorString());
                PostQuitMessage(1);
            }
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

    // Redirect log when /subsystem:windows is used
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN) {
        if (!RedirectLogToWindowsEvents(FelixTarget))
            return 1;
    }

    // Default config filename
    const char *config_filename;
    {
        Span<const char> prefix = GetApplicationExecutable();
        prefix.len -= prefix.end() - GetPathExtension(prefix).ptr;

        config_filename = Fmt(&config.str_alloc, "%1.ini", prefix).ptr;
    }

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0%!0)",
                FelixTarget, config_filename);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Parse options
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Parse config file
    if (TestFile(config_filename, FileType::File)) {
        if (!LoadConfig(config_filename, &config))
            return 1;

        profile_idx = config.default_idx;
    } else {
        config.profiles.Append({.name = "Enable", .settings = {.mode = LightMode::Static}});
        config.profiles.Append({.name = "Disable", .settings = {.mode = LightMode::Disabled}});
    }

    HINSTANCE module = GetModuleHandle(nullptr);
    const char *cls_name = FelixTarget;
    const char *win_name = FelixTarget;

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
                           CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, module, nullptr);
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
    {
        notify.cbSize = RG_SIZE(notify);
        notify.hWnd = hwnd;
        notify.uID = 0xA56B96F2u;
        notify.hIcon = LoadIcon(module, MAKEINTRESOURCE(1));
        notify.uCallbackMessage = WM_USER_TRAY;
        CopyString(FelixTarget, notify.szTip);
        notify.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;

        if (!notify.hIcon || !Shell_NotifyIconA(NIM_ADD, &notify)) {
            LogError("Failed to register tray icon: %1", GetWin32ErrorString());
            return 1;
        }
    }
    RG_DEFER { Shell_NotifyIconA(NIM_DELETE, &notify); };

    // Check that it works once, at least
    if (!ApplyLight(config.profiles[config.default_idx].settings))
        return 1;

    // Run main message loop
    int code;
    {
        MSG msg;
        DWORD ret;

        msg.wParam = 1;
        while ((ret = GetMessage(&msg, nullptr, 0, 0))) {
            if (ret < 0) {
                LogError("GetMessage() failed: %1", GetWin32ErrorString());
                return 1;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        code = (int)msg.wParam;
    }

    return code;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }

#endif
