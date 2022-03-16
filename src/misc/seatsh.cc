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
#include <userenv.h>
#include <wchar.h>

namespace RG {

struct PendingIO {
    OVERLAPPED ov = {}; // Keep first

    bool pending = false;
    DWORD err = 0;
    Size len = -1;

    uint8_t buf[8192];

    static void CompletionHandler(DWORD err, DWORD len, OVERLAPPED *ov)
    {
        PendingIO *self = (PendingIO *)ov;

        self->pending = false;
        self->err = err;
        self->len = err ? -1 : len;
    }
};

static Size ReadSync(HANDLE h, void *buf, Size buf_len)
{
    OVERLAPPED ov = {};
    DWORD len;

    if (!::ReadFile(h, buf, (DWORD)buf_len, nullptr, &ov) &&
            GetLastError() != ERROR_IO_PENDING)
        return -1;
    if (!GetOverlappedResult(h, &ov, &len, TRUE))
        return -1;

    return (Size)len;
}

static bool WriteSync(HANDLE h, const void *buf, Size buf_len)
{
    OVERLAPPED ov = {};
    DWORD dummy;

    if (!::WriteFile(h, buf, (DWORD)buf_len, nullptr, &ov) &&
            GetLastError() != ERROR_IO_PENDING)
        return false;
    if (!GetOverlappedResult(h, &ov, &dummy, TRUE))
        return false;

    return true;
}

// Client

static HANDLE ConnectToServer(Span<const uint8_t> msg)
{
    HANDLE pipe = CreateFileA("\\\\.\\pipe\\SeatSH", GENERIC_READ | GENERIC_WRITE,
                              0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            LogError("SeatSH service does not seem to be running");
        } else {
            LogError("Failed to call SeatSH service: %1", GetWin32ErrorString());
        }
        return nullptr;
    }
    RG_DEFER_N(err_guard) { CloseHandle(pipe); };

    // We want messages, not bytes
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
        LogError("Failed to switch pipe to message mode: %1", GetWin32ErrorString());
        return nullptr;
    }

    // Welcome message
    if (!WriteSync(pipe, msg.ptr, msg.len)) {
        LogError("Failed to send welcome to SeatSH: %1", GetWin32ErrorString());
        return nullptr;
    }

    err_guard.Disable();
    return pipe;
}

