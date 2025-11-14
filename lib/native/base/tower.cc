// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "base.hh"
#include "tower.hh"

#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
    #define SECURITY_WIN32
    #include <windows.h>
    #include <security.h>
#else
    #include <fcntl.h>
    #include <poll.h>
    #include <sys/socket.h>
#endif

namespace K {

#if defined(_WIN32)

struct OverlappedPipe {
    OVERLAPPED ov = {};
    HANDLE h = nullptr;
    uint8_t buf[1024];

    ~OverlappedPipe();
};

OverlappedPipe::~OverlappedPipe()
{
    if (h) {
        CancelIo(h);
        CloseHandle(h);
    }
    if (ov.hEvent) {
        CloseHandle(ov.hEvent);
    }
}

static bool CheckPipePath(const char *path)
{
    if (!StartsWith(path, "\\\\.\\pipe\\")) {
        LogError("Control pipe names must start with '%1'", "\\\\.\\pipe\\");
        return false;
    }
    if (!path[9]) {
        LogError("Truncated control pipe name '%1'", path);
        return false;
    }

    return true;
}

static OverlappedPipe *BindPipe(const char *path)
{
    OverlappedPipe *pipe = new OverlappedPipe();
    K_DEFER_N(err_guard) { delete pipe; };

    pipe->ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!pipe->ov.hEvent) {
        LogError("Failed to create event: %1", GetWin32ErrorString());
        return nullptr;
    }

    pipe->h = CreateNamedPipeA(path, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                               PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 8192, 8192, 0, nullptr);
    if (pipe->h == INVALID_HANDLE_VALUE) {
        pipe->h = nullptr;

        LogError("Failed to create named control pipe: %1", GetWin32ErrorString());
        return nullptr;
    }

    if (ConnectNamedPipe(pipe->h, &pipe->ov) || GetLastError() == ERROR_PIPE_CONNECTED) {
        SetEvent(pipe->ov.hEvent);
    } else if (GetLastError() != ERROR_IO_PENDING) {
        LogError("Failed to connect to named pipe: %1", GetWin32ErrorString());
        return nullptr;
    }

    err_guard.Disable();
    return pipe;
}

static OverlappedPipe *ConnectPipe(const char *path)
{
    OverlappedPipe *pipe = new OverlappedPipe();
    K_DEFER_N(err_guard) { delete pipe; };

    pipe->ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!pipe->ov.hEvent) {
        LogError("Failed to create event: %1", GetWin32ErrorString());
        return nullptr;
    }

    for (int i = 0; i < 10; i++) {
        if (!WaitNamedPipeA(path, 10))
            continue;

        pipe->h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                              0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

        if (pipe->h != INVALID_HANDLE_VALUE)
            break;
        pipe->h = nullptr;

        if (GetLastError() != ERROR_PIPE_BUSY) {
            LogError("Failed to connect to named pipe: %1", GetWin32ErrorString());
            return nullptr;
        }
    }

    if (!pipe->h) {
        LogError("Failed to connect to named pipe: %1", GetWin32ErrorString());
        return nullptr;
    }

    err_guard.Disable();
    return pipe;
}

// Does not print errors
static bool StartRead(OverlappedPipe *pipe)
{
    ResetEvent(pipe->ov.hEvent);

    if (!::ReadFile(pipe->h, pipe->buf, K_SIZE(pipe->buf), nullptr, &pipe->ov) &&
            GetLastError() != ERROR_IO_PENDING)
        return false;

    return true;
}

// Does not print errors
static Size FinalizeRead(OverlappedPipe *pipe)
{
    DWORD len = 0;
    if (!GetOverlappedResult(pipe->h, &pipe->ov, &len, TRUE))
        return -1;

    return len;
}

// Does not print errors
static Size ReadSync(OverlappedPipe *pipe, void *buf, Size buf_len, int timeout)
{
    DWORD len = 0;

    if (!::ReadFile(pipe->h, buf, (DWORD)buf_len, nullptr, &pipe->ov) &&
            GetLastError() != ERROR_IO_PENDING)
        return -1;
    if (timeout > 0)
        WaitForSingleObject(pipe->ov.hEvent, timeout);
    if (!GetOverlappedResult(pipe->h, &pipe->ov, &len, timeout < 0) &&
            GetLastError() != ERROR_IO_INCOMPLETE)
        return -1;

    return (Size)len;
}

