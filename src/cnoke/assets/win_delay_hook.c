// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include <stdlib.h>
#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <delayimp.h>

static HMODULE node_dll;

static FARPROC WINAPI self_exe_hook(unsigned int event, DelayLoadInfo *info)
{
    if (event == dliStartProcessing) {
        node_dll = GetModuleHandleA("node.dll");
        if (!node_dll) {
            node_dll = GetModuleHandle(NULL);
        }
        return NULL;
    }

    if (event == dliNotePreLoadLibrary && !stricmp(info->szDll, "node.exe"))
        return (FARPROC)node_dll;

    return NULL;
}

#if defined(__MINGW32__)
PfnDliHook __pfnDliNotifyHook2 = self_exe_hook;
#else
const PfnDliHook __pfnDliNotifyHook2 = self_exe_hook;
#endif
