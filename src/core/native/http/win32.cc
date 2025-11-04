// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if defined(_WIN32)

#include "src/core/native/base/base.hh"
#include "server.hh"

#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <io.h>

namespace K {

struct http_Socket {
    int sock = -1;
    bool process = false;

    http_IO client;

    http_Socket(http_Daemon *daemon) : client(daemon) {}
    ~http_Socket() { CloseSocket(sock); }
};

static const int WorkersPerDispatcher = 4;
static const Size MaxSend = Mebibytes(2);

class http_Dispatcher {
    http_Daemon *daemon;
    http_Dispatcher *next;

    int listener;

    int pair_fd[2] = { -1, -1 };

    HeapArray<http_Socket *> sockets;
    LocalArray<http_Socket *, 64> free_sockets;

public:
    http_Dispatcher(http_Daemon *daemon, http_Dispatcher *next, int listener)
        : daemon(daemon), next(next), listener(listener) {}

    bool Run();
    void Wake(http_Socket *socket);

private:
    http_Socket *InitSocket(SOCKET sock, int64_t start, struct sockaddr *sa);
    void ParkSocket(http_Socket *socket);

    friend class http_Daemon;
};

bool http_Daemon::Start(std::function<void(http_IO *io)> func)
{
    K_ASSERT(listeners.len);
    K_ASSERT(!handle_func);
    K_ASSERT(func);

    async = new Async(1 + (int)listeners.len);

    handle_func = func;

    // Run request dispatchers
    for (Size i = 0; i < workers; i++) {
        int listener = listeners[i % listeners.len];

        http_Dispatcher *dispatcher = new http_Dispatcher(this, this->dispatcher, listener);
        this->dispatcher = dispatcher;

        async->Run([=] { return dispatcher->Run(); });
    }

    return true;
}

void http_Daemon::Stop()
{
    // Shut everything down
    for (int listener: listeners) {
        shutdown(listener, SD_BOTH);
    }

    // On Windows, the shutdown() does not wake up poll() so use the pipe to wake it up
    // and signal the ongoing shutdown.
    for (http_Dispatcher *it = dispatcher; it; it = it->next) {
        it->Wake(nullptr);
    }

    if (async) {
        async->Sync();

        delete async;
        async = nullptr;
    }

    while (dispatcher) {
        http_Dispatcher *next = dispatcher->next;
        delete dispatcher;
        dispatcher = next;
    }

    for (int listener: listeners) {
        CloseSocket(listener);
    }
    listeners.Clear();

    handle_func = {};
}

void http_Daemon::StartRead(http_Socket *socket)
{
    SetDescriptorNonBlock(socket->sock, false);
}

void http_Daemon::StartWrite(http_Socket *socket)
{
    SetDescriptorNonBlock(socket->sock, false);
}

void http_Daemon::EndWrite(http_Socket *)
{
    // Nothing to do
}

Size http_Daemon::ReadSocket(http_Socket *socket, Span<uint8_t> buf)
{
    int len = (int)std::min(buf.len, (Size)INT_MAX);
    Size bytes = recv((SOCKET)socket->sock, (char *)buf.ptr, len, 0);

    if (bytes < 0) {
        int error = GetLastError();
        if (error != WSAENOTCONN && error != WSAECONNRESET) {
            LogError("Failed to read from client: %1", GetWin32ErrorString(error));
        }
        return -1;
    }

    return bytes;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<const uint8_t> buf)
{
    while (buf.len) {
        int len = (int)std::min(buf.len, MaxSend);
        int bytes = send(socket->sock, (char *)buf.ptr, len, 0);

        if (bytes < 0) {
            int error = GetLastError();
            if (error != WSAENOTCONN && error != WSAECONNRESET) {
                LogError("Failed to send to client: %1", GetWin32ErrorString(error));
            }
            return false;
        }

        buf.ptr += bytes;
        buf.len -= bytes;
    }

    return true;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<Span<const uint8_t>> parts)
{
    while (parts.len) {
        LocalArray<WSABUF, 64> bufs;
        bufs.len = std::min(parts.len, bufs.Available());

        for (Size i = 0; i < bufs.len; i++) {
            const Span<const uint8_t> &part = parts[i];

            if (part.len > (Size)INT_MAX) [[unlikely]] {
                LogError("Cannot proceed with excessive scattered chunk size");
                return false;
            }

            bufs[i].buf = (char *)part.ptr;
            bufs[i].len = (unsigned long)part.len;
        }

        DWORD sent = 0;
        int ret = WSASend((SOCKET)socket->sock, bufs.data, (DWORD)bufs.len, &sent, 0, nullptr, nullptr);

        if (ret) {
            int error = GetLastError();
            if (error != WSAENOTCONN && error != WSAECONNRESET) {
                LogError("Failed to send to client: %1", GetWin32ErrorString(error));
            }
            return false;
        }

        // Windows does not apparently do partial writes, so don't bother dealing with that.
        // Go on!

        parts.ptr += bufs.len;
        parts.len -= bufs.len;
    }

    return true;
}

static bool CreateSocketPair(int out_pair[2])
{
    SOCKET listener = INVALID_SOCKET;
    SOCKET client = INVALID_SOCKET;
    SOCKET peer = INVALID_SOCKET;

    K_DEFER {
        closesocket(listener);
        closesocket(client);
        closesocket(peer);
    };

    sockaddr_in addr = {};
    socklen_t addr_len = K_SIZE(addr);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
        goto error;
    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET)
        goto error;

