// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "base.hh"
#include "tower.hh"

#if !defined(_WIN32)
    #include <poll.h>
    #include <sys/socket.h>
#endif

namespace K {

#if !defined(_WIN32)

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

static Size DoForClients(Span<WaitSource> sources, Size offset, FunctionRef<bool(Size idx, int fd)> func)
{
    K_ASSERT(sources.len >= offset);

    Size j = offset;
    for (Size i = offset; i < sources.len; i++) {
        const WaitSource &src = sources[i];

        sources[j] = src;

        if (!func(i, src.fd)) {
            close(src.fd);
            continue;
        }

        j++;
    }
    return j;
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

void TowerServer::Process(uint64_t ready)
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

    sources.len = DoForClients(sources, 1, [&](Size idx, int sock) {
        if (!(ready & (1u << idx)))
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
}

void TowerServer::Send(FunctionRef<void(StreamWriter *)> func)
{
    sources.len = DoForClients(sources, 1, [&](Size, int sock) {
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

#endif

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

}
