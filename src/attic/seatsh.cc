// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/base/base.hh"

#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <wchar.h>

namespace K {

struct PendingIO {
    OVERLAPPED ov = {}; // Keep first

    bool pending = false;
    DWORD err = 0;
    Size len = -1;

    uint8_t buf[8192];

    static void CALLBACK CompletionHandler(DWORD err, DWORD len, OVERLAPPED *ov)
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
    HANDLE pipe = CreateFileA("\\\\.\\pipe\\seatsh", GENERIC_READ | GENERIC_WRITE,
                              0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            LogError("SeatSH service does not seem to be running");
        } else {
            LogError("Failed to call SeatSH service: %1", GetWin32ErrorString());
        }
        return nullptr;
    }
    K_DEFER_N(err_guard) { CloseHandle(pipe); };

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
    BlockAllocator temp_alloc;

    // Options
    const char *cmd = nullptr;
    const char *binary = nullptr;
    HeapArray<const char *> args;
    const char *work_dir = GetWorkingDirectory();

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...] bin [arg...]%!0

Options:

    %!..+-W, --work_dir directory%!0       Change working directory

In order for this to work, you must first install the service from an elevated command prompt:

%!..+sc create SeatSH start= auto binPath= "%2" obj= LocalSystem password= ""%!0
%!..+sc start SeatSH%!0)", FelixTarget, GetApplicationExecutable());
    };

    // Parse arguments
    {
        OptionParser opt(argc, argv, OptionMode::Stop);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-W", "--work_dir", OptionType::Value)) {
                work_dir = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 127;
            }
        }

        cmd = opt.ConsumeNonOption();
        if (!cmd || !cmd[0]) {
            LogError("No command provided");
            return 127;
        }

        args.AppendDefault(3);
        opt.ConsumeNonOptions(&args);
    }

    if (!FindExecutableInPath(cmd, &temp_alloc, &binary)) {
        LogError("Cannot find this command in PATH");
        return 127;
    }
    args[0] = work_dir;
    args[1] = binary;
    args[2] = cmd;

    // Ask SeatSH to launch process
    HANDLE pipe;
    {
        LocalArray<uint8_t, 8192> msg;

        {
            int count = (int)args.len;
            MemCpy(msg.data, &count, 4);
        }
        msg.len = 4;

        for (const char *arg: args) {
            Size len = strlen(arg) + 1;

            if (len > msg.Available()) {
                LogError("Excessive command line length");
                return 127;
            }

            MemCpy(msg.end(), arg, len);
            msg.len += len;
        }

        pipe = ConnectToServer(msg);
        if (!pipe)
            return 127;
    }
    K_DEFER { CloseHandle(pipe); };

    int exit_code = 0;

    // Get the send pipe from the server
    HANDLE rev = nullptr;
    if (ReadSync(pipe, &rev, K_SIZE(rev)) != K_SIZE(rev)) {
        LogError("Failed to get back reverse HANDLE: %1", GetWin32ErrorString());
        return 127;
    }
    K_DEFER { CloseHandle(rev); };

    // Send stdin through second pipe and from background thread, to avoid issues when trying
    // to do asynchronous I/O with standard input/output and using the same pipe.
    // A lot of weird things happened, I gave up. If you want to simplify this, you're welcome!
    HANDLE send_thread = CreateThread(nullptr, 0, +[](void *rev) {
        uint8_t buf[8192];
        DWORD buf_len;

        for (;;) {
            if (!::ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf, K_SIZE(buf), &buf_len, nullptr)) {
                DWORD err = GetLastError();

                if (err != ERROR_BROKEN_PIPE && err != ERROR_NO_DATA) {
                    LogError("Failed to read from standard input: %1", GetWin32ErrorString(err));
                }

                return (DWORD)1;
            }
            if (!buf_len)
                break;

            if (!WriteSync(rev, buf, buf_len)) {
                LogError("Failed to relay stdin to server: %1", GetWin32ErrorString());
                return (DWORD)1;
            }
        }

        // Signal EOF
        if (!WriteSync(rev, buf, 0)) {
            LogError("Failed to relay EOF to server: %1", GetWin32ErrorString());
            return (DWORD)1;
        }

        return (DWORD)0;
    }, rev, 0, nullptr);
    if (!send_thread) {
        LogError("Failed to create thread: %1", GetWin32ErrorString());
        return 127;
    }
    K_DEFER { CloseHandle(send_thread); };

    // Interpret messages from server (output, exit, error)
    for (;;) {
        uint8_t buf[8192];
        Size buf_len;

        buf_len = ReadSync(pipe, buf, K_SIZE(buf));
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
            ::WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf + 1, (DWORD)buf_len - 1, &dummy, nullptr);
        } else if (buf[0] == 3) { // stderr
            DWORD dummy;
            ::WriteFile(GetStdHandle(STD_ERROR_HANDLE), buf + 1, (DWORD)buf_len - 1, &dummy, nullptr);
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
static int instance_id = 0;
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
}