// Does not print errors
static Size WriteSync(OverlappedPipe *pipe, const void *buf, Size buf_len)
{
    OVERLAPPED ov = {};
    DWORD written = 0;

    if (!::WriteFile(pipe->h, buf, (DWORD)buf_len, nullptr, &ov) &&
            GetLastError() != ERROR_IO_PENDING)
        return -1;
    if (!GetOverlappedResult(pipe->h, &ov, &written, TRUE))
        return -1;

    return (Size)written;
}

bool TowerServer::Bind(const char *path)
{
    K_ASSERT(!name[0]);
    K_ASSERT(!pipes.len);

    K_DEFER_N(err_guard) { Stop(); };

    if (!CheckPipePath(path))
        return false;
    if (!CopyString(path, name)) {
        LogError("Control pipe name '%1' is too long", path);
        return false;
    }

    OverlappedPipe *pipe = BindPipe(path);
    if (!pipe)
        return false;
    pipes.Append(pipe);

    err_guard.Disable();
    return true;
}

void TowerServer::Start(std::function<bool(StreamReader *, StreamWriter *)> func)
{
    K_ASSERT(pipes.len == 1);
    K_ASSERT(!sources.len);
    K_ASSERT(!handle_func);

    sources.Append({ pipes[0]->ov.hEvent, -1 });
    handle_func = func;
}

void TowerServer::Stop()
{
    for (OverlappedPipe *pipe: pipes) {
        delete pipe;
    }
    pipes.Clear();
    sources.Clear();

    MemSet(name, 0, K_SIZE(name));

    handle_func = {};
}

static bool IsSignaled(HANDLE h)
{
    DWORD ret = WaitForSingleObject(h, 0);
    return ret == WAIT_OBJECT_0;
}

bool TowerServer::Process(uint64_t ready)
{
    // Accept new clients
    if (ready & 1) {
        OverlappedPipe *client = pipes[0];

        if (IsSignaled(client->ov.hEvent)) {
            OverlappedPipe *pipe = BindPipe(name);

            // We're kind of screwed if this happens, let the caller know and fail hard
            if (!pipe) {
                sources.len = 0;
                return false;
            }

            pipes[0] = pipe;
            sources[0].handle = pipe->ov.hEvent;

            if (pipes.Available()) [[likely]] {
                if (FinalizeRead(client) == 0 && StartRead(client)) {
                    pipes.Append(client);
                    sources.Append({ client->ov.hEvent, -1 });

                    LogDebug("Client has connected");
                } else {
                    LogError("Failed to accept client: %1", GetWin32ErrorString());
                    delete client;
                }
            } else {
                LogError("Too many connections, refusing new client");
                delete client;
            }
        }
    }

    RunClients([&](Size idx, OverlappedPipe *pipe) {
        if (!(ready & (1ull << idx)))
            return true;

        Span<uint8_t> buf = MakeSpan(pipe->buf, FinalizeRead(pipe));
        if (buf.len < 0) {
            LogDebug("Client has disconnected");
            return false;
        }

        const auto read = [&](Span<uint8_t> out_buf) {
            if (buf.len) {
                Size copy_len = std::min(buf.len, out_buf.len);
                MemCpy(out_buf.ptr, buf.ptr, copy_len);

                buf.ptr += copy_len;
                buf.len -= copy_len;

                return copy_len;
            }

            Size received = ReadSync(pipe, out_buf.ptr, out_buf.len, 1000);
            if (received < 0) {
                LogError("Failed to receive data from client: %1", GetWin32ErrorString());
            } else if (!received) {
                LogError("Client has timed out");
                received = -1;
            }

            return received;
        };

        const auto write = [&](Span<const uint8_t> buf) {
            while (buf.len) {
                Size sent = WriteSync(pipe, buf.ptr, buf.len);
                if (sent < 0) {
                    LogError("Failed to send data to server: %1", GetWin32ErrorString());
                    return false;
                }

                buf.ptr += sent;
                buf.len -= sent;
            }

            return true;
        };

        StreamReader reader(read, "<client>");
        StreamWriter writer(write, "<client>");

        if (!handle_func(&reader, &writer))
            return false;
        if (!reader.Close())
            return false;
        if (!writer.Close())
            return false;

        if (!StartRead(pipe)) {
            LogDebug("Client has disconnected");
            return false;
        }

        return true;
    });

    return true;
}

