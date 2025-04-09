// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#if defined(__linux__)

#include "src/core/base/base.hh"
#include "server.hh"

#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>

namespace RG {

struct http_Socket {
    int sock = -1;
    bool process = false;

    http_IO client;

    http_Socket(http_Daemon *daemon) : client(daemon) {}
    ~http_Socket() { CloseDescriptor(sock); }
};

static const int WorkersPerDispatcher = 4;

class http_Dispatcher {
    http_Daemon *daemon;
    http_Dispatcher *next;

    int listener;

    int epoll_fd = -1;

    HeapArray<http_Socket *> sockets;
    LocalArray<http_Socket *, 64> free_sockets;

public:
    http_Dispatcher(http_Daemon *daemon, http_Dispatcher *next, int listener)
        : daemon(daemon), next(next), listener(listener) {}

    bool Run();

private:
    http_Socket *InitSocket(int sock, int64_t start, struct sockaddr *sa);
    void ParkSocket(http_Socket *socket);

    bool AddEpollDescriptor(int fd, uint32_t events, void *ptr);
    void DeleteEpollDescriptor(int fd);

    friend class http_Daemon;
};

static int CreateListenSocket(const http_Config &config)
{
    int sock = CreateSocket(config.sock_type, SOCK_STREAM);
    if (sock < 0)
        return -1;
    RG_DEFER_N(err_guard) { close(sock); };

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: {
            if (!BindIPSocket(sock, config.sock_type, config.port))
                return -1;
        } break;
        case SocketType::Unix: {
            if (!BindUnixSocket(sock, config.unix_path))
                return -1;
        } break;
    }

    if (listen(sock, 200) < 0) {
        LogError("Failed to listen on socket: %1", strerror(errno));
        return -1;
    }

    SetDescriptorNonBlock(sock, true);

    err_guard.Disable();
    return sock;
}

bool http_Daemon::Bind(const http_Config &config, bool log_addr)
{
    RG_ASSERT(!listeners.len);

    if (!InitConfig(config))
        return false;

    RG_DEFER_N(err_guard) {
        for (int listener: listeners) {
            close(listener);
        }
        listeners.Clear();
    };

    Size workers = 2 * GetCoreCount();

    for (Size i = 0; i < workers; i++) {
        int listener = CreateListenSocket(config);
        if (listener < 0)
            return false;
        listeners.Append(listener);
    }

    if (log_addr) {
        if (config.sock_type == SocketType::Unix) {
            LogInfo("Listening on socket '%!..+%1%!0' (Unix stack)", config.unix_path);
        } else {
            LogInfo("Listening on %!..+http://localhost:%1/%!0 (%2 stack)",
                    config.port, SocketTypeNames[(int)config.sock_type]);
        }
    }

    err_guard.Disable();
    return true;
}