static void ReportError(int error)
{
    K_ASSERT(error > 0);

    current_error = error;

    ReportStatus(SERVICE_STOPPED);
    SetEvent(stop_event);
}

static DWORD WINAPI ServiceHandler(DWORD ctrl, DWORD, void *, void *)
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
    K_DEFER { RevertToSelf(); };

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

    if (!GetTokenInformation(token, TokenUser, &buf, K_SIZE(buf), &size)) {
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

static Span<const wchar_t> WideToSpan(const wchar_t *str)
{
    Size len = wcslen(str);
    return MakeSpan(str, len);
}

static bool HandleClient(HANDLE pipe, Span<const char> work_dir,
                         Span<const char> binary, Span<const char *const> args)
{
    BlockAllocator temp_alloc;

    LogInfo("Executing '%1' in '%2', arguments: %3", binary, work_dir, FmtList(args));

    // Create another pipe to send data for bidirectional communication.
    // I tried to do asynchronous I/O with standard input/output using only one pipe.
    // A lot of weird things happened, I gave up. If you want to simplify this, you're welcome!
    HANDLE rev[2];
    if (!CreateOverlappedPipe(true, true, PipeMode::Message, rev))
        return false;
    K_DEFER {
        CloseHandleSafe(&rev[0]);
        CloseHandleSafe(&rev[1]);
    };

    // We need a HANDLE to the client process...
    HANDLE client;
    {
        ULONG pid;
        if (!GetNamedPipeClientProcessId(pipe, &pid)) {
            LogError("Failed to get client process ID: %1", GetWin32ErrorString());
            return false;
        }

        client = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
        if (!client) {
            LogError("Failed to open HANDLE to client process: %1", GetWin32ErrorString());
            return false;
        }
    }
    K_DEFER { CloseHandle(client); };

    // ... in order to give it access to our new pipe. It's all about handles folks!
    {
        HANDLE rev_client = nullptr;

        if (!DuplicateHandle(GetCurrentProcess(), rev[1], client,
                             &rev_client, 0, FALSE, DUPLICATE_SAME_ACCESS))
            return false;
        if (!WriteSync(pipe, &rev_client, K_SIZE(rev_client))) {
            LogError("Failed to send reverse HANDLE to client: %1", GetWin32ErrorString());
            return false;
        }

        CloseHandleSafe(&rev[1]);
    }

    // Fuck UCS-2 and UTF-16...
    Span<wchar_t> work_dir_w;
    Span<wchar_t> binary_w;
    work_dir_w = AllocateSpan<wchar_t>(&temp_alloc, 2 * work_dir.len + 1);
    work_dir_w.len = ConvertUtf8ToWin32Wide(work_dir, work_dir_w);
    binary_w = AllocateSpan<wchar_t>(&temp_alloc, 2 * binary.len + 1);
    binary_w.len = ConvertUtf8ToWin32Wide(binary, binary_w);
    if (binary_w.len < 0 || work_dir_w.len < 0)
        return false;

    // Windows command line quoting rules are batshit crazy
    Span<wchar_t> cmd_w;
    {
        HeapArray<wchar_t> buf(&temp_alloc);

        for (const char *arg: args) {
            bool quote = strchr(arg, ' ');

            wchar_t arg_w[8192];
            Size len_w = ConvertUtf8ToWin32Wide(arg, arg_w);
            if (len_w < 0)
                return false;
            static_assert(sizeof(wchar_t) == 2);

            buf.Append(WideToSpan(quote ? L"\"" : L""));
            for (Size i = 0; i < len_w; i++) {
                wchar_t wc = arg_w[i];

                if (wc == L'"') {
                    buf.Append(L'\\');
                }
                buf.Append(wc);
            }
            buf.Append(WideToSpan(quote ? L"\" " : L" "));
        }

        buf.len = std::max((Size)0, buf.len - 1);
        buf.Grow(1);
        buf.ptr[buf.len] = 0;

        cmd_w = buf.TrimAndLeak(1);
    }

    HANDLE client_token = GetClientToken(pipe);
    K_DEFER { CloseHandle(client_token); };

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
    K_DEFER { CloseHandle(console_token); };

    // Security check: same user?
    if (!MatchUsers(client_token, console_token)) {
        LogError("SeatSH refuses to do cross-user launches");
        return false;
    }

    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};

    // Basic startup information
    si.cb = K_SIZE(si);
    si.lpDesktop = (wchar_t *)L"winsta0\\default";
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Prepare standard stream redirection pipes
    HANDLE in_pipe[2] = {};
    HANDLE out_pipe[2] = {};
    HANDLE err_pipe[2] = {};
    K_DEFER {
        CloseHandleSafe(&in_pipe[0]);
        CloseHandleSafe(&in_pipe[1]);
        CloseHandleSafe(&out_pipe[0]);
        CloseHandleSafe(&out_pipe[1]);
        CloseHandleSafe(&err_pipe[0]);
        CloseHandleSafe(&err_pipe[1]);
    };
    if (!CreateOverlappedPipe(false, true, PipeMode::Byte, in_pipe))
        return false;
    if (!CreateOverlappedPipe(true, false, PipeMode::Byte, out_pipe))
        return false;
    if (!CreateOverlappedPipe(true, false, PipeMode::Byte, err_pipe))
        return false;

    // Retrieve user environment
    void *env;
    if (!CreateEnvironmentBlock(&env, client_token, FALSE)) {
        LogError("Failed to retrieve user environment: %1", GetWin32ErrorString());
        return false;
    }
    K_DEFER { DestroyEnvironmentBlock(env); };

    // Launch process with our redirections
    {
        K_DEFER {
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

        if (!CreateProcessAsUserW(console_token, binary_w.ptr, cmd_w.ptr, nullptr, nullptr, TRUE,
                                  CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW, env, work_dir_w.ptr, &si, &pi)) {
            LogError("Failed to start process: %1", GetWin32ErrorString());
            return false;
        }

        CloseHandleSafe(&in_pipe[0]);
        CloseHandleSafe(&out_pipe[1]);
        CloseHandleSafe(&err_pipe[1]);
    }
    K_DEFER {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    };

    // Forward stdout and stderr to client
    {
        bool running = true;

        PendingIO client_in;
        PendingIO client_out;
        PendingIO client_err;
        PendingIO proc_in;
        PendingIO proc_out;
        PendingIO proc_err;

        while (running) {
            // Transmit stdin from client to process
            if (!client_in.pending && !proc_in.pending) {
                if (client_in.err) {
                    TerminateProcess(pi.hProcess, 1);
                } else if (client_in.len >= 0) {
                    if (client_in.len) {
                        proc_in.len = client_in.len;
                        MemCpy(proc_in.buf, client_in.buf, proc_in.len);
                        client_in.len = -1;

                        if (!proc_in.err && !WriteFileEx(in_pipe[1], proc_in.buf, (DWORD)proc_in.len,
                                                         &proc_in.ov, PendingIO::CompletionHandler)) {
                            proc_in.err = GetLastError();
                        }
                    } else { // EOF
                        CloseHandleSafe(&in_pipe[1]);
                    }

                    proc_in.pending = true;
                }

                if (client_in.len < 0) {
                    if (!ReadFileEx(rev[0], client_in.buf, K_SIZE(client_in.buf),
                                    &client_in.ov, PendingIO::CompletionHandler)) {
                        client_in.err = GetLastError();
                    }

                    client_in.pending = true;
                }

                if (client_in.err) {
                    TerminateProcess(pi.hProcess, 1);
                    if (client_in.err != ERROR_BROKEN_PIPE && client_in.err != ERROR_NO_DATA) {
                        LogError("Lost read connection to client: %1", GetWin32ErrorString(client_in.err));
                    }
                    client_in.pending = true;
                }
                if (proc_in.err) {
                    if (proc_in.err != ERROR_BROKEN_PIPE && proc_in.err != ERROR_NO_DATA) {
                        LogError("Failed to write to process: %1", GetWin32ErrorString(proc_in.err));
                    }
                    proc_in.pending = true;
                }
            }

            // Transmit stdout from process to client
            if (!proc_out.pending && !client_out.pending) {
                if (!proc_out.err && proc_out.len >= 0) {
                    client_out.len = proc_out.len + 1;
                    MemCpy(client_out.buf, proc_out.buf, client_out.len);
                    proc_out.len = -1;

                    if (!client_out.err && !WriteFileEx(pipe, client_out.buf, (DWORD)client_out.len,
                                                        &client_out.ov, PendingIO::CompletionHandler)) {
                        client_out.err = GetLastError();
                    }

                    client_out.pending = true;
                }

                if (proc_out.len < 0) {
                    proc_out.buf[0] = 2;

                    if (!ReadFileEx(out_pipe[0], proc_out.buf + 1, K_SIZE(proc_out.buf) - 1,
                                    &proc_out.ov, PendingIO::CompletionHandler)) {
                        proc_out.err = GetLastError();
                    }

                    proc_out.pending = true;
                }

                if (proc_out.err) {
                    if (proc_out.err != ERROR_BROKEN_PIPE && proc_out.err != ERROR_NO_DATA) {
                        LogError("Failed to read process stdout: %1", GetWin32ErrorString(proc_out.err));
                    }
                    proc_out.pending = true;
                }
                if (client_out.err) {
                    if (client_out.err != ERROR_BROKEN_PIPE && client_out.err != ERROR_NO_DATA) {
                        LogError("Lost write connection to client: %1", GetWin32ErrorString(client_out.err));
                    }
                    client_out.pending = true;
                }
            }

            // Transmit stderr from process to client
            if (!proc_err.pending && !client_err.pending) {
                if (!proc_err.err && proc_err.len >= 0) {
                    client_err.len = proc_err.len + 1;
                    MemCpy(client_err.buf, proc_err.buf, client_err.len);
                    proc_err.len = -1;

                    if (!client_err.err && !WriteFileEx(pipe, client_err.buf, (DWORD)client_err.len,
                                                        &client_err.ov, PendingIO::CompletionHandler)) {
                        client_err.err = GetLastError();
                    }

                    client_err.pending = true;
                }

                if (proc_err.len < 0) {
                    proc_err.buf[0] = 3;

                    if (!ReadFileEx(err_pipe[0], proc_err.buf + 1, K_SIZE(proc_err.buf) - 1,
                                    &proc_err.ov, PendingIO::CompletionHandler)) {
                        proc_err.err = GetLastError();
                    }

                    proc_err.pending = true;
                }

                if (proc_err.err) {
                    if (proc_err.err != ERROR_BROKEN_PIPE && proc_err.err != ERROR_NO_DATA) {
                        LogError("Failed to read process stderr: %1", GetWin32ErrorString(proc_err.err));
                    }
                    proc_err.pending = true;
                }
                if (client_err.err) {
                    if (client_err.err != ERROR_BROKEN_PIPE && client_err.err != ERROR_NO_DATA) {
                        LogError("Lost write connection to client: %1", GetWin32ErrorString(client_err.err));
                    }
                    client_err.pending = true;
                }
            }

            running = (WaitForSingleObjectEx(pi.hProcess, INFINITE, TRUE) != WAIT_OBJECT_0);
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
        MemCpy(buf + 1, &exit_code, 4);

        if (!WriteSync(pipe, buf, K_SIZE(buf))) {
            LogError("Failed to send process exit code to client: %1", GetWin32ErrorString());
            return false;
        }
    }

    return true;
}

static DWORD WINAPI RunPipeThread(void *pipe)
{
    K_DEFER { CloseHandle(pipe); };

    int client_id = GetRandomInt(0, 100000000);

    char err_buf[8192];
    CopyString("Unknown error", MakeSpan(err_buf + 1, K_SIZE(err_buf) - 1));
    err_buf[0] = 1;

    // If something fails (command does not exist, etc), send it to the client
    K_DEFER_N(err_guard) { WriteSync(pipe, err_buf, strlen(err_buf)); };

    PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        char ctx_buf[1024];
        Fmt(ctx_buf, "%1[Client %2_%3]", ctx ? ctx : "", FmtInt(instance_id, 8), FmtInt(client_id, 8));

        if (level == LogLevel::Error) {
            CopyString(msg, MakeSpan(err_buf + 1, K_SIZE(err_buf) - 1));
        };

        func(level, ctx_buf, msg);
    });
    K_DEFER { PopLogFilter(); };

    char buf[8192];
    Size buf_len = ReadSync(pipe, buf, K_SIZE(buf) - 1);
    if (buf_len < 0)
        return 1;
    if (buf_len < 4) {
        LogError("Malformed message from client");
        return 1;
    }
    buf[buf_len] = 0;

    int count = *(int *)buf;
    if (count < 2) {
        LogError("Malformed message from client");
        return 1;
    }

    HeapArray<const char *> args;
    {
        Size offset = 4;

        for (int i = 0; i < count; i++) {
            if (offset >= buf_len) {
                LogError("Malformed message from client");
                return 1;
            }

            Span<const char> arg = buf + offset;
            args.Append(arg.ptr);

            offset += arg.len + 1;
        }
    }

    const char *work_dir = args[0];
    const char *binary = args[1];

    if (!HandleClient(pipe, work_dir, binary, args.Take(2, args.len - 2)))
        return 1;

    err_guard.Disable();
    return 0;
}

