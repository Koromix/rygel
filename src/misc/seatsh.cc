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

#include "src/core/libcc/libcc.hh"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wtsapi32.h>
#include <sddl.h>

namespace RG {

// Client

static int RunClient(int argc, char **argv)
{
    DWORD dummy;

    // Options
    Span<const char> cmd = nullptr;
    Span<const char> work_dir = GetWorkingDirectory();

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 [options] <command>%!0

Options:
    %!..+-w, --work_dir <dir>%!0   Change working directory

In order for this to work, you must first install the service from an elevated command prompt:
%!..+sc create SeatSH start= auto binPath= "%2" obj= LocalSystem password= ""%!0)", FelixTarget, GetApplicationExecutable());
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
                return 127;
            }
        }

        cmd = opt.ConsumeNonOption();
        if (!cmd.len) {
            LogError("No command provided");
            return 127;
        }
    }

    // Contact SeatSH service
    HANDLE pipe = CreateFileA("\\\\.\\pipe\\SeatSH", GENERIC_READ | GENERIC_WRITE,
                              0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            LogError("SeatSH service does not seem to be running");
        } else {
            LogError("Failed to call SeatSH service: %1", GetWin32ErrorString());
        }
        return 127;
    }
    RG_DEFER { CloseHandle(pipe); };

    // We want messages, not bytes
    {
        DWORD mode = PIPE_READMODE_MESSAGE;

        if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
            LogError("Failed to switch pipe to message mode: %1", GetWin32ErrorString());
            return 127;
        }
    }

    // Assemble message for SeatSH
    LocalArray<char, 8192> msg;
    if (cmd.len + work_dir.len > RG_SIZE(msg.data) - 2) {
        LogError("Excessive command or working directory length");
        return 127;
    }
    msg.len = Fmt(msg.data, "%1%2%3", cmd, '\0', work_dir).len + 1;

    // Ask SeatSH nicely
    {
        if (!::WriteFile(pipe, msg.data, msg.len, &dummy, nullptr)) {

        }
    }

    int exit_code = 127;

    // Read relayed output from SeatSH service
    for (;;) {
        uint8_t buf[8192];
        DWORD buf_len;

        if (!::ReadFile(pipe, buf, RG_SIZE(buf), &buf_len, nullptr)) {
            LogError("Failed to read from SeatSH: %1", GetWin32ErrorString());
            return 127;
        }

        if (buf_len <= 0)
            goto malformed;

        if (buf[0] == 0) { // exit
            if (buf_len != 5)
                goto malformed;

            exit_code = *(int *)(buf + 1);
            break;
        } else if (buf[0] == 1) { // error
            Span<const char> str = MakeSpan((const char *)buf + 1, buf_len - 1);
            LogError("%1", str);

            break;
        } else if (buf[0] == 2) { // stdout
            fwrite(buf + 1, 1, buf_len - 1, stdout);
        } else if (buf[0] == 3) { // stderr
            fwrite(buf + 1, 1, buf_len - 1, stderr);
        } else {
            goto malformed;
        }

        continue;

malformed:
        LogError("Malformed message from SeatSH service");
        return 127;
    }

    return (int)exit_code;
}

// Server (service)

static SERVICE_STATUS_HANDLE status_handle = nullptr;
static int current_state = 0;
static int current_error = 0;
static HANDLE stop_event = nullptr;
static std::atomic_uint client_counter = 0;

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
}

static void ReportError(int error)
{
    RG_ASSERT(error > 0);

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

static HANDLE GetClientToken(HANDLE pipe)
{
    if (!ImpersonateNamedPipeClient(pipe)) {
        LogError("Failed to get pipe client information: %1", GetWin32ErrorString());
        return nullptr;
    }
    RG_DEFER { RevertToSelf(); };

    HANDLE token;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_READ, FALSE, &token)) {
        LogError("Failed to get pipe client information: %1", GetWin32ErrorString());
        return nullptr;
    }

    return token;
}