bool http_Daemon::Start(std::function<void(http_IO *io)> func)
{
    RG_ASSERT(listeners.len);
    RG_ASSERT(!handle_func);
    RG_ASSERT(func);

    async = new Async(1 + listeners.len);

    handle_func = func;

    // Run request dispatchers
    for (int listener: listeners) {
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
        shutdown(listener, SHUT_RDWR);
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

void http_Daemon::StartRead(http_Socket *)
{
    // Nothing to do
}

void http_Daemon::StartWrite(http_Socket *)
{
    // Nothing to do
}

void http_Daemon::EndWrite(http_Socket *socket)
{
    SetDescriptorRetain(socket->sock, false);
}

Size http_Daemon::ReadSocket(http_Socket *socket, Span<uint8_t> buf)
{
restart:
    Size bytes = recv(socket->sock, buf.ptr, (size_t)buf.len, 0);

    if (bytes < 0) {
        if (errno == EINTR)
            goto restart;

        if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
            LogError("Failed to read from client: %1", strerror(errno));
        }

        socket->client.request.keepalive = false;
        return -1;
    }

    socket->client.timeout_at = GetMonotonicTime() + idle_timeout;

    return bytes;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<const uint8_t> buf)
{
    int flags = MSG_NOSIGNAL | MSG_MORE;

    while (buf.len) {
        Size len = std::min(buf.len, Mebibytes(2));
        Size bytes = send(socket->sock, buf.ptr, len, flags);

        if (bytes < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }

            socket->client.request.keepalive = false;
            return false;
        }

        socket->client.timeout_at = GetMonotonicTime() + send_timeout;

        buf.ptr += bytes;
        buf.len -= bytes;
    }

    return true;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<Span<const uint8_t>> parts)
{
    static_assert(RG_SIZE(Span<const uint8_t>) == RG_SIZE(struct iovec));
    static_assert(alignof(Span<const uint8_t>) == alignof(struct iovec));
    static_assert(offsetof(Span<const uint8_t>, ptr) == offsetof(struct iovec, iov_base));
    static_assert(offsetof(Span<const uint8_t>, len) == offsetof(struct iovec, iov_len));

    struct msghdr msg = {
        .msg_name = nullptr,
        .msg_namelen = 0,
        .msg_iov = (struct iovec *)parts.ptr,
        .msg_iovlen = (decltype(msghdr::msg_iovlen))parts.len,
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0
    };
    int flags = MSG_NOSIGNAL | MSG_MORE;

    while (msg.msg_iovlen) {
        Size sent = sendmsg(socket->sock, &msg, flags);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }

            socket->client.request.keepalive = false;
            return false;
        }

        socket->client.timeout_at = GetMonotonicTime() + send_timeout;

        do {
            struct iovec *part = msg.msg_iov;

            if (part->iov_len > (size_t)sent) {
                part->iov_base = (uint8_t *)part->iov_base + sent;
                part->iov_len -= (size_t)sent;

                break;
            }

            msg.msg_iov++;
            msg.msg_iovlen--;
            sent -= (Size)part->iov_len;
        } while (msg.msg_iovlen);
    }

    return true;
}

void http_IO::SendFile(int status, int fd, int64_t len)
{
    RG_ASSERT(socket);
    RG_ASSERT(!response.started);

    RG_DEFER { close(fd); };

    response.started = true;

    if (len < 0) {
        struct stat sb;
        if (fstat(fd, &sb) < 0) {
            LogError("Cannot get file size: %1", strerror(errno));

            request.keepalive = false;
            return;
        }

        len = (int64_t)sb.st_size;
    }

    Span<const char> intro = PrepareResponse(status, CompressionType::None, len);

    if (!daemon->WriteSocket(socket, intro.As<uint8_t>())) {
        request.keepalive = false;
        return;
    }

    off_t offset = 0;
    int64_t remain = len;

    while (remain) {
        Size send = (Size)std::min(remain, (int64_t)Mebibytes(2));
        Size sent = sendfile(socket->sock, fd, &offset, (size_t)send);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send file: %1", strerror(errno));
            }

            request.keepalive = false;
            return;
        }

        if (!sent) [[unlikely]] {
            LogError("Truncated file sent");

            request.keepalive = false;
            return;
        }

        socket->client.timeout_at = GetMonotonicTime() + daemon->send_timeout;

        remain -= sent;
    }
}