    // Set reuse flag
    {
        int reuse = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, K_SIZE(reuse));
    }

    if (bind(listener, (struct sockaddr *)&addr, K_SIZE(addr)) < 0)
        goto error;
    if (getsockname(listener, (struct sockaddr *)&addr, &addr_len) < 0)
        goto error;
    if (listen(listener, 1) < 0)
        goto error;
    if (connect(client, (struct sockaddr *)&addr, K_SIZE(addr)) < 0)
        goto error;

    peer = accept(listener, nullptr, nullptr);
    if (peer == INVALID_SOCKET)
        goto error;

    // Success!
    out_pair[0] = (int)client;
    out_pair[1] = (int)peer;
    client = INVALID_SOCKET;
    peer = INVALID_SOCKET;

    return true;

error:
    LogError("Failed to create socket pair: %1", GetWin32ErrorString());
    return false;
}

bool http_Dispatcher::Run()
{
    Async async(1 + WorkersPerDispatcher);

    if (!CreateSocketPair(pair_fd))
        return false;
    K_DEFER {
        CloseSocket(pair_fd[0]);
        CloseSocket(pair_fd[1]);

        pair_fd[0] = -1;
        pair_fd[1] = -1;
    };

    // Delete remaining clients when function exits
    K_DEFER {
        if (!async.Wait(100)) {
            LogInfo("Waiting up to %1 sec before shutting down clients...", (double)daemon->stop_timeout / 1000);

            if (!async.Wait(daemon->stop_timeout)) {
                for (http_Socket *socket: sockets) {
                    shutdown(socket->sock, SD_BOTH);
                }
                async.Sync();
            }
        }

        for (http_Socket *socket: sockets) {
            delete socket;
        }
        for (http_Socket *socket: free_sockets) {
            delete socket;
        }

        sockets.Clear();
        free_sockets.Clear();
    };

    HeapArray<struct pollfd> pfds;
    int next_worker = 0;

    // React to connections
    pfds.Append({ (SOCKET)listener, POLLIN, 0 });
    pfds.Append({ (SOCKET)pair_fd[0], POLLIN, 0 });

    for (;;) {
        int64_t now = GetMonotonicTime();
        bool accepts = false;

        // Handle poll events
        if (pfds[0].revents) {
            if (pfds[0].revents & POLLHUP) [[unlikely]]
                return true;

            accepts = true;
        }
        if (pfds[1].revents) {
            uintptr_t addr = 0;
            Size ret = recv(pair_fd[0], (char *)&addr, K_SIZE(addr), 0);

            if (ret <= 0)
                break;
            K_ASSERT(ret == K_SIZE(void *));

            http_Socket *socket = (http_Socket *)addr;

            if (!socket) [[unlikely]]
                return true;

            SetDescriptorNonBlock(socket->sock, true);

            sockets.Append(socket);
        }
        for (Size i = 2; i < pfds.len; i++) {
            struct pollfd &pfd = pfds[i];

            if (pfd.revents) {
                http_Socket *socket = sockets[i - 2];
                socket->process = true;
            }
        }

        // Process new connections
        if (accepts) {
            sockaddr_storage ss;
            socklen_t ss_len = K_SIZE(ss);

            // Accept queued clients
            for (int i = 0; i < 1; i++) {
                SOCKET sock = accept(listener, (sockaddr *)&ss, &ss_len);

                if (sock == INVALID_SOCKET) {
                    int error = GetLastError();

                    if (error == WSAEWOULDBLOCK)
                        break;
                    if (error == WSAEINVAL)
                        return true;

                    LogError("Failed to accept client: %1", GetWin32ErrorString());
                    return false;
                }

                SetDescriptorNonBlock((int)sock, true);

                http_Socket *socket = InitSocket(sock, now, (sockaddr *)&ss);

                if (!socket) [[unlikely]] {
                    closesocket(sock);
                    continue;
                }

                // Try to read without waiting for more performance
                socket->process = true;

                sockets.Append(socket);
            }
        }

        Size keep = 0;
        unsigned int timeout = UINT_MAX;

        // Process clients
        for (Size i = 0; i < sockets.len; i++, keep++) {
            sockets[keep] = sockets[i];

            http_Socket *socket = sockets[i];
            http_IO *client = &socket->client;
            http_RequestStatus status = http_RequestStatus::Busy;

            if (socket->process) {
                socket->process = false;

                client->incoming.buf.Grow(Kibibytes(8));

                Size available = client->incoming.buf.Available() - 1;
                Size bytes = recv(socket->sock, (char *)client->incoming.buf.ptr, (int)available, 0);

                if (bytes > 0) {
                    client->incoming.buf.len += bytes;
                    client->incoming.buf.ptr[client->incoming.buf.len] = 0;

                    status = client->ParseRequest();
                } else {
                    int error = GetLastError();

                    if (!bytes || error != WSAEWOULDBLOCK) {
                        if (client->IsBusy()) {
                            if (bytes) {
                                LogError("Connection failed: %1", GetWin32ErrorString());
                            } else {
                                LogError("Connection closed unexpectedly");
                            }
                        }

                        status = http_RequestStatus::Close;
                    }
                }
            }

            switch (status) {
                case http_RequestStatus::Busy: { /* Do nothing */ } break;

                case http_RequestStatus::Ready: {
                    int worker_idx = 1 + next_worker;
                    next_worker = (next_worker + 1) % WorkersPerDispatcher;

                    keep--;

                    async.Run(worker_idx, [=, this] {
                        do {
                            daemon->RunHandler(client, now);

                            if (!client->Rearm(GetMonotonicTime())) {
                                shutdown(socket->sock, SD_RECEIVE);
                                break;
                            }
                        } while (client->ParseRequest() == http_RequestStatus::Ready);

                        Wake(socket);

                        return true;
                    });
                } break;

                case http_RequestStatus::Close: {
                    ParkSocket(socket);
                    keep--;

                    continue;
                } break;
            }

            int delay = (int)(client->timeout_at.load() - now);

            if (delay <= 0) {
                shutdown(socket->sock, SD_BOTH);
                continue;
            }

            timeout = std::min(timeout, (unsigned int)delay);
        }
        sockets.len = keep;

        pfds.RemoveFrom(2);

        // Prepare poll descriptors
        for (const http_Socket *socket: sockets) {
            struct pollfd pfd = { (SOCKET)socket->sock, POLLIN, 0 };
            pfds.Append(pfd);
        }

        // The timeout is unsigned to make it easier to use with std::min() without dealing
        // with the default value -1. If it stays at UINT_MAX, the (int) cast results in -1.
        int ready = WSAPoll(pfds.ptr, (unsigned long)pfds.len, (int)timeout);

        if (ready < 0) {
            LogError("Failed to poll descriptors: %1", GetWin32ErrorString());
            return false;
        }
    }

    K_UNREACHABLE();
}