static bool GetTokenUser(HANDLE token, PSID *out_sid)
{
    union {
        TOKEN_USER user;
        char raw[1024];
    } buf;
    DWORD size;

    if (!GetTokenInformation(token, TokenUser, &buf, RG_SIZE(buf), &size)) {
        LogError("Failed to get token user information: %1", GetWin32ErrorString());
        return false;
    }
    *out_sid = (PSID)buf.user.User.Sid;

    return true;
}

static bool MatchUsers(HANDLE token1, HANDLE token2)
{
    PSID sid1;
    PSID sid2;
    if (!GetTokenUser(token1, &sid1))
        return false;
    if (!GetTokenUser(token2, &sid2))
        return false;

    return EqualSid(sid1, sid2);
}

static DWORD WINAPI RunPipeThread(void *pipe)
{
    DWORD dummy;

    RG_DEFER { CloseHandle(pipe); };

    unsigned int client = ++client_counter;

    char err_buf[8192];
    CopyString("Unknown error", MakeSpan(err_buf + 1, RG_SIZE(err_buf) - 1));
    err_buf[0] = 1;

    // If something fails (command does not exist, etc), send it to the client
    RG_DEFER_N(err_guard) { ::WriteFile(pipe, err_buf, strlen(err_buf), &dummy, nullptr); };

    PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        char ctx_buf[1024];
        Fmt(ctx_buf, "Client %1%2%3", client, ctx ? ": " : "", ctx ? ctx : "");

        if (level == LogLevel::Error) {
            CopyString(msg, MakeSpan(err_buf + 1, RG_SIZE(err_buf) - 1));
        };

        func(level, ctx_buf, msg);
    });
    RG_DEFER { PopLogFilter(); };


    char buf[8192];
    OVERLAPPED ov = {};
    DWORD len;

    if (!::ReadFile(pipe, buf, RG_SIZE(buf) - 1, nullptr, &ov) && GetLastError() != ERROR_IO_PENDING) {
        LogError("Failed to read from named pipe: %1", GetWin32ErrorString());
        return 1;
    }
    if (!GetOverlappedResult(pipe, &ov, &len, TRUE)) {
        LogError("Failed to read from named pipe: %1", GetWin32ErrorString());
        return 1;
    }
    buf[len] = 0;

    // Security checks: same user?

    char *cmd = buf;
    char *work_dir = buf + strlen(cmd) + 1;

    LogInfo("Executing '%1' in '%2'", cmd, work_dir);

    HANDLE client_token = GetClientToken(pipe);
    RG_DEFER { CloseHandle(client_token); };

    HANDLE console_token;
    {
        DWORD sid = WTSGetActiveConsoleSessionId();

        if (sid == UINT32_MAX) {
            LogError("Failed to get active control session ID: %1", GetWin32ErrorString());
            return 1;
        }
        if (!WTSQueryUserToken(sid, &console_token)) {
            LogError("Failed to query active session user token: %1", GetWin32ErrorString());
            return 1;
        }
    }
    RG_DEFER { CloseHandle(console_token); };

    if (!MatchUsers(client_token, console_token)) {
        LogError("SeatSH refuses to do cross-user launches");
        return 1;
    }

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};

    // Basic startup information
    si.cb = RG_SIZE(si);
    si.lpDesktop = (char *)"winsta0\\default";
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Prepare stdout and stderr redirection pipes
    HANDLE in_pipe[2] = {};
    HANDLE out_pipe[2] = {};
    HANDLE err_pipe[2] = {};
    RG_DEFER {
        CloseHandleSafe(&in_pipe[0]);
        CloseHandleSafe(&in_pipe[1]);
        CloseHandleSafe(&out_pipe[0]);
        CloseHandleSafe(&out_pipe[1]);
        CloseHandleSafe(&err_pipe[0]);
        CloseHandleSafe(&err_pipe[1]);
    };
    if (!CreateOverlappedPipe(false, true, in_pipe))
        return 1;
    if (!CreateOverlappedPipe(true, false, out_pipe))
        return 1;
    if (!CreateOverlappedPipe(true, false, err_pipe))
        return 1;

    // Launch process with our redirections
    {
        RG_DEFER {
            CloseHandleSafe(&si.hStdInput);
            CloseHandleSafe(&si.hStdOutput);
            CloseHandleSafe(&si.hStdError);
        };

        if (!DuplicateHandle(GetCurrentProcess(), in_pipe[0],
                             GetCurrentProcess(), &si.hStdInput, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            LogError("Failed to duplicate handle: %1", GetWin32ErrorString());
            return 1;
        }
        if (!DuplicateHandle(GetCurrentProcess(), out_pipe[1],
                             GetCurrentProcess(), &si.hStdOutput, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            LogError("Failed to duplicate handle: %1", GetWin32ErrorString());
            return 1;
        }
        if (!DuplicateHandle(GetCurrentProcess(), err_pipe[1],
                             GetCurrentProcess(), &si.hStdError, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            LogError("Failed to duplicate handle: %1", GetWin32ErrorString());
            return 1;
        }

        // Launch process
        if (!CreateProcessAsUserA(console_token, nullptr, cmd, nullptr, nullptr, TRUE, 0, nullptr, work_dir, &si, &pi)) {
            LogError("Failed to start process: %1", GetWin32ErrorString());
            return 1;
        }

        CloseHandleSafe(&in_pipe[0]);
        CloseHandleSafe(&out_pipe[1]);
        CloseHandleSafe(&err_pipe[1]);
    }
    RG_DEFER {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    };

    // Forward stdout and stderr to client
    {
        HANDLE events[] = {
            CreateEvent(nullptr, TRUE, TRUE, nullptr),
            CreateEvent(nullptr, TRUE, TRUE, nullptr),
            pi.hProcess
        };
        RG_DEFER {
            CloseHandleSafe(&events[0]);
            CloseHandleSafe(&events[1]);
        };
        if (!events[0] || !events[1]) {
            LogError("Failed to create event HANDLE: %1", GetWin32ErrorString());
            return 1;
        }

        uint8_t stdout_buf[8192];
        bool stdout_pending = false;
        OVERLAPPED stdout_ov = {};
        stdout_ov.hEvent = events[0];

        uint8_t stderr_buf[8192];
        bool stderr_pending = false;
        OVERLAPPED stderr_ov = {};
        stderr_ov.hEvent = events[1];

        do {
            DWORD ret = WaitForMultipleObjects(RG_LEN(events), events, FALSE, INFINITE);

            if (RG_UNLIKELY(ret < WAIT_OBJECT_0 || ret >= WAIT_OBJECT_0 + RG_LEN(events))) {
                // Not sure how this could happen, but who knows?
                LogError("Read/write for process failed: %1", GetWin32ErrorString());
                break;
            }

            // Try to read from stdout
            if (ret == WAIT_OBJECT_0) {
                if (stdout_pending) {
                    DWORD out_len = 0;

                    if (GetOverlappedResult(out_pipe[0], &stdout_ov, &out_len, TRUE)) {
                        stdout_buf[0] = 2;

                        if (::WriteFile(pipe, stdout_buf, out_len + 1, &dummy, nullptr)) {
                            stdout_pending = false;
                        } else {
                            LogError("Failed to relay stdout to client: %1", GetWin32ErrorString());

                            CancelIo(out_pipe[0]);
                            CloseHandleSafe(&out_pipe[0]);
                            ResetEvent(events[0]);
                        }
                    } else {
                        DWORD err = GetLastError();

                        if (err != ERROR_BROKEN_PIPE && err != ERROR_PIPE_NOT_CONNECTED && err != ERROR_NO_DATA) {
                            LogError("Failed to read process output: %1", GetWin32ErrorString(err));
                        }

                        CancelIo(out_pipe[0]);
                        CloseHandleSafe(&out_pipe[0]);
                        ResetEvent(events[0]);
                    }
                }

                if (RG_LIKELY(out_pipe[0])) {
                    DWORD out_len = 0;

                    while (::ReadFile(out_pipe[0], stdout_buf + 1, RG_SIZE(stdout_buf) - 1, nullptr, &stdout_ov) &&
                           GetOverlappedResult(out_pipe[0], &stdout_ov, &out_len, FALSE)) {
                        stdout_buf[0] = 2;

                        if (!::WriteFile(pipe, stdout_buf, out_len + 1, &dummy, nullptr)) {
                            LogError("Failed to relay stdout to client: %1", GetWin32ErrorString());

                            CancelIo(out_pipe[0]);
                            CloseHandleSafe(&out_pipe[0]);
                            ResetEvent(events[0]);

                            SetLastError(ERROR_IO_PENDING);
                        }
                    }

                    DWORD err = GetLastError();

                    if (err == ERROR_IO_PENDING) {
                        stdout_pending = true;
                    } else {
                        if (err != ERROR_BROKEN_PIPE && err != ERROR_PIPE_NOT_CONNECTED && err != ERROR_NO_DATA) {
                            LogError("Failed to read process output: %1", GetWin32ErrorString(err));
                        }

                        CancelIo(out_pipe[0]);
                        CloseHandleSafe(&out_pipe[0]);
                        ResetEvent(events[0]);
                    }
                }
            }

            // Try to read from stderr
            if (ret == WAIT_OBJECT_0 + 1) {
                if (stderr_pending) {
                    DWORD err_len = 0;

                    if (GetOverlappedResult(err_pipe[0], &stderr_ov, &err_len, TRUE)) {
                        stderr_buf[0] = 3;

                        if (::WriteFile(pipe, stderr_buf, err_len + 1, &dummy, nullptr)) {
                            stderr_pending = false;
                        } else {
                            LogError("Failed to relay stderr to client: %1", GetWin32ErrorString());

                            CancelIo(err_pipe[0]);
                            CloseHandleSafe(&err_pipe[0]);
                            ResetEvent(events[1]);
                        }
                    } else {
                        DWORD err = GetLastError();

                        if (err != ERROR_BROKEN_PIPE && err != ERROR_PIPE_NOT_CONNECTED && err != ERROR_NO_DATA) {
                            LogError("Failed to read process output: %1", GetWin32ErrorString(err));
                        }

                        CancelIo(err_pipe[0]);
                        CloseHandleSafe(&err_pipe[0]);
                        ResetEvent(events[1]);
                    }
                }

                if (RG_LIKELY(err_pipe[0])) {
                    DWORD err_len = 0;

                    while (::ReadFile(err_pipe[0], stderr_buf + 1, RG_SIZE(stderr_buf) - 1, nullptr, &stderr_ov) &&
                           GetOverlappedResult(err_pipe[0], &stderr_ov, &err_len, FALSE)) {
                        stderr_buf[0] = 3;

                        if (!::WriteFile(pipe, stderr_buf, err_len + 1, &dummy, nullptr)) {
                            LogError("Failed to relay stderr to client: %1", GetWin32ErrorString());

                            CancelIo(err_pipe[0]);
                            CloseHandleSafe(&err_pipe[0]);
                            ResetEvent(events[1]);

                            SetLastError(ERROR_IO_PENDING);
                        }
                    }

                    DWORD err = GetLastError();

                    if (err == ERROR_IO_PENDING) {
                        stderr_pending = true;
                    } else {
                        if (err != ERROR_BROKEN_PIPE && err != ERROR_PIPE_NOT_CONNECTED && err != ERROR_NO_DATA) {
                            LogError("Failed to read process output: %1", GetWin32ErrorString(err));
                        }

                        CancelIo(err_pipe[0]);
                        CloseHandleSafe(&err_pipe[0]);
                        ResetEvent(events[1]);
                    }
                }
            }

            if (ret == WAIT_OBJECT_0 + 2) {
                if (out_pipe[0]) {
                    CancelIo(out_pipe[0]);
                }
                if (err_pipe[0]) {
                    CancelIo(err_pipe[0]);
                }

                break;
            }
        } while (out_pipe[0] || err_pipe[0]);
    }

    // Wait for process exit and get exit code
    DWORD exit_code;
    if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED) {
        LogError("WaitForSingleObject() failed: %1", GetWin32ErrorString());
        return 1;
    }
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        LogError("GetExitCodeProcess() failed: %1", GetWin32ErrorString());
        return 1;
    }

    // Send exit code to client
    {
        uint8_t buf[5];

        buf[0] = 0;
        memcpy(buf + 1, &exit_code, 4);

        if (!::WriteFile(pipe, &buf, RG_SIZE(buf), &len, nullptr)) {
            LogError("Failed to send process exit code to client: %1", GetWin32ErrorString());
            return 1;
        }
        err_guard.Disable();
    }

    return 0;
}