bool http_Dispatcher::Run()
{
    RG_ASSERT(epoll_fd < 0);

    Async async(1 + WorkersPerDispatcher);

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        LogError("Failed to initialize epoll: %1", strerror(errno));
        return false;
    }
    RG_DEFER {
        CloseDescriptor(epoll_fd);
        epoll_fd = -1;
    };

    // Delete remaining clients when function exits
    RG_DEFER {
        if (!async.Wait(100)) {
            LogInfo("Waiting up to %1 sec before shutting down clients...", (double)daemon->stop_timeout / 1000);

            if (!async.Wait(daemon->stop_timeout)) {
                for (http_Socket *socket: sockets) {
                    shutdown(socket->sock, SHUT_RDWR);
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

    AddEpollDescriptor(listener, EPOLLIN | EPOLLEXCLUSIVE, nullptr);

    HeapArray<struct epoll_event> events;
    int next_worker = 0;

    for (;;) {
        int64_t now = GetMonotonicTime();
        bool accepts = false;

        for (const struct epoll_event &ev: events) {
            if (!ev.data.fd) {
                if (ev.events & EPOLLHUP) [[unlikely]]
                    return true;

                accepts = true;
            } else {
                http_Socket *socket = (http_Socket *)ev.data.ptr;
                socket->process = true;
            }
        }

        // Process new connections
        if (accepts) {
            sockaddr_storage ss;
            socklen_t ss_len = RG_SIZE(ss);

            // Accept queued clients
            for (int i = 0; i < 8; i++) {
                int sock = accept4(listener, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);

                if (sock < 0) {
                    static_assert(EAGAIN == EWOULDBLOCK);

                    if (errno == EAGAIN)
                        break;
                    if (errno == EINVAL)
                        return true;

                    LogError("Failed to accept client: %1", strerror(errno));
                    return false;
                }

                http_Socket *socket = InitSocket(sock, now, (sockaddr *)&ss);

                if (!socket) [[unlikely]] {
                    close(sock);
                    continue;
                }

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
                Size bytes = recv(socket->sock, client->incoming.buf.ptr, available, MSG_DONTWAIT);

                if (bytes > 0) {
                    client->incoming.buf.len += bytes;
                    client->incoming.buf.ptr[client->incoming.buf.len] = 0;

                    status = client->ParseRequest();
                } else if (!bytes || errno != EAGAIN) {
                    if (client->IsBusy()) {
                        const char *reason = bytes ? strerror(errno) : "closed unexpectedly";
                        LogError("Client connection failed: %1", reason);
                    }

                    status = http_RequestStatus::Close;
                }
            }

            switch (status) {
                case http_RequestStatus::Busy: { /* Do nothing */ } break;

                case http_RequestStatus::Ready: {
                    int worker_idx = 1 + next_worker;
                    next_worker = (next_worker + 1) % WorkersPerDispatcher;

                    DeleteEpollDescriptor(socket->sock);

                    async.Run(worker_idx, [=, this] {
                        do {
                            daemon->RunHandler(client, now);

                            if (!client->Rearm(GetMonotonicTime())) {
                                shutdown(socket->sock, SHUT_RD);
                                break;
                            }
                        } while (client->ParseRequest() == http_RequestStatus::Ready);

                        AddEpollDescriptor(socket->sock, EPOLLIN | EPOLLET, socket);

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
                shutdown(socket->sock, SHUT_RDWR);
                continue;
            }

            timeout = std::min(timeout, (unsigned int)delay);
        }
        sockets.len = keep;

        events.RemoveFrom(0);
        events.AppendDefault(2 + sockets.len);

        // The timeout is unsigned to make it easier to use with std::min() without dealing
        // with the default value -1. If it stays at UINT_MAX, the (int) cast results in -1.
        int ready = epoll_wait(epoll_fd, events.ptr, events.len, (int)timeout);

        if (ready < 0) {
            if (errno != EINTR) {
                LogError("Failed to poll descriptors: %1", strerror(errno));
                return false;
            }

            ready = 0;
        }

        events.len = ready;
    }

    RG_UNREACHABLE();
}

http_Socket *http_Dispatcher::InitSocket(int sock, int64_t start, struct sockaddr *sa)
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

    socket->sock = sock;

    RG_DEFER_N(err_guard) { delete socket; };

    if (!socket->client.Init(socket, start, sa)) [[unlikely]]
        return nullptr;
    if (!AddEpollDescriptor(sock, EPOLLIN, socket)) [[unlikely]]
        return nullptr;

    err_guard.Disable();
    return socket;
}

void http_Dispatcher::ParkSocket(http_Socket *socket)
{
    if (free_sockets.Available()) {
        close(socket->sock);
        socket->sock = -1;

        socket->client.socket = nullptr;
        socket->client.Rearm(-1);

        free_sockets.Append(socket);
    } else {
        delete socket;
    }
}

bool http_Dispatcher::AddEpollDescriptor(int fd, uint32_t events, void *ptr)
{
    struct epoll_event ev = { events, { .ptr = ptr }};

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0 && errno != EEXIST) {
        LogError("Failed to add descriptor to epoll: %1", strerror(errno));
        return false;
    }

    return true;
}

void http_Dispatcher::DeleteEpollDescriptor(int fd)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
}

}

#endif