void TowerServer::Send(FunctionRef<void(StreamWriter *)> func)
{
    RunClients([&](Size, OverlappedPipe *pipe) {
        const auto write = [&](Span<const uint8_t> buf) {
            while (buf.len) {
                Size sent = WriteSync(pipe, buf.ptr, buf.len);
                if (sent < 0) {
                    LogError("Failed to send data to server: %1", GetWin32ErrorString());
                    return false;
                }

                buf.ptr += sent;
                buf.len -= sent;
            }

            return true;
        };

        StreamWriter writer(write, "<client>");
        func(&writer);

        return writer.Close();
    });
}

void TowerServer::RunClients(FunctionRef<bool(Size, OverlappedPipe *)> func)
{
    Size j = 1;
    for (Size i = 1; i < pipes.len; i++) {
        OverlappedPipe *pipe = pipes[i];

        pipes[j] = pipe;
        sources[j].handle = pipe->ov.hEvent;

        if (!func(i, pipe)) {
            delete pipe;
            continue;
        }

        j++;
    }
    pipes.len = j;
    sources.len = j;
}

bool TowerClient::Connect(const char *path)
{
    Stop();

    K_DEFER_N(err_guard) { Stop(); };

    if (!CheckPipePath(path))
        return false;

    pipe = ConnectPipe(path);
    if (!pipe)
        return false;

    if (!StartRead(pipe)) {
        LogError("Failed to connect to named pipe: %1", GetWin32ErrorString());
        return false;
    }

    err_guard.Disable();
    return true;
}

void TowerClient::Start(std::function<void(StreamReader *)> func)
{
    K_ASSERT(pipe);
    K_ASSERT(!handle_func);

    src = { pipe->ov.hEvent, -1 };
    handle_func = func;
}

void TowerClient::Stop()
{
    if (pipe) {
        delete pipe;
        pipe = nullptr;
    }

    handle_func = {};
}

bool TowerClient::Process()
{
    if (!IsSignaled(pipe->ov.hEvent))
        return true;

    Span<uint8_t> buf = MakeSpan(pipe->buf, FinalizeRead(pipe));
    if (buf.len < 0) {
        LogError("Lost connection to server");
        return false;
    }

    const auto read = [&](Span<uint8_t> out_buf) {
        if (buf.len) {
            Size copy_len = std::min(buf.len, out_buf.len);
            MemCpy(out_buf.ptr, buf.ptr, copy_len);

            buf.ptr += copy_len;
            buf.len -= copy_len;

            return copy_len;
        }

        Size received = ReadSync(pipe, out_buf.ptr, out_buf.len, -1);
        if (received < 0) {
            LogError("Failed to receive data from server: %1", strerror(errno));
        }
        return received;
    };

    StreamReader reader(read, "<client>");
    handle_func(&reader);

    if (!reader.Close())
        return false;

    if (!StartRead(pipe)) {
        LogError("Lost connection to server");
        return false;
    }

    return true;
}

bool TowerClient::Send(FunctionRef<void(StreamWriter *)> func)
{
    const auto write = [&](Span<const uint8_t> buf) {
        while (buf.len) {
            Size sent = WriteSync(pipe, buf.ptr, buf.len);
            if (sent < 0) {
                LogError("Failed to send data to server: %1", GetWin32ErrorString());
                return false;
            }

            buf.ptr += sent;
            buf.len -= sent;
        }

        return true;
    };

    StreamWriter writer(write, "<server>");
    func(&writer);

    return writer.Close();
}

