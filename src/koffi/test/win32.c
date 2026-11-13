// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include <stdlib.h>
#include <windows.h>

#define IMPORT __declspec(dllimport)
#define EXPORT __declspec(dllexport)

#ifndef PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY
    #define PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY 0x00020007
#endif
#ifndef PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE
    #define PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE 0x04
#endif

IMPORT int __cdecl DoDivideBySafe1(int a, int b);
IMPORT int __cdecl DoDivideBySafe2(int a, int b);
IMPORT void __cdecl DoSetCrashFilter(void);

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

// Returns the exit code of the child, -1 if SEHOP cannot be forced, -2 on spawn failure, -3 on timeout
EXPORT int RunWithSehop(const wchar_t *cmdline)
{
    int ret = -2;

    STARTUPINFOEXW si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    SIZE_T attr_size = 0;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE nul_in = INVALID_HANDLE_VALUE;
    HANDLE out = INVALID_HANDLE_VALUE;
    wchar_t tmp_dir[MAX_PATH], tmp_file[MAX_PATH];
    HANDLE inherit[2];
    DWORD64 policy = PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE;
    wchar_t *cmd = _wcsdup(cmdline);

    if (!cmd)
        return -2;

    // The child only inherits NUL for stdin and a temporary file for stdout/stderr
    nul_in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    if (nul_in == INVALID_HANDLE_VALUE)
        goto cleanup;
    if (!GetTempPathW(MAX_PATH, tmp_dir) || !GetTempFileNameW(tmp_dir, L"kof", 0, tmp_file))
        goto cleanup;
    out = CreateFileW(tmp_file, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa,
                      CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (out == INVALID_HANDLE_VALUE)
        goto cleanup;
    inherit[0] = nul_in;
    inherit[1] = out;

    InitializeProcThreadAttributeList(NULL, 2, 0, &attr_size);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attr_size);
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = nul_in;
    si.StartupInfo.hStdOutput = out;
    si.StartupInfo.hStdError = out;

    if (!si.lpAttributeList || !InitializeProcThreadAttributeList(si.lpAttributeList, 2, 0, &attr_size))
        goto cleanup;
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                                   &policy, sizeof(policy), NULL, NULL)) {
        ret = -1;
        goto cleanup2;
    }
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   inherit, sizeof(inherit), NULL, NULL))
        goto cleanup2;

    if (CreateProcessW(NULL, cmd, NULL, NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT,
                       NULL, NULL, &si.StartupInfo, &pi)) {
        DWORD code = 0;

        if (WaitForSingleObject(pi.hProcess, 60000) == WAIT_OBJECT_0) {
            GetExitCodeProcess(pi.hProcess, &code);
            ret = (int)code;
        } else {
            TerminateProcess(pi.hProcess, 1);
            ret = -3;
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

cleanup2:
    DeleteProcThreadAttributeList(si.lpAttributeList);
cleanup:
    if (nul_in != INVALID_HANDLE_VALUE)
        CloseHandle(nul_in);
    if (out != INVALID_HANDLE_VALUE)
        CloseHandle(out);
    free(si.lpAttributeList);
    free(cmd);

    return ret;
}
