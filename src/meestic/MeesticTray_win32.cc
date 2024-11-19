// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#if defined(_WIN32)

#include "src/core/base/base.hh"
#include "config.hh"
#include "light.hh"

#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <wchar.h>

namespace RG {

#define WM_APP_TRAY (WM_APP + 1)
#define WM_APP_REHOOK (WM_APP + 2)

static Config config;
static Size profile_idx = 0;

static HWND main_hwnd;
static NOTIFYICONDATAA notify;
static HHOOK hook;
static HANDLE toggle;

static hs_port *port = nullptr;

static void ShowDialog(const char *text)
{
    HINSTANCE module = GetModuleHandle(nullptr);

    TASKDIALOGCONFIG dialog = {};

    wchar_t title[1024]; ConvertUtf8ToWin32Wide(FelixTarget, title);
    wchar_t main[1024]; swprintf(main, RG_LEN(main), L"%hs %hs", FelixTarget, FelixVersion);
    wchar_t content[2048]; ConvertUtf8ToWin32Wide(text, content);

    dialog.cbSize = RG_SIZE(dialog);
    dialog.hwndParent = main_hwnd;
    dialog.hInstance = module;
    dialog.dwCommonButtons = TDCBF_OK_BUTTON;
    dialog.pszWindowTitle = title;
    dialog.hMainIcon = LoadIcon(module, MAKEINTRESOURCE(1));
    dialog.pszMainInstruction = main;
    dialog.pszContent = content;
    dialog.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT | (dialog.hMainIcon ? TDF_USE_HICON_MAIN : 0);

    dialog.pfCallback = [](HWND, UINT msg, WPARAM, LPARAM lparam, LONG_PTR) {
        if (msg == TDN_HYPERLINK_CLICKED) {
            const wchar_t *url = (const wchar_t *)lparam;
            ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);

            // Close the dialog by simulating a button click
            PostMessageW(main_hwnd, TDM_CLICK_BUTTON, IDOK, 0);
        }

        return (HRESULT)S_OK;
    };

    TaskDialogIndirect(&dialog, nullptr, nullptr, nullptr);
}

static void ShowAboutDialog()
{
    const char *text = R"(<a href="https://koromix.dev/misc#meestic">https://koromix.dev/</a>)";
    ShowDialog(text);
}

static bool ApplyProfile(Size idx)
{
    LogInfo("Applying profile %1", idx);

    if (port) {
        // Should work first time...
        {
            PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
            RG_DEFER { PopLogFilter(); };

            if (ApplyLight(port, config.profiles[idx].settings)) {
                profile_idx = idx;
                return true;
            }
        }

        CloseLightDevice(port);
        port = OpenLightDevice();
        if (!port)
            return false;
        if (!ApplyLight(port, config.profiles[idx].settings))
            return false;
    }

    profile_idx = idx;

    return true;
}

static bool ToggleProfile(int delta)
{
    if (!delta)
        return true;

    Size next_idx = profile_idx;

    do {
        next_idx += delta;

        if (next_idx < 0) {
            next_idx = config.profiles.count - 1;
        } else if (next_idx >= config.profiles.count) {
            next_idx = 0;
        }
    } while (config.profiles[next_idx].manual);

    return ApplyProfile(next_idx);
}

static bool UpdateTrayIcon()
{
    HINSTANCE module = GetModuleHandle(nullptr);
    HICON icon = LoadIcon(module, MAKEINTRESOURCE(1));

    if (!icon) {
        LogError("Failed to update tray icon: %1", GetWin32ErrorString());
        icon = notify.hIcon;
    }
    notify.hIcon = icon;

    if (!Shell_NotifyIconA(NIM_ADD, &notify)) {
        LogError("Failed to restore tray icon: %1", GetWin32ErrorString());
        return false;
    }

    return true;
}