static int RunClient(int argc, char **argv)
{
    // Options
    Span<const char> cmd_line = nullptr;
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

        cmd_line = opt.ConsumeNonOption();
        if (!cmd_line.len) {
            LogError("No command provided");
            return 127;
        }
    }

    // Ask SeatSH to launch process
    HANDLE pipe;
    {
        LocalArray<char, 8192> msg;
        if (cmd_line.len + work_dir.len > RG_SIZE(msg.data) - 2) {
            LogError("Excessive command or working directory length");
            return 127;
        }
        msg.len = Fmt(msg.data, "%1%2%3%4", '\0', cmd_line, '\0', work_dir).len + 1;

        pipe = ConnectToServer(msg.As<uint8_t>());
        if (!pipe)
            return 127;
    }
    RG_DEFER { CloseHandle(pipe); };

    int client_id = 0;
    int exit_code = 0;

    // Get the client ID from the server
    if (!ReadSync(pipe, &client_id, RG_SIZE(client_id))) {
        LogError("Failed to get back client ID: %1", GetWin32ErrorString());
        return 127;
    }

    HANDLE send_pipe;
    {
        uint8_t msg[5];
        msg[0] = 1;
        memcpy(msg + 1, &client_id, RG_SIZE(client_id));

        send_pipe = ConnectToServer(msg);
        if (!send_pipe)
            return 127;
    }
    RG_DEFER { CloseHandle(send_pipe); };

    // Send stdin through second pipe and from background thread, to avoid issues when trying
    // to do asynchronous I/O with standard input/output and using the same pipe.
    // A lot of weird things happened, I gave up. If you want to simplify this, you're welcome!
    HANDLE send_thread = CreateThread(nullptr, 0, +[](void *send_pipe) {
        uint8_t buf[8192];
        DWORD buf_len;

        for (;;) {
            if (!::ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf, RG_SIZE(buf), &buf_len, nullptr)) {
                LogError("Failed to read from standard input: %1", GetWin32ErrorString());
                return (DWORD)1;
            }
            if (!buf_len)
                break;

            if (!WriteSync(send_pipe, buf, buf_len)) {
                LogError("Failed to relay stdin to server: %1", GetWin32ErrorString());
                return (DWORD)1;
            }
        }

        // Signal EOF
        if (!WriteSync(send_pipe, buf, 0)) {
            LogError("Failed to relay EOF to server: %1", GetWin32ErrorString());
            return (DWORD)1;
        }

        return (DWORD)0;
    }, send_pipe, 0, nullptr);
    if (!send_thread) {
        LogError("Failed to create thread: %1", GetWin32ErrorString());
        return 127;
    }
    RG_DEFER { CloseHandle(send_thread); };

    // Interpret messages from server (output, exit, error)
    for (;;) {
        uint8_t buf[8192];
        Size buf_len;

        buf_len = ReadSync(pipe, buf, RG_SIZE(buf));
        if (buf_len < 0) {
            LogError("Failed to read from SeatSH: %1", GetWin32ErrorString());
            return 127;
        }
        if (!buf_len)
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
            DWORD dummy;
            ::WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf + 1, buf_len - 1, &dummy, nullptr);
        } else if (buf[0] == 3) { // stderr
            DWORD dummy;
            ::WriteFile(GetStdHandle(STD_ERROR_HANDLE), buf + 1, buf_len - 1, &dummy, nullptr);
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

struct ClientControl {
    int id;

    std::atomic<void *> pipe;
    HANDLE wakeup;

    RG_HASHTABLE_HANDLER(ClientControl, id);
};

static std::mutex server_mutex;
static SERVICE_STATUS_HANDLE status_handle = nullptr;
static int instance_id = 0;
static int current_state = 0;
static int current_error = 0;
static HANDLE stop_event = nullptr;
static HashTable<int, ClientControl *> clients_map;

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

static bool GetTokenSID(HANDLE token, PSID *out_sid)
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
    if (!GetTokenSID(token1, &sid1))
        return false;
    if (!GetTokenSID(token2, &sid2))
        return false;

    return EqualSid(sid1, sid2);
}

