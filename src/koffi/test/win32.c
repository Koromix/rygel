// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#if !defined(PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY)
    #define PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY 0x00020007
#endif
#if !defined(PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE)
    #define PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE 0x04
#endif

#define IMPORT __declspec(dllimport)
#define EXPORT __declspec(dllexport)

IMPORT int __cdecl DoDivideBySafe1(int a, int b);
IMPORT int __cdecl DoDivideBySafe2(int a, int b);
IMPORT void __cdecl DoSetCrashFilter(void);

// Returns the exit code of the child, -1 if SEHOP cannot be forced, -2 on spawn failure
EXPORT bool ExecuteWithSehop(wchar_t *cmd, int *out_code)
{
    bool success = false;

    _Alignas(16) uint8_t attributes[2048];
    STARTUPINFOEXW si = { { .cb = sizeof(si) } };
    PROCESS_INFORMATION pi = {};
    uint64_t policy = PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE;

    // Prepare process/thread attributes
    {
        size_t size = 0;

        InitializeProcThreadAttributeList(NULL, 1, 0, &size);
        assert(size <= sizeof(attributes));

        LPPROC_THREAD_ATTRIBUTE_LIST list = (LPPROC_THREAD_ATTRIBUTE_LIST)attributes;
        if (!InitializeProcThreadAttributeList(list, 1, 0, &size))
            goto exit;
        si.lpAttributeList = list;

        if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                                       &policy, sizeof(policy), NULL, NULL))
            goto exit;
    }

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi))
        goto exit;

    // Wait for process to end
    {
        DWORD code = (DWORD)-2;

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);

        *out_code = (int)code;
    }

    success = true;

exit:
    if (pi.hThread) {
        CloseHandle(pi.hThread);
    }
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
    }
    if (si.lpAttributeList) {
        DeleteProcThreadAttributeList(si.lpAttributeList);
    }

    return success;
}

EXPORT int DivideBySafe1(int a, int b)
{
    return DoDivideBySafe1(a, b);
}

EXPORT int DivideBySafe2(int a, int b)
{
    return DoDivideBySafe2(a, b);
}

EXPORT int CallThrough(int (__stdcall *func)(int value), int value)
{
    int ret = func(value);
    return ret;
}

EXPORT void SetCrashFilter(void)
{
    DoSetCrashFilter();
}

EXPORT void CrashNow(void)
{
    *(volatile int *)0 = 42;
}
