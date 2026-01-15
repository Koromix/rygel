// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__linux__)

#include "lib/native/base/base.hh"
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

namespace K {

struct http_Socket {
    int sock = -1;
    bool process = false;

    http_IO client;

    http_Socket(http_Daemon *daemon) : client(daemon) {}
    ~http_Socket() { CloseDescriptor(sock); }
};

static const int WorkersPerDispatcher = 4;
static const Size MaxSend = Mebibytes(2);

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

    void StopWS();

    friend class http_Daemon;
};

bool http_Daemon::Start(std::function<void(http_IO *io)> func)
{
    K_ASSERT(listeners.len);
    K_ASSERT(!handle_func);
    K_ASSERT(func);

    async = new Async(1 + listeners.len);

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
        Size len = std::min(buf.len, MaxSend);
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
    static_assert(K_SIZE(Span<const uint8_t>) == K_SIZE(struct iovec));
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
    K_ASSERT(socket);
    K_ASSERT(!response.started);

    K_DEFER { close(fd); };

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
    bool cork = (len >= MaxSend);

    if (cork) {
        SetDescriptorRetain(socket->sock, true);
    }
    K_DEFER {
        if (cork) {
            SetDescriptorRetain(socket->sock, false);
        }
    };

    if (!daemon->WriteSocket(socket, intro.As<uint8_t>())) {
        request.keepalive = false;
        return;
    }

    off_t offset = 0;
    int64_t remain = len;

    while (remain) {
        Size send = (Size)std::min(remain, (int64_t)MaxSend);
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
    K_ASSERT(epoll_fd < 0);

    Async async(1 + WorkersPerDispatcher);

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        LogError("Failed to initialize epoll: %1", strerror(errno));
        return false;
    }
    K_DEFER {
        CloseDescriptor(epoll_fd);
        epoll_fd = -1;
    };

    // Delete remaining clients when function exits
    K_DEFER {
        StopWS();

        if (!async.Wait(100)) {
            LogInfo("Waiting up to %1 sec before shutting down clients...", (double)daemon->stop_timeout / 1000);

            int64_t start = GetMonotonicTime();

            do {
                StopWS();

                if (async.Wait(100))
                    break;
            } while (GetMonotonicTime() - start < daemon->stop_timeout);

            for (http_Socket *socket: sockets) {
                shutdown(socket->sock, SHUT_RDWR);
            }
            async.Sync();
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
            socklen_t ss_len = K_SIZE(ss);

            // Accept queued clients
            for (int i = 0; i < 8; i++) {
                int sock = accept4(listener, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);

                if (sock < 0) {
                    static_assert(EAGAIN == EWOULDBLOCK);

                    if (errno == EAGAIN)
                        break;
                    if (errno == EINVAL)
                        return true;

                    // Assume transient error (such as too many open files)
                    LogError("Failed to accept client: %1", strerror(errno));
                    WaitDelay(20);

                    break;
                }

                http_Socket *socket = InitSocket(sock, now, (sockaddr *)&ss);

                if (!socket) [[unlikely]] {
                    close(sock);
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
                Size bytes = recv(socket->sock, client->incoming.buf.ptr, available, MSG_DONTWAIT);

                if (bytes > 0) {
                    client->incoming.buf.len += bytes;
                    client->incoming.buf.ptr[client->incoming.buf.len] = 0;

                    status = client->ParseRequest();
                } else if (!bytes || errno != EAGAIN) {
                    if (client->IsBusy()) {
                        if (bytes) {
                            LogError("Connection failed: %1", strerror(errno));
                        } else {
                            LogError("Connection closed unexpectedly");
                        }
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

                        AddEpollDescriptor(socket->sock, EPOLLIN, socket);

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

    K_UNREACHABLE();
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

    K_DEFER_N(err_guard) { delete socket; };

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

void http_Dispatcher::StopWS()
{
    for (http_Socket *socket: sockets) {
        // Slight data race but it is harmless given the context
        if (socket->client.ws_opcode) {
            shutdown(socket->sock, SHUT_RDWR);
        }
    }
}

}

#endif