const char *GetControlSocketPath(ControlScope scope, const char *name, Allocator *alloc)
{
    K_ASSERT(strlen(name) < 64);

    switch (scope) {
        case ControlScope::System: return Fmt(alloc, "\\\\.\\pipe\\tower\\system\\%1", name).ptr;

        case ControlScope::User: {
            char buf[128] = {};

            ULONG size = K_SIZE(buf);
            BOOL success = GetUserNameExA(NameUniqueId, buf, &size);
            K_CRITICAL(success, "Failed to get user name");

            Span<const char> uuid = MakeSpan(buf, size);
            return Fmt(alloc, "\\\\.\\pipe\\tower\\%1\\%2", TrimStr(uuid, "{}"), name).ptr;
        } break;
    }

    K_UNREACHABLE();
}

#else

bool TowerServer::Bind(const char *path)
{
    K_ASSERT(fd < 0);

    K_DEFER_N(err_guard) { Stop(); };

    fd = CreateSocket(SocketType::Unix, SOCK_STREAM);
    if (fd < 0)
        return false;
    SetDescriptorNonBlock(fd, true);

    if (!BindUnixSocket(fd, path))
        return false;
    if (listen(fd, 4) < 0) {
        LogError("listen() failed: %1", strerror(errno));
        return false;
    }

    err_guard.Disable();
    return true;
}

void TowerServer::Start(std::function<bool(StreamReader *, StreamWriter *)> func)
{
    K_ASSERT(fd >= 0);
    K_ASSERT(!handle_func);

    sources.Append({ fd, -1 });
    handle_func = func;
}

void TowerServer::Stop()
{
    if (fd >= 0) {
        CloseDescriptor(fd);
        fd = -1;
    }

    for (Size i = 1; i < sources.len; i++) {
        CloseDescriptor(sources[i].fd);
    }
    sources.Clear();

    handle_func = {};
}

static bool IsReadable(int fd, int timeout)
{
    struct pollfd pfd = { fd, POLLIN, 0 };

    if (poll(&pfd, 1, timeout) < 0)
        return true;
    if (pfd.revents)
        return true;

    return false;
}