static bool HandleClient(HANDLE pipe, ClientControl *client, Span<const char> cmd_line,
                         Span<const char> work_dir, Allocator *alloc)
{
    LogInfo("Executing '%1' in '%2'", cmd_line, work_dir);

    // Register this client
    {
        std::lock_guard<std::mutex> lock(server_mutex);
        clients_map.Set(client);
    }
    RG_DEFER { clients_map.Remove(client->id); };

    // Give the ID to the client
    if (!WriteSync(pipe, &client->id, RG_SIZE(client->id))) {
        LogError("Failed to send ID to client: %1", GetWin32ErrorString());
        return false;
    }

    // Fuck UCS-2 and UTF-16...
    Span<wchar_t> cmd_line_w;
    Span<wchar_t> work_dir_w;
    cmd_line_w.len = 4 * cmd_line.len + 2;
    cmd_line_w.ptr = (wchar_t *)Allocator::Allocate(alloc, cmd_line_w.len);
    cmd_line_w.len = ConvertUtf8ToWin32Wide(cmd_line, cmd_line_w);
    work_dir_w.len = 4 * work_dir.len + 2;
    work_dir_w.ptr = (wchar_t *)Allocator::Allocate(alloc, work_dir_w.len);
    work_dir_w.len = ConvertUtf8ToWin32Wide(work_dir, work_dir_w);
    if (cmd_line_w.len < 0 || work_dir_w.len < 0)
        return false;

    HANDLE client_token = GetClientToken(pipe);
    RG_DEFER { CloseHandle(client_token); };

    HANDLE console_token;
    {
        DWORD sid = WTSGetActiveConsoleSessionId();

        if (sid == UINT32_MAX) {
            LogError("Failed to get active control session ID: %1", GetWin32ErrorString());
            return false;
        }
        if (!WTSQueryUserToken(sid, &console_token)) {
            LogError("Failed to query active session user token: %1", GetWin32ErrorString());
            return false;
        }
    }
    RG_DEFER { CloseHandle(console_token); };

    // Security check: same user?
    if (!MatchUsers(client_token, console_token)) {
        LogError("SeatSH refuses to do cross-user launches");
        return false;
    }

    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};

    // Basic startup information
    si.cb = RG_SIZE(si);
    si.lpDesktop = (wchar_t *)L"winsta0\\default";
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Prepare standard stream redirection pipes
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
        return false;
    if (!CreateOverlappedPipe(true, false, out_pipe))
        return false;
    if (!CreateOverlappedPipe(true, false, err_pipe))
        return false;

    // Retrieve user environment
    void *env;
    if (!CreateEnvironmentBlock(&env, client_token, FALSE)) {
        LogError("Failed to retrieve user environment: %1", GetWin32ErrorString());
        return false;
    }
    RG_DEFER { DestroyEnvironmentBlock(env); };

    // Find the PATH variable
    const wchar_t *path_w = nullptr;
    for (const wchar_t *ptr = (const wchar_t *)env; ptr[0]; ptr += wcslen(ptr) + 1) {
        if (!_wcsnicmp(ptr, L"PATH=", 5)) {
            path_w = ptr + 5;
            break;
        }
    }

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
            return false;
        }
        if (!DuplicateHandle(GetCurrentProcess(), out_pipe[1],
                             GetCurrentProcess(), &si.hStdOutput, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            LogError("Failed to duplicate handle: %1", GetWin32ErrorString());
            return false;
        }
        if (!DuplicateHandle(GetCurrentProcess(), err_pipe[1],
                             GetCurrentProcess(), &si.hStdError, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            LogError("Failed to duplicate handle: %1", GetWin32ErrorString());
            return false;
        }

        // Launch process, after setting the PATH variable to match the user. This is a bit dirty, and needs a lock.
        // XXX: A better solution would be to extract the binary from cmd_line and to use FindExecutableInPath.
        {
            std::lock_guard<std::mutex> lock(server_mutex);

            if (path_w) {
                SetEnvironmentVariableW(L"PATH", path_w);
            }

            if (!CreateProcessAsUserW(console_token, nullptr, cmd_line_w.ptr, nullptr, nullptr, TRUE,
                                      CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW, env, work_dir_w.ptr, &si, &pi)) {
                LogError("Failed to start process: %1", GetWin32ErrorString());
                return false;
            }
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
        bool running = true;

        PendingIO client_in;
        PendingIO client_out;
        PendingIO proc_in;
        PendingIO proc_out;
        PendingIO proc_err;

        while (running) {
            // Transmit stdin from client to process
            if (!client_in.pending && !proc_in.pending) {
                if (client_in.err) {
                    if (client_in.err != ERROR_BROKEN_PIPE && client_in.err != ERROR_NO_DATA) {
                        LogError("Lost connection to client: %1", GetWin32ErrorString(client_in.err));
                    }

                    if (in_pipe[1]) {
                        TerminateProcess(pi.hProcess, 1);
                    }

                    client_in.pending = true; // Don't try anything again
                } else if (client_in.len >= 0) {
                    if (client_in.len) {
                        proc_in.len = client_in.len;
                        memcpy_safe(proc_in.buf, client_in.buf, proc_in.len);
                        client_in.len = -1;

                        if (!proc_in.err) {
                            if (WriteFileEx(in_pipe[1], proc_in.buf, proc_in.len, &proc_in.ov, PendingIO::CompletionHandler)) {
                                proc_in.pending = true;
                            } else {
                                proc_in.err = GetLastError();
                            }
                        }
                    } else { // EOF
                        CloseHandleSafe(&in_pipe[1]);
                        client_in.pending = true;
                    }
                }

                if (client_in.len < 0) {
                    HANDLE pipe2 = client->pipe.load();

                    if (ReadFileEx(pipe2, client_in.buf, RG_SIZE(client_in.buf), &client_in.ov, PendingIO::CompletionHandler)) {
                        client_in.pending = true;
                    } else {
                        client_in.err = GetLastError();
                    }
                }
            }

            // Transmit stdout from process to client
            if (!proc_out.pending && !client_out.pending) {
                if (proc_out.err) {
                    if (proc_out.err != ERROR_BROKEN_PIPE && proc_out.err != ERROR_NO_DATA) {
                        LogError("Failed to read process stdout: %1", GetWin32ErrorString(proc_out.err));
                    }

                    proc_out.pending = true; // Don't try anything again
                } else if (proc_out.len >= 0) {
                    client_out.len = proc_out.len + 1;
                    memcpy_safe(client_out.buf, proc_out.buf, client_out.len);
                    proc_out.len = -1;

                    if (!client_out.err) {
                        if (WriteFileEx(pipe, client_out.buf, client_out.len, &client_out.ov, PendingIO::CompletionHandler)) {
                            client_out.pending = true;
                        } else {
                            client_out.err = GetLastError();
                        }
                    }
                }

                if (proc_out.len < 0) {
                    proc_out.buf[0] = 2;

                    if (ReadFileEx(out_pipe[0], proc_out.buf + 1, RG_SIZE(proc_out.buf) - 1, &proc_out.ov, PendingIO::CompletionHandler)) {
                        proc_out.pending = true;
                    } else {
                        proc_out.err = GetLastError();
                    }
                }
            }

            // Transmit stderr from process to client
            if (!proc_err.pending && !client_out.pending) {
                if (proc_err.err) {
                    if (proc_err.err != ERROR_BROKEN_PIPE && proc_err.err != ERROR_NO_DATA) {
                        LogError("Failed to read process stderr: %1", GetWin32ErrorString(proc_err.err));
                    }
                    proc_err.pending = true; // Don't try anything again
                } else if (proc_err.len >= 0) {
                    client_out.len = proc_err.len + 1;
                    memcpy_safe(client_out.buf, proc_err.buf, client_out.len);
                    proc_err.len = -1;

                    if (!client_out.err) {
                        if (WriteFileEx(pipe, client_out.buf, client_out.len, &client_out.ov, PendingIO::CompletionHandler)) {
                            client_out.pending = true;
                        } else {
                            client_out.err = GetLastError();
                        }
                    }
                }

                if (proc_err.len < 0) {
                    proc_err.buf[0] = 3;

                    if (ReadFileEx(err_pipe[0], proc_err.buf + 1, RG_SIZE(proc_err.buf) - 1, &proc_err.ov, PendingIO::CompletionHandler)) {
                        proc_err.pending = true;
                    } else {
                        proc_err.err = GetLastError();
                    }
                }
            }

            HANDLE events[] = {
                pi.hProcess,
                client->wakeup
            };
            running = (WaitForMultipleObjectsEx(RG_LEN(events), events, FALSE, INFINITE, TRUE) != WAIT_OBJECT_0);
            ResetEvent(client->wakeup);
        }
    }

    // Get process exit code
    DWORD exit_code;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        LogError("GetExitCodeProcess() failed: %1", GetWin32ErrorString());
        return false;
    }

    // Send exit code to client
    {
        uint8_t buf[5];

        buf[0] = 0;
        memcpy(buf + 1, &exit_code, 4);

        if (!WriteSync(pipe, buf, RG_SIZE(buf))) {
            LogError("Failed to send process exit code to client: %1", GetWin32ErrorString());
            return false;
        }
    }

    return true;
}