static LRESULT __stdcall LowLevelKeyboardProc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == 0) {
        const KBDLLHOOKSTRUCT *kbd = (const KBDLLHOOKSTRUCT *)lparam;

        if (wparam == WM_KEYDOWN && kbd->vkCode == 255 && kbd->scanCode == 14) {
            SetEvent(toggle);
        }
    }

    return CallNextHookEx(hook, code, wparam, lparam);
}

static LRESULT __stdcall MainWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    static UINT taskbar_created = RegisterWindowMessageA("TaskbarCreated");

    UINT msg_or_timer = (msg != WM_TIMER) ? msg : (UINT)wparam;

    switch (msg_or_timer) {
        case WM_APP_TRAY: {
            int button = LOWORD(lparam);

            if (button == WM_LBUTTONDOWN) {
                if (!ToggleProfile(1)) {
                    PostQuitMessage(1);
                }
            } else if (button == WM_RBUTTONDOWN) {
                POINT click;
                GetCursorPos(&click);

                HMENU menu = CreatePopupMenu();
                RG_DEFER { DestroyMenu(menu); };

                for (Size i = 0; i < config.profiles.count; i++) {
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

                        if (idx >= 0 && idx < config.profiles.count) {
                            if (!ApplyProfile(idx)) {
                                PostQuitMessage(1);
                            }
                        }
                    } break;
                }
            }
        } break;

        case WM_APP_REHOOK: {
            if (hook) {
                UnhookWindowsHookEx(hook);
            }

            LogDebug("Reinserting low-level keyboard hook");

            hook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
            if (!hook) {
                LogError("Failed to insert low-level keyboard hook: %1", GetWin32ErrorString());
                PostQuitMessage(1);
            }
        } break;

        case WM_CLOSE: {
            PostQuitMessage(0);
            return 0;
        } break;

        case WM_DPICHANGED: {
            if (!UpdateTrayIcon()) {
                PostQuitMessage(1);
            }
        } break;

        default: {
            if (msg == taskbar_created) {
                if (!UpdateTrayIcon()) {
                    PostQuitMessage(1);
                }
            }
        } break;
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static void RedirectErrors() {
    static wchar_t title[1024];
    ConvertUtf8ToWin32Wide(FelixTarget, title);

    SetLogHandler([](LogLevel level, const char *ctx, const char *msg) {
        UINT flags = 0;
        LocalArray<wchar_t, 8192> buf_w;

        switch (level)  {
            case LogLevel::Debug:
            case LogLevel::Info: return;

            case LogLevel::Warning: { flags |= MB_ICONWARNING; } break;
            case LogLevel::Error: { flags |= MB_ICONERROR; } break;
        }
        flags |= MB_OK;

        // Append context
        if (ctx) {
            Size len = ConvertUtf8ToWin32Wide(ctx, buf_w.Take(0, RG_LEN(buf_w.data) - 2));
            if (len < 0)
                return;
            wcscpy(buf_w.data + len, L": ");
            buf_w.len += len + 2;
        }

        // Append message
        {
            Size len = ConvertUtf8ToWin32Wide(msg, buf_w.TakeAvailable());
            if (len < 0)
                return;
            buf_w.len += len;
        }

        const wchar_t *ptr = buf_w.data;
        MessageBoxW(nullptr, ptr, title, flags);
    }, false);
}