static void WINAPI RunService(DWORD, char **)
{
    if (!RedirectLogToWindowsEvents("SeatSH")) {
        ReportError(__LINE__);
        return;
    }

    // Register our service controller
    status_handle = RegisterServiceCtrlHandlerExA("SeatSH", &ServiceHandler, nullptr);
    K_CRITICAL(status_handle, "Failed to register service controller: %1", GetWin32ErrorString());

    ReportStatus(SERVICE_START_PENDING);

    instance_id = GetRandomInt(0, 100000000);

    // This event is used (embedded in an OVERLAPPED) to wake up on connection
    HANDLE connect_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!connect_event) {
        LogError("Failed to create event: %1", GetWin32ErrorString());
        ReportError(__LINE__);
        return;
    }
    K_DEFER { CloseHandle(connect_event); };

    // The stop event is used by the service control handler, for shutdown
    stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!stop_event) {
        LogError("Failed to create event: %1", GetWin32ErrorString());
        ReportError(__LINE__);
        return;
    }
    K_DEFER { CloseHandle(stop_event); };

    // Open for everyone!
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES sa = {};
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);
    sa.nLength = K_SIZE(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    ReportStatus(SERVICE_RUNNING);

    for (;;) {
        HANDLE pipe = CreateNamedPipeA("\\\\.\\pipe\\seatsh", PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                       PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                       PIPE_UNLIMITED_INSTANCES, 8192, 8192, 0, &sa);
        if (pipe == INVALID_HANDLE_VALUE) {
            LogError("Failed to create main named pipe: %1", GetWin32ErrorString());
            ReportError(__LINE__);
            return;
        }
        K_DEFER_N(pipe_guard) {
            CancelIo(pipe);
            CloseHandle(pipe);
        };

        OVERLAPPED ov = {};
        ov.hEvent = connect_event;

        if (!ConnectNamedPipe(pipe, &ov) && GetLastError() != ERROR_IO_PENDING) {
            LogError("Failed to connect to named pipe: %1", GetWin32ErrorString());
            ReportError(__LINE__);
            return;
        }

        HANDLE events[] = {
            connect_event,
            stop_event
        };
        DWORD ret = WaitForMultipleObjects(K_LEN(events), events, FALSE, INFINITE);

        if (ret == WAIT_OBJECT_0) {
            DWORD dummy;

            if (!GetOverlappedResult(pipe, &ov, &dummy, TRUE)) {
                LogError("Failed to connect to named pipe: %1", GetWin32ErrorString());
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
        PrintLn(T("Compiler: %1"), FelixCompiler);
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
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
