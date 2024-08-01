// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
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
#include "posix_priv.hh"

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

namespace RG {

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

bool http_Daemon::Bind(const http_Config &config, bool log_addr)
{
    RG_ASSERT(!listeners.len);

    if (!InitConfig(config))
        return false;

    int listener = -1;
    RG_DEFER_N(err_guard) { CloseDescriptor(listener); };

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: { listener = OpenIPSocket(config.sock_type, config.port, SOCK_STREAM); } break;
        case SocketType::Unix: { listener = OpenUnixSocket(config.unix_path, SOCK_STREAM); } break;
    }
    if (listener < 0)
        return false;

    if (listen(listener, 200) < 0) {
        LogError("Failed to listen on socket: %1", strerror(errno));
        return false;
    }
    SetSocketNonBlock(listener, true);

    listeners.Append(listener);
    err_guard.Disable();

    if (log_addr) {
        if (config.sock_type == SocketType::Unix) {
            LogInfo("Listening on socket '%!..+%1%!0' (Unix stack)", config.unix_path);
        } else {
            LogInfo("Listening on %!..+http://localhost:%1/%!0 (%2 stack)",
                    config.port, SocketTypeNames[(int)config.sock_type]);
        }
    }

    return true;
}

bool http_Daemon::Start(std::function<void(const http_RequestInfo &request, http_IO *io)> func)
{
    RG_ASSERT(listeners.len == 1);
    RG_ASSERT(!handle_func);
    RG_ASSERT(func);

    async = new Async(1 + 2 * GetCoreCount());

    handle_func = func;

    // Run request dispatchers
    for (int i = 1; i < async->GetWorkerCount(); i++) {
        http_Dispatcher *dispatcher = new http_Dispatcher(this, this->dispatcher, listeners[0]);
        this->dispatcher = dispatcher;

        async->Run([=] { return dispatcher->Run(); });
    }

    return true;
}

void http_Daemon::Stop()
{
    // Shut everything down
    for (int listener: listeners) {
        shutdown(listener, SHUT_RD);
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

void http_Daemon::EndWrite(http_Socket *)
{
    // Nothing to do
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
        async.Sync();

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

        for (const struct epoll_event &ev: events) {
            if (!ev.data.fd) {
                if (ev.events & EPOLLHUP) [[unlikely]]
                    return true;

                sockaddr_storage ss;
                socklen_t ss_len = RG_SIZE(ss);

                // Accept queued clients
                for (int i = 0; i < 8; i++) {
                    int sock = accept4(listener, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);

                    if (sock < 0) {
                        if (errno == EINVAL)
                            return true;
                        if (errno == EAGAIN)
                            break;

                        LogError("Failed to accept client: %1 %2", strerror(errno), errno);
                        return false;
                    }

                    http_Socket *socket = InitSocket(sock, now, (sockaddr *)&ss);

                    if (!socket) [[unlikely]] {
                        close(sock);
                        continue;
                    }

                    sockets.Append(socket);
                }
            } else {
                http_Socket *socket = (http_Socket *)ev.data.ptr;
                socket->process = true;
            }
        }

        Size keep = 0;
        unsigned int timeout = UINT_MAX;

        // Process clients
        for (Size i = 0; i < sockets.len; i++, keep++) {
            sockets[keep] = sockets[i];

            http_Socket *socket = sockets[i];
            http_IO *client = &socket->client;

            const auto disconnect = [&]() {
                ParkSocket(socket);
                keep--;
            };

            http_RequestStatus status = http_RequestStatus::Incomplete;

            if (socket->process) {
                socket->process = false;

                if (!client->working) [[likely]] {
                    client->incoming.buf.Grow(Kibibytes(8));

                    Size available = client->incoming.buf.Available() - 1;
                    Size bytes = recv(socket->sock, client->incoming.buf.ptr, available, MSG_DONTWAIT);

                    if (bytes > 0) {
                        client->incoming.buf.len += bytes;
                        client->incoming.buf.ptr[client->incoming.buf.len] = 0;

                        status = client->ParseRequest();
                    } else if (!bytes) {
                        if (!client->IsKeptAlive()) {
                            LogError("Connection closed unexpectedly");
                        }
                        status = http_RequestStatus::Close;
                    } else if (errno != EAGAIN) {
                        LogError("Connection failed: %1", strerror(errno));
                        status = http_RequestStatus::Close;
                    }
                } else {
                    status = http_RequestStatus::Busy;
                }
            }

            switch (status) {
                case http_RequestStatus::Incomplete: {
                    int64_t delay = std::max((int64_t)0, client->GetTimeout(now));
                    timeout = std::min(timeout, (unsigned int)delay);
                } break;

                case http_RequestStatus::Ready: {
                    if (!client->InitAddress()) {
                        client->request.keepalive = false;
                        client->SendError(400);
                        disconnect();

                        break;
                    }

                    client->request.keepalive &= (now < client->socket_start + daemon->keepalive_time);

                    int worker_idx = 1 + next_worker;
                    next_worker = (next_worker + 1) % WorkersPerDispatcher;

                    DeleteEpollDescriptor(socket->sock);

                    if (client->request.keepalive) {
                        async.Run(worker_idx, [=, this] {
                            daemon->RunHandler(client);
                            client->Rearm(now);

                            AddEpollDescriptor(socket->sock, EPOLLIN | EPOLLET, socket);

                            return true;
                        });
                    } else {
                        async.Run(worker_idx, [=, this] {
                            daemon->RunHandler(client);
                            client->Rearm(-1);

                            shutdown(socket->sock, SHUT_RD);
                            AddEpollDescriptor(socket->sock, EPOLLIN, socket);

                            return true;
                        });
                    }
                } break;

                case http_RequestStatus::Busy: { /* Should be rare */ } break;

                case http_RequestStatus::Close: { disconnect(); } break;
            }
        }
        sockets.len = keep;

        events.RemoveFrom(0);
        events.AppendDefault(2 + sockets.len);

        // The timeout is unsigned to make it easier to use with std::min() without dealing
        // with the default value -1. If it stays at UINT_MAX, the (int) cast results in -1.
        int ready = epoll_wait(epoll_fd, events.ptr, events.len, (int)timeout);

        if (ready < 0 && errno != EINTR) [[unlikely]] {
            LogError("Failed to poll descriptors: %1", strerror(errno));
            return false;
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
    if (!AddEpollDescriptor(sock, EPOLLIN | EPOLLET, socket)) [[unlikely]]
        return nullptr;

    err_guard.Disable();
    return socket;
}

void http_Dispatcher::ParkSocket(http_Socket *socket)
{
    DeleteEpollDescriptor(socket->sock);

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
