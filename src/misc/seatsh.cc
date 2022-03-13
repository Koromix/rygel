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

// Install service (from admin command): sc create binpath="C:\seatsh.exe" obj=".\LocalSystem" password=""

#include "src/core/libcc/libcc.hh"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wtsapi32.h>

namespace RG {

// Client

static int RunClient(int argc, char **argv)
{
    // Options
    Span<const char> cmd = nullptr;
    Span<const char> work_dir = GetWorkingDirectory();

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 [options] <command>%!0

Options:
    %!..+-w, --work_dir <dir>%!0   Change working directory

In order for this to work, you must first install the service from an elevated command prompt:
%!D..+sc create SeatSH binPath= "%2" obj= LocalSystem password= "")", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-w", "--work_dir", OptionType::Value)) {
                work_dir = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        cmd = opt.ConsumeNonOption();
        if (!cmd.len) {
            LogError("No command provided");
            return 1;
        }
    }

    // Assemble message
    LocalArray<char, 8192> msg;
    if (cmd.len + work_dir.len > RG_SIZE(msg.data) - 2) {
        LogError("Excessive command or working directory length");
        return 1;
    }
    msg.len = Fmt(msg.data, "%1%2%3", cmd, '\0', work_dir).len + 1;

    DWORD exit_code = 1;
    DWORD read = 0;

    if (!CallNamedPipeA("\\\\.\\pipe\\SeatSH", msg.data, msg.len,
                        &exit_code, RG_SIZE(exit_code), &read, NMPWAIT_WAIT_FOREVER)) {
        LogError("Failed to call SeatSH service: %1", GetWin32ErrorString());
        return 1;
    }
    RG_CRITICAL(read == 4);

    return (int)exit_code;
}

// Server (service)

static SERVICE_STATUS_HANDLE status_handle = nullptr;
static int current_state = 0;
static int current_error = 0;
static HANDLE stop_event = nullptr;

static void ReportStatus(int state)
{
    if (current_error) {
        state = SERVICE_STOPPED;
    }
    current_state = state;

    SERVICE_STATUS status = {};

    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = (DWORD)state;
    status.dwControlsAccepted = (state == SERVICE_START_PENDING) ? 0 : (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
    status.dwWin32ExitCode = current_error ? ERROR_SERVICE_SPECIFIC_ERROR : NO_ERROR;
    status.dwServiceSpecificExitCode = (DWORD)current_error;

    SetServiceStatus(status_handle, &status);
    LogInfo("Reported status: %1", FmtHex(state));
}

static void ReportError(int error)
{
    current_error = error;

    ReportStatus(SERVICE_STOPPED);
    SetEvent(stop_event);
}

static DWORD WINAPI ServiceHandler(DWORD ctrl, DWORD event, void *data, void *ctx)
{
    switch (ctrl) {
        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP: {
            ReportStatus(SERVICE_STOP_PENDING);
            SetEvent(stop_event);
            return NO_ERROR;
        } break;

        case SERVICE_CONTROL_INTERROGATE: {
            ReportStatus(current_state);
        } break;
    }

    return ERROR_CALL_NOT_IMPLEMENTED;
}

static DWORD WINAPI RunPipeThread(void *pipe)
{
    char buf[8192];
    OVERLAPPED ov = {};
    DWORD len;

    if (!::ReadFile(pipe, buf, sizeof(buf) - 1, nullptr, &ov) && GetLastError() != ERROR_IO_PENDING) {
        LogError("Failed to read from named pipe: %1", GetWin32ErrorString());
        ReportError(10);
        return 1;
    }
    if (!GetOverlappedResult(pipe, &ov, &len, TRUE)) {
        LogError("Failed to read from named pipe: %1", GetWin32ErrorString());
        ReportError(11);
        return 1;
    }
    buf[len] = 0;

    char *cmd = buf;
    char *cwd = buf + strlen(cmd) + 1;

    DWORD sid = WTSGetActiveConsoleSessionId();
    if (sid == UINT32_MAX) {
        LogError("Failed to get active control session ID: %1", GetWin32ErrorString());
        ReportError(12);
        return 1;
    }

    HANDLE token;
    if (!WTSQueryUserToken(sid, &token)) {
        LogError("Failed to query active session user token: %1", GetWin32ErrorString());
        ReportError(13);
        return 1;
    }
    RG_DEFER { CloseHandle(token); };

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    DWORD exit_code = 128;

    si.cb = RG_SIZE(si);
    si.lpDesktop = (char *)"winsta0\\default";

    if (!CreateProcessAsUserA(token, nullptr, cmd, nullptr, nullptr, TRUE, 0, nullptr, cwd, &si, &pi)) {
        LogError("Failed to create process on desktop session: %1", GetWin32ErrorString());
        ReportError(14);
        return 1;
    }
    RG_DEFER {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    };
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        LogError("Failed to get process exit code: %1", GetWin32ErrorString());
        ReportError(15);
        return 1;
    }

    // Send exit code to client
    if (!::WriteFile(pipe, &exit_code, sizeof(exit_code), &len, nullptr)) {
        LogError("Failed to send process exit code to client: %1", GetWin32ErrorString());
        ReportError(16);
        return 1;
    }

    return 0;
}