static void WINAPI RunService(DWORD argc, char **argv)
{
    DWORD dummy;

    HANDLE log = OpenEventLogA(nullptr, "SeatSH");
    if (!log) {
        LogError("Failed to register event provider: %1", GetWin32ErrorString());
        ReportError(__LINE__);
        return;
    }
    RG_DEFER { CloseEventLog(log); };

    // Redirect log to Win32 stuff
    SetLogHandler([&](LogLevel level, const char *ctx, const char *msg) {
        const char *strings[] = {
            ctx,
            msg
        };

        switch (level)  {
            case LogLevel::Debug: { ReportEventA(log, EVENTLOG_INFORMATION_TYPE, 0, 0, nullptr, 2, 0, strings, nullptr); } break;
            case LogLevel::Info: { ReportEventA(log, EVENTLOG_INFORMATION_TYPE, 0, 0, nullptr, 2, 0, strings, nullptr); } break;
            case LogLevel::Warning: { ReportEventA(log, EVENTLOG_WARNING_TYPE, 0, 0, nullptr, 2, 0, strings, nullptr); } break;
            case LogLevel::Error: { ReportEventA(log, EVENTLOG_ERROR_TYPE, 0, 0, nullptr, 2, 0, strings, nullptr); } break;
        }
    });

    // Register our service controller
    status_handle = RegisterServiceCtrlHandlerExA("SeatSH", &ServiceHandler, nullptr);
    RG_CRITICAL(status_handle, "Failed to register service controller: %1", GetWin32ErrorString());

    ReportStatus(SERVICE_START_PENDING);

    HANDLE connect_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!connect_event || !stop_event) {
        LogError("Failed to create event: %1", GetWin32ErrorString());
        ReportError(__LINE__);
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
            ReportError(__LINE__);
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
            ReportError(__LINE__);
            return;
        }

        HANDLE events[] = {
            connect_event,
            stop_event
        };
        DWORD ret = WaitForMultipleObjects(RG_LEN(events), events, FALSE, INFINITE);

        if (ret == WAIT_OBJECT_0) {
            if (!GetOverlappedResult(pipe, &ov, &dummy, TRUE)) {
                LogError("Failed to connect named pipe: %1", GetWin32ErrorString());
                ReportError(__LINE__);
                return;
            }

            HANDLE thread = CreateThread(nullptr, 0, RunPipeThread, pipe, 0, nullptr);
            if (!thread) {
                LogError("Failed to create new thread: %1", GetWin32ErrorString());
                ReportError(__LINE__);
                return;
            }
            CloseHandle(thread);
            pipe_guard.Disable();
        } else if (ret == WAIT_OBJECT_0 + 1) {
            break;
        } else {
            LogError("WaitForMultipleObjects() failed: %1", GetWin32ErrorString());
            ReportError(__LINE__);
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