void http_Dispatcher::Wake(http_Socket *socket)
{
    uintptr_t addr = (uintptr_t)socket;
    Size ret = send((SOCKET)pair_fd[1], (char *)&addr, K_SIZE(addr), 0);
    (void)ret;
}

http_Socket *http_Dispatcher::InitSocket(SOCKET sock, int64_t start, struct sockaddr *sa)
{
    http_Socket *socket = nullptr;

    if (free_sockets.len) {
        int idx = GetRandomInt(0, (int)free_sockets.len);

        socket = free_sockets[idx];

        std::swap(free_sockets[idx], free_sockets[free_sockets.len - 1]);
        free_sockets.len--;
    } else {
        socket = new http_Socket(daemon);
    }

    socket->sock = (int)sock;

    K_DEFER_N(err_guard) { delete socket; };

    if (!socket->client.Init(socket, start, sa)) [[unlikely]]
        return nullptr;

    err_guard.Disable();
    return socket;
}

void http_Dispatcher::ParkSocket(http_Socket *socket)
{
    if (free_sockets.Available()) {
        closesocket(socket->sock);
        socket->sock = -1;

        socket->client.socket = nullptr;
        socket->client.Rearm(-1);

        free_sockets.Append(socket);
    } else {
        delete socket;
    }
}