static void WINAPI RunService(DWORD argc, char **argv)
{
    static StreamWriter logger("C:\\log");

    SetLogHandler([](LogLevel level, const char *ctx, const char *msg) {
        switch (level)  {
            case LogLevel::Debug:
            case LogLevel::Info: { PrintLn(&logger, "%!D..%1%2%!0%3", ctx ? ctx : "", ctx ? ": " : "", msg); } break;
            case LogLevel::Warning: { PrintLn(&logger, "%!M..%1%2%!0%3", ctx ? ctx : "", ctx ? ": " : "", msg); } break;
            case LogLevel::Error: { PrintLn(&logger, "%!R..%1%2%!0%3", ctx ? ctx : "", ctx ? ": " : "", msg); } break;
        }

        logger.Flush();
    });

    status_handle = RegisterServiceCtrlHandlerExA("SeatSH", &ServiceHandler, nullptr);
    RG_CRITICAL(status_handle, "Failed to register service controller: %1", GetWin32ErrorString());

    ReportStatus(SERVICE_START_PENDING);

    HANDLE connect_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!connect_event || !stop_event) {
        LogError("Failed to create event: %1", GetWin32ErrorString());
        ReportError(1);
        return;
    }

    // Open for everyone!
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES sa = {};
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);
    sa.nLength = RG_SIZE(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    ReportStatus(SERVICE_RUNNING);

    for (;;) {
        HANDLE pipe = CreateNamedPipeA("\\\\.\\pipe\\SeatSH", PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                       PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                       PIPE_UNLIMITED_INSTANCES, 8192, 8192, 0, &sa);
        if (pipe == INVALID_HANDLE_VALUE) {
            LogError("Failed to create main named pipe: %1", GetWin32ErrorString());
            ReportError(2);
            return;
        }
        RG_DEFER_N(pipe_guard) {
            CancelIo(pipe);
            CloseHandle(pipe);
        };

        OVERLAPPED ov = {};
        ov.hEvent = connect_event;

        if (!ConnectNamedPipe(pipe, &ov) && GetLastError() != ERROR_IO_PENDING) {
            LogError("Failed to connect named pipe: %1", GetWin32ErrorString());
            ReportError(3);
            return;
        }

        HANDLE events[] = {
            connect_event,
            stop_event
        };
        DWORD ret = WaitForMultipleObjects(RG_LEN(events), events, FALSE, INFINITE);

        if (ret == WAIT_OBJECT_0) {
            DWORD dummy;
            if (!GetOverlappedResult(pipe, &ov, &dummy, TRUE)) {
                LogError("Failed to connect named pipe: %1", GetWin32ErrorString());
                ReportError(4);
                return;
            }

            HANDLE thread = CreateThread(nullptr, 0, RunPipeThread, pipe, 0, nullptr);
            if (!thread) {
                LogError("Failed to create new thread: %1", GetWin32ErrorString());
                ReportError(5);
                return;
            }
            CloseHandle(thread);
            pipe_guard.Disable();
        } else if (ret == WAIT_OBJECT_0 + 1) {
            break;
        } else {
            LogError("WaitForMultipleObjects() failed: %1", GetWin32ErrorString());
            ReportError(6);
            return;
        }
    }

    ReportStatus(SERVICE_STOP_PENDING);
    ReportStatus(SERVICE_STOPPED);
}

int Main(int argc, char **argv)
{
    SERVICE_TABLE_ENTRYA services[] = {
        {(char *)"SeatSH", (LPSERVICE_MAIN_FUNCTIONA)RunService},
        {}
    };

    if (StartServiceCtrlDispatcherA(services)) {
        return 0; // Run service
    } else if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
        return RunClient(argc, argv);
    } else {
        LogError("Failed to connect to service control manager: %1", GetWin32ErrorString());
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