static DWORD WINAPI RunPipeThread(void *pipe)
{
    BlockAllocator temp_alloc;

    ClientControl client = {};

    RG_DEFER {
        HANDLE client_pipe = client.pipe.load();

        if (pipe) {
            CloseHandle(pipe);
        }
        if (client_pipe) {
            CloseHandle(client_pipe);
        }
    };

    client.id = GetRandomIntSafe(0, 100000000);
    client.wakeup = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!client.wakeup) {
        LogError("Failed to create event: %1", GetWin32ErrorString());
        return 1;
    }
    RG_DEFER { CloseHandle(client.wakeup); };

    char err_buf[8192];
    CopyString("Unknown error", MakeSpan(err_buf + 1, RG_SIZE(err_buf) - 1));
    err_buf[0] = 1;

    // If something fails (command does not exist, etc), send it to the client
    RG_DEFER_N(err_guard) { WriteSync(pipe, err_buf, strlen(err_buf)); };

    PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        char ctx_buf[1024];
        Fmt(ctx_buf, "Client %1_%2%3%4", FmtArg(instance_id).Pad0(-8), FmtArg(client.id).Pad0(-8),
                                         ctx ? ": " : "", ctx ? ctx : "");

        if (level == LogLevel::Error) {
            CopyString(msg, MakeSpan(err_buf + 1, RG_SIZE(err_buf) - 1));
        };

        func(level, ctx_buf, msg);
    });
    RG_DEFER { PopLogFilter(); };

    char buf[8192];
    Size buf_len = ReadSync(pipe, buf, RG_SIZE(buf) - 1);
    if (buf_len < 0)
        return 1;
    if (!buf_len) {
        LogError("Received empty message from client");
        return 1;
    }
    buf[buf_len] = 0;

    if (buf[0] == 0) {
        Span<const char> cmd_line = DuplicateString(buf + 1, &temp_alloc);
        Span<const char> work_dir = DuplicateString(buf + cmd_line.len + 2, &temp_alloc);

        if (HandleClient(pipe, &client, cmd_line, work_dir, &temp_alloc)) {
            err_guard.Disable();
            return 0;
        } else {
            return 1;
        }
    } else if (buf[0] == 1) {
        std::lock_guard<std::mutex> lock(server_mutex);

        if (buf_len != 5) {
            LogError("Malformed message from client");
            return 1;
        }

        int id = *(int *)(buf + 1);

        ClientControl *target = clients_map.FindValue(id, nullptr);
        if (!target) {
            LogError("Trying to join non-existent client '%1'", id);
            return 1;
        }
        if (target->pipe.load()) {
            LogError("Cannot join client '%1' again", id);
            return 1;
        }

        LogInfo("Joining client %1 for sending", id);

        target->pipe.store(pipe);
        pipe = nullptr; // Don't close it in defer

        SetEvent(target->wakeup);

        err_guard.Disable();
        return 0;
    } else {
        LogError("Malformed message from client");
        return 1;
    }
}

static void WINAPI RunService(DWORD argc, char **argv)
{
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

    instance_id = GetRandomIntSafe(0, 100000000);

    // This event is used (embedded in an OVERLAPPED) to wake up on connection
    HANDLE connect_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!connect_event) {
        LogError("Failed to create event: %1", GetWin32ErrorString());
        ReportError(__LINE__);
        return;
    }
    RG_DEFER { CloseHandle(connect_event); };

    // The stop event is used by the service control handler, for shutdown
    stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!stop_event) {
        LogError("Failed to create event: %1", GetWin32ErrorString());
        ReportError(__LINE__);
        return;
    }
    RG_DEFER { CloseHandle(stop_event); };

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
            DWORD dummy;

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

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

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