void http_IO::SendFile(int status, int fd, int64_t len)
{
    K_ASSERT(socket);
    K_ASSERT(!response.started);

    response.started = true;

    SetDescriptorNonBlock(socket->sock, false);

    if (len < 0) {
        HANDLE h = (HANDLE)_get_osfhandle(fd);

        BY_HANDLE_FILE_INFORMATION attr;
        if (!GetFileInformationByHandle(h, &attr)) {
            LogError("Cannot get file size: %1", GetWin32ErrorString());

            request.keepalive = false;
            return;
        }

        len = (int64_t)(((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow);
    }

    // Send intro and file in one go
    Span<const char> intro = PrepareResponse(status, CompressionType::None, len);

    if (intro.len >= MaxSend) {
        if (!daemon->WriteSocket(socket, intro.As<uint8_t>())) {
            request.keepalive = false;
            return;
        }

        intro.len = 0;
    }

    HANDLE h = (HANDLE)_get_osfhandle(fd);
    int64_t offset = 0;
    int64_t remain = len;

    // Send intro and start of file
    {
        TRANSMIT_FILE_BUFFERS tbuf = { (void *)intro.ptr, (DWORD)intro.len, nullptr, 0 };
        DWORD send = (DWORD)(std::min(remain, (int64_t)MaxSend) - intro.len);

        if (!TransmitFile((SOCKET)socket->sock, h, send, 0, nullptr, &tbuf, 0)) [[unlikely]] {
            LogError("Failed to send file: %1", GetWin32ErrorString());

            request.keepalive = false;
            return;
        }

        offset += send;
        remain -= send;
    }

    // Send remaining file content
    while (remain) {
        OVERLAPPED ov = {};

        ov.OffsetHigh = (DWORD)(offset >> 32);
        ov.Offset = (DWORD)(offset & 0xFFFFFFFFu);

        DWORD send = (DWORD)std::min(remain, (int64_t)MaxSend);

        if (!TransmitFile((SOCKET)socket->sock, h, send, 0, &ov, nullptr, 0)) [[unlikely]] {
            LogError("Failed to send file: %1", GetWin32ErrorString());

            request.keepalive = false;
            return;
        }

        offset += send;
        remain -= send;
    }
}

}

#endif