bool TowerServer::Process(uint64_t ready)
{
    // Accept new clients
    if (ready & 1) {
#if defined(SOCK_CLOEXEC)
        int sock = accept4(fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
        int sock = accept(fd, nullptr, nullptr);
#endif

        if (sock >= 0) {
#if !defined(SOCK_CLOEXEC)
            fcntl(sock, F_SETFD, FD_CLOEXEC);
#endif
#if !defined(MSG_DONTWAIT)
            SetDescriptorNonBlock(sock, true);
#endif

            if (sources.Available()) [[likely]] {
                sources.Append({ sock, -1 });
                LogDebug("Client has connected");
            } else {
                LogError("Too many connections, refusing new client");
                CloseDescriptor(sock);
            }
        } else if (errno != EAGAIN) {
            LogError("Failed to accept client: %1", strerror(errno));
        }
    }

    RunClients([&](Size idx, int sock) {
        if (!(ready & (1ull << idx)))
            return true;

        // Handle disconnection and errors first
        {
            struct pollfd pfd = { sock, POLLIN, 0 };
            K_IGNORE poll(&pfd, 1, 1000);

            if (pfd.revents & (POLLHUP | POLLERR)) {
                LogDebug("Client has disconnected");
                return false;
            }
        }

        const auto read = [&](Span<uint8_t> out_buf) {
            Size received = recv(sock, out_buf.ptr, out_buf.len, 0);
            if (received < 0) {
                if (errno == EAGAIN) {
                    if (IsReadable(sock, 1000)) {
                        received = recv(sock, out_buf.ptr, out_buf.len, 0);
                    } else {
                        LogError("Client has timed out");
                    }
                } else {
                    LogError("Failed to receive data from client: %1", strerror(errno));
                }
            }

            return received;
        };

        const auto write = [&](Span<const uint8_t> buf) {
            while (buf.len) {
                Size sent = send(sock, buf.ptr, (size_t)buf.len, 0);
                if (sent < 0) {
                    LogError("Failed to send data to server: %1", strerror(errno));
                    return false;
                }

                buf.ptr += sent;
                buf.len -= sent;
            }

            return true;
        };

        StreamReader reader(read, "<client>");
        StreamWriter writer(write, "<client>");

        if (!handle_func(&reader, &writer))
            return false;
        if (!reader.Close())
            return false;
        if (!writer.Close())
            return false;

        return true;
    });

    return true;
}

void TowerServer::Send(FunctionRef<void(StreamWriter *)> func)
{
    RunClients([&](Size, int sock) {
        const auto write = [&](Span<const uint8_t> buf) {
            while (buf.len) {
                Size sent = send(sock, buf.ptr, (size_t)buf.len, 0);
                if (sent < 0) {
                    LogError("Failed to send data to server: %1", strerror(errno));
                    return false;
                }

                buf.ptr += sent;
                buf.len -= sent;
            }

            return true;
        };

        StreamWriter writer(write, "<client>");
        func(&writer);

        return writer.Close();
    });
}

void TowerServer::RunClients(FunctionRef<bool(Size, int)> func)
{
    Size j = 1;
    for (Size i = 1; i < sources.len; i++) {
        const WaitSource &src = sources[i];

        sources[j] = src;

        if (!func(i, src.fd)) {
            close(src.fd);
            continue;
        }

        j++;
    }
    sources.len = j;
}

bool TowerClient::Connect(const char *path)
{
    Stop();

    K_DEFER_N(err_guard) { Stop(); };

    sock = CreateSocket(SocketType::Unix, SOCK_STREAM);
    if (sock < 0)
        return false;
    if (!ConnectUnixSocket(sock, path))
        return false;

    err_guard.Disable();
    return true;
}

void TowerClient::Start(std::function<void(StreamReader *)> func)
{
    K_ASSERT(sock >= 0);
    K_ASSERT(!handle_func);

    src = { sock, -1 };
    handle_func = func;
}

void TowerClient::Stop()
{
    CloseDescriptor(sock);
    sock = -1;

    handle_func = {};
}

bool TowerClient::Process()
{
    // We need to poll because StreamReader does not support non-blocking reads,
    // so make sure there's data on the other end. The caller probably knows and may
    // have skipped the call to Process but we don't want to enforce this; Process()
    // should work and do nothing if there's nothing to do.
    if (!IsReadable(sock, 0))
        return true;

    const auto read = [&](Span<uint8_t> out_buf) {
        Size received = recv(sock, out_buf.ptr, out_buf.len, 0);
        if (received < 0) {
            LogError("Failed to receive data from server: %1", strerror(errno));
        }
        return received;
    };

    StreamReader reader(read, "<client>");
    handle_func(&reader);

    return reader.Close();
}

bool TowerClient::Send(FunctionRef<void(StreamWriter *)> func)
{
    const auto write = [&](Span<const uint8_t> buf) {
        while (buf.len) {
            Size sent = send(sock, buf.ptr, (size_t)buf.len, 0);
            if (sent < 0) {
                LogError("Failed to send data to server: %1", strerror(errno));
                return false;
            }

            buf.ptr += sent;
            buf.len -= sent;
        }

        return true;
    };

    StreamWriter writer(write, "<server>");
    func(&writer);

    return writer.Close();
}

const char *GetControlSocketPath(ControlScope scope, const char *name, Allocator *alloc)
{
    K_ASSERT(strlen(name) < 64);

    switch (scope) {
        case ControlScope::System: {
            const char *prefix = TestFile("/run", FileType::Directory) ? "/run" : "/var/run";
            return Fmt(alloc, "%1/%2.sock", prefix, name).ptr;
        } break;

        case ControlScope::User: {
            const char *xdg = GetEnv("XDG_RUNTIME_DIR");
            const char *path = nullptr;

            if (xdg) {
                path = Fmt(alloc, "%1/%2.sock", xdg, name).ptr;
            } else {
                const char *prefix = TestFile("/run", FileType::Directory) ? "/run" : "/var/run";
                uid_t uid = getuid();

                path = Fmt(alloc, "%1/%2/%3.sock", prefix, uid, name).ptr;
            }

            // Best effort
            EnsureDirectoryExists(path);

            return path;
        } break;
    }

    K_UNREACHABLE();
}

#endif

}
