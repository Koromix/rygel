// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if defined(_WIN32)

#include "src/core/native/base/base.hh"
#include "src/core/native/gui/tray.hh"
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

namespace K {

extern "C" const AssetInfo MeesticPng;

#define WM_APP_REHOOK (WM_APP + 1)
#define WM_APP_TOGGLE (WM_APP + 2)

static Config config;
static Size active_idx = 0;

static hs_port *port = nullptr;

static HWND hwnd;
static HHOOK hook;
static std::unique_ptr<gui_TrayIcon> tray;

static void ShowDialog(const char *text)
{
    HINSTANCE module = GetModuleHandle(nullptr);

    TASKDIALOGCONFIG dialog = {};

    wchar_t title[1024]; ConvertUtf8ToWin32Wide(FelixTarget, title);
    wchar_t main[1024]; swprintf(main, K_LEN(main), L"%hs %hs", FelixTarget, FelixVersion);
    wchar_t content[2048]; ConvertUtf8ToWin32Wide(text, content);

    dialog.cbSize = K_SIZE(dialog);
    dialog.hwndParent = hwnd;
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
            PostMessageW(hwnd, TDM_CLICK_BUTTON, IDOK, 0);
        }

        return (HRESULT)S_OK;
    };

    TaskDialogIndirect(&dialog, nullptr, nullptr, nullptr);
}

static void ShowAboutDialog()
{
    const char *text = R"(<a href="https://koromix.dev/meestic">https://koromix.dev/</a>)";
    ShowDialog(text);
}

static bool ApplyProfile(Size idx)
{
    LogInfo("Applying profile %1", idx);

    if (port) {
        // Should work first time...
        {
            PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
            K_DEFER { PopLogFilter(); };

            if (ApplyLight(port, config.profiles[idx].settings)) {
                active_idx = idx;
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

    active_idx = idx;

    return true;
}

static bool ToggleProfile(int delta)
{
    if (!delta)
        return true;

    Size next_idx = active_idx;

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

static LRESULT __stdcall LowLevelKeyboardProc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == 0) {
        const KBDLLHOOKSTRUCT *kbd = (const KBDLLHOOKSTRUCT *)lparam;

        if (wparam == WM_KEYDOWN && kbd->vkCode == 255 && kbd->scanCode == 14) {
            PostMessageA(hwnd, WM_APP_TOGGLE, 0, 0);
        }
    }

    return CallNextHookEx(hook, code, wparam, lparam);
}

static LRESULT __stdcall MainWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    UINT msg_or_timer = (msg != WM_TIMER) ? msg : (UINT)wparam;

    switch (msg_or_timer) {
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

        case WM_APP_TOGGLE: {
            ToggleProfile(1);
        } break;

        case WM_CLOSE: {
            PostQuitMessage(0);
            return 0;
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
            Size len = ConvertUtf8ToWin32Wide(ctx, buf_w.Take(0, K_LEN(buf_w.data) - 2));
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

static void UpdateTray()
{
    tray->ClearMenu();

    for (Size i = 0; i < config.profiles.count; i++) {
        const ConfigProfile &profile = config.profiles[i];
        tray->AddAction(profile.name, i == active_idx, [i]() { ApplyProfile(i); });
    }

    tray->AddSeparator();
    tray->AddAction(T("&About"), ShowAboutDialog);
    tray->AddSeparator();
    tray->AddAction(T("&Exit"), []() { PostMessageA(hwnd, WM_CLOSE, 0, 0); });
}

int Main(int argc, char **argv)
{
    InitLocales(TranslationTables);

    BlockAllocator temp_alloc;

    InitCommonControls();

    // Use message boxes when /subsystem:windows is used
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN) {
        RedirectErrors();
    }

    // Default config filename
    HeapArray<const char *> config_filenames;
    const char *config_filename = FindConfigFile({ "MeesticTray.ini", "MeesticGui.ini" }, &temp_alloc, &config_filenames);

    const auto print_usage = [=]() {
        HeapArray<char> help;

        Fmt(&help,
T(R"(Usage: %!..+%1 [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

By default, the first of the following config files will be used:

)"),
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
                    config_filename = Fmt(&temp_alloc, "%1%/meestic.ini", TrimStrRight(opt.current_value, K_PATH_SEPARATORS)).ptr;
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

        active_idx = config.default_idx;
    } else {
        AddDefaultProfiles(&config);
    }

    // Open the light MSI HID device ahead of time
    if (!GetDebugFlag("FAKE_LIGHTS")) {
        port = OpenLightDevice();
        if (!port)
            return 1;
    }
    K_DEFER { CloseLightDevice(port); };

    HINSTANCE module = GetModuleHandle(nullptr);
    const char *cls_name = FelixTarget;
    const char *win_name = FelixTarget;

    // Register window class
    {
        WNDCLASSEXA wc = {};

        wc.cbSize = K_SIZE(wc);
        wc.hInstance = module;
        wc.lpszClassName = cls_name;
        wc.lpfnWndProc = MainWindowProc;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        if (!RegisterClassExA(&wc)) {
            LogError("Failed to register window class '%1': %2", cls_name, GetWin32ErrorString());
            return 1;
        }
    }
    K_DEFER { UnregisterClassA(cls_name, module); };

    // Create hidden window
    hwnd = CreateWindowExA(0, cls_name, win_name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, module, nullptr);
    if (!hwnd) {
        LogError("Failed to create window named '%1': %2", win_name, GetLastError(), GetWin32ErrorString());
        return 1;
    }
    K_DEFER { DestroyWindow(hwnd); };

    // We want to intercept Fn+F8, and this is not possible with RegisterHotKey because
    // it is not mapped to a virtual key. We want the raw scan code.
    hook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
    if (!hook) {
        LogError("Failed to insert low-level keyboard hook: %1", GetWin32ErrorString());
        return 1;
    }
    K_DEFER { if (hook) UnhookWindowsHookEx(hook); };

    // Unfortunately, Windows sometimes disconnects our hook for no good reason
    if (!SetTimer(hwnd, WM_APP_REHOOK, 30000, nullptr)) {
        LogError("Failed to create Win32 timer: %1", GetWin32ErrorString());
        return 1;
    }
    K_DEFER { KillTimer(hwnd, WM_APP_REHOOK); };

    K_ASSERT(MeesticPng.compression_type == CompressionType::None);

    tray = gui_CreateTrayIcon(MeesticPng.data);
    if (!tray)
        return 1;
    tray->OnContext(UpdateTray);

    // Check that it works once, at least
    if (!ApplyProfile(config.default_idx))
        return 1;

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }

#endif