static inline ConfigProfile MakeDefaultProfile(const char *name, LightMode mode)
{
    ConfigProfile profile = {};

    profile.name = name;
    profile.settings.mode = mode;

    return profile;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    BlockAllocator temp_alloc;

    InitCommonControls();

    // Use message boxes when /subsystem:windows is used
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN) {
        RedirectErrors();
    }

    // Default config filename
    LocalArray<const char *, 4> config_filenames;
    const char *config_filename = FindConfigFile({ "MeesticTray.ini", "MeesticGui.ini" }, &temp_alloc, &config_filenames);

    const auto print_usage = [=]() {
        HeapArray<char> help;

        Fmt(&help,
R"(Usage: %!..+%1 [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

By default, the first of the following config files will be used:

)",
                FelixTarget);

        for (const char *filename: config_filenames) {
            Fmt(&help, "    %1\n", filename);
        }

        ShowDialog(help.ptr);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        HeapArray<char> version;

        Fmt(&version, "%1 %2\n", FelixTarget, FelixVersion);
        Fmt(&version, "Compiler: %1", FelixCompiler);

        ShowDialog(version.ptr);
        return 0;
    }

    // Parse options
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage();
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                if (IsDirectory(opt.current_value)) {
                    config_filename = Fmt(&temp_alloc, "%1%/meestic.ini", TrimStrRight(opt.current_value, RG_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Parse config file
    if (config_filename && TestFile(config_filename, FileType::File)) {
        if (!LoadConfig(config_filename, &config))
            return 1;

        profile_idx = config.default_idx;
    } else {
        config.profiles.Append(MakeDefaultProfile("Enable", LightMode::Static));
        config.profiles.Append(MakeDefaultProfile("Disable", LightMode::Disabled));
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
    main_hwnd = CreateWindowExA(0, cls_name, win_name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, module, nullptr);
    if (!main_hwnd) {
        LogError("Failed to create window named '%1': %2", win_name, GetLastError(), GetWin32ErrorString());
        return 1;
    }
    RG_DEFER { DestroyWindow(main_hwnd); };

    // We want to intercept Fn+F8, and this is not possible with RegisterHotKey because
    // it is not mapped to a virtual key. We want the raw scan code.
    hook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
    if (!hook) {
        LogError("Failed to insert low-level keyboard hook: %1", GetWin32ErrorString());
        return 1;
    }
    RG_DEFER { if (hook) UnhookWindowsHookEx(hook); };

    // Unfortunately, Windows sometimes disconnects our hook for no good reason
    if (!SetTimer(main_hwnd, WM_APP_REHOOK, 30000, nullptr)) {
        LogError("Failed to create Win32 timer: %1", GetWin32ErrorString());
        return 1;
    }
    RG_DEFER { KillTimer(main_hwnd, WM_APP_REHOOK); };

    toggle = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!toggle) {
        LogError("Failed to create Win32 event object: %1", GetWin32ErrorString());
        return 1;
    }
    RG_DEFER { CloseHandle(toggle); };

    // Create tray icon
    {
        notify.cbSize = RG_SIZE(notify);
        notify.hWnd = main_hwnd;
        notify.uID = 0xA56B96F2u;
        notify.hIcon = LoadIcon(module, MAKEINTRESOURCE(1));
        notify.uCallbackMessage = WM_APP_TRAY;
        CopyString(FelixTarget, notify.szTip);
        notify.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;

        if (!notify.hIcon || !Shell_NotifyIconA(NIM_ADD, &notify)) {
            LogError("Failed to register tray icon: %1", GetWin32ErrorString());
            return 1;
        }
    }
    RG_DEFER { Shell_NotifyIconA(NIM_DELETE, &notify); };

    // Open the light MSI HID device ahead of time
    if (!GetDebugFlag("FAKE_LIGHTS")) {
        port = OpenLightDevice();
        if (!port)
            return 1;
    }
    RG_DEFER { CloseLightDevice(port); };

    // Check that it works once, at least
    if (!ApplyProfile(config.default_idx))
        return 1;

    // Run main message loop
    for (;;) {
        DWORD ret = MsgWaitForMultipleObjects(1, &toggle, FALSE, INFINITE, QS_ALLINPUT);

        if (ret == WAIT_OBJECT_0) {
            if (!ToggleProfile(1))
                return 1;
            ResetEvent(toggle);
        } else if (ret == WAIT_OBJECT_0 + 1) {
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT)
                    return (int)msg.wParam;

                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            LogError("Failed in Win32 wait loop: %1", GetWin32ErrorString());
            return 1;
        }
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }

#endif
