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
#include "misc.hh"

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
#include <sys/eventfd.h>
#include <sys/sendfile.h>

namespace RG {

// Sane platform
static_assert(EAGAIN == EWOULDBLOCK);

static const int WorkersPerDispatcher = 4;

struct http_Socket {
    int sock = -1;
    bool process = false;

    http_IO client;

    http_Socket(http_Daemon *daemon) : client(daemon) {}
    ~http_Socket() { CloseDescriptor(sock); }
};

static_assert(EAGAIN == EWOULDBLOCK);

class http_Dispatcher {
    http_Daemon *daemon;
    http_Dispatcher *next;

    int epoll_fd = -1;

    HeapArray<http_Socket *> sockets;
    LocalArray<http_Socket *, 256> free_sockets;
    HashSet<void *> busy_sockets;

public:
    http_Dispatcher(http_Daemon *daemon, http_Dispatcher *next) : daemon(daemon), next(next) {}

    bool Run();

private:
    http_Socket *InitSocket(int sock, int64_t start, struct sockaddr *sa);
    void ParkSocket(http_Socket *socket);
    void IgnoreSocket(http_Socket *socket);

    bool AddEpollDescriptor(int fd, uint32_t events, int value);
    bool AddEpollDescriptor(int fd, uint32_t events, void *ptr);
    void DeleteEpollDescriptor(int fd);

    friend class http_Daemon;
};

static void SetSocketNonBlock(int sock, bool enable)
{
    int flags = fcntl(sock, F_GETFL, 0);
    flags = ApplyMask(flags, O_NONBLOCK, enable);
    fcntl(sock, F_SETFL, flags);
}

static void SetSocketPush(int sock, bool push)
{
    int flag = !push;
    setsockopt(sock, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));
}

bool http_Daemon::Bind(const http_Config &config, bool log_addr)
{
    RG_ASSERT(listener < 0);

    if (!InitConfig(config))
        return false;

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: { listener = OpenIPSocket(config.sock_type, config.port, SOCK_STREAM); } break;
        case SocketType::Unix: { listener = OpenUnixSocket(config.unix_path, SOCK_STREAM); } break;
    }
    if (listener < 0)
        return false;

    if (listen(listener, 1024) < 0) {
        LogError("Failed to listen on socket: %1", strerror(errno));
        return false;
    }

    SetSocketNonBlock(listener, true);

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
    RG_ASSERT(listener >= 0);
    RG_ASSERT(!handle_func);
    RG_ASSERT(func);

    async = new Async(1 + GetCoreCount());

    handle_func = func;

    // Run request dispatchers
    for (Size i = 1; i < async->GetWorkerCount(); i++) {
        http_Dispatcher *dispatcher = new http_Dispatcher(this, this->dispatcher);
        this->dispatcher = dispatcher;

        async->Run([=] { return dispatcher->Run(); });
    }

    return true;
}

void http_Daemon::Stop()
{
    // Shut everything down
    shutdown(listener, SHUT_RD);

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

    CloseSocket(listener);
    listener = -1;

    handle_func = {};
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
        for (void *ptr: busy_sockets.table) {
            http_Socket *socket = (http_Socket *)ptr;
            delete socket;
        }

        sockets.Clear();
        free_sockets.Clear();
    };

    if (!AddEpollDescriptor(daemon->listener, EPOLLIN | EPOLLEXCLUSIVE, 0))
        return false;

    HeapArray<struct epoll_event> events;
    int next_worker = 0;

    for (;;) {
        int64_t now = GetMonotonicTime();

        for (const struct epoll_event &ev: events) {
            if (ev.data.fd == 0) {
                if (ev.events & EPOLLHUP) [[unlikely]]
                    return true;

                sockaddr_storage ss;
                socklen_t ss_len = RG_SIZE(ss);

                // Accept queued clients
                for (int i = 0; i < 64; i++) {
                    int sock = accept4(daemon->listener, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);

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
                void **ptr = busy_sockets.Find(socket);

                if (ptr) {
                    sockets.Append(socket);
                    busy_sockets.Remove(ptr);
                }

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
            const auto ignore = [&]() {
                IgnoreSocket(socket);
                keep--;
            };

            http_IO::RequestStatus status = socket->process ? client->ProcessIncoming(now)
                                                            : http_IO::RequestStatus::Incomplete;
            socket->process = false;

            switch (status) {
                case http_IO::RequestStatus::Incomplete: {
                    int64_t delay = std::max((int64_t)0, client->GetTimeout(now));
                    timeout = std::min(timeout, (unsigned int)delay);
                } break;

                case http_IO::RequestStatus::Ready: {
                    if (!client->InitAddress()) {
                        client->request.keepalive = false;
                        client->SendError(400);
                        disconnect();

                        break;
                    }

                    client->request.keepalive &= (now < client->socket_start + daemon->keepalive_time);

                    int worker_idx = 1 + next_worker;
                    next_worker = (next_worker + 1) % WorkersPerDispatcher;

                    ignore();

                    if (client->request.keepalive) {
                        async.Run(worker_idx, [=, this] {
                            daemon->RunHandler(client);
                            client->Rearm(now);

                            if (!AddEpollDescriptor(socket->sock, EPOLLIN | EPOLLET, socket)) [[unlikely]] {
                                // It will fail and get collected and closed eventually
                                shutdown(socket->sock, SHUT_RD);
                            }

                            return true;
                        });
                    } else {
                        async.Run(worker_idx, [=, this] {
                            daemon->RunHandler(client);
                            client->Rearm(-1);

                            AddEpollDescriptor(socket->sock, EPOLLIN, socket);
                            shutdown(socket->sock, SHUT_RD);

                            return true;
                        });
                    }
                } break;

                case http_IO::RequestStatus::Close: { disconnect(); } break;
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

        if (!ready) {
            // Process everyone after a timeout
            for (http_Socket *socket: sockets) {
                socket->process = true;
            }
        }
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

void http_Dispatcher::IgnoreSocket(http_Socket *socket)
{
    DeleteEpollDescriptor(socket->sock);
    busy_sockets.Set(socket);
}

bool http_Dispatcher::AddEpollDescriptor(int fd, uint32_t events, int value)
{
    RG_ASSERT(value < 4096);

    struct epoll_event ev = { events, { .fd = value }};

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0 && errno != EEXIST) {
        LogError("Failed to add descriptor to epoll: %1", strerror(errno));
        return false;
    }

    return true;
}

bool http_Dispatcher::AddEpollDescriptor(int fd, uint32_t events, void *ptr)
{
    RG_ASSERT((uintptr_t)ptr >= 4096);

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

void http_IO::Send(int status, CompressionType encoding, int64_t len, FunctionRef<bool(int, StreamWriter *)> func)
{
    RG_ASSERT(!response.sent);

    if (request.headers_only) {
        func = [](int, StreamWriter *) { return true; };
    }

    RG_DEFER {
        response.sent = true;
        SetSocketPush(socket->sock, true);
    };

    const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };
    StreamWriter writer(write, "<http>");

    Span<const char> intro = PrepareResponse(status, encoding, len);
    writer.Write(intro);

    if (len >= 0) {
        if (encoding != CompressionType::None) {
            writer.Close();
            writer.Open(write, "<http>", encoding);
        }

        request.keepalive &= func(socket->sock, &writer);
    } else {
        const auto chunk = [this](Span<const uint8_t> buf) { return WriteChunked(buf); };
        StreamWriter chunker(chunk, "<http>", encoding);

        if (func(-1, &chunker)) {
            request.keepalive &= chunker.Close();
            writer.Write("0\r\n\r\n");
        } else {
            request.keepalive = false;
        }
    }

    request.keepalive &= writer.Close();
}

void http_IO::SendFile(int status, int fd, int64_t len)
{
    RG_DEFER { close(fd); };

    Send(status, len, [&](int sock, StreamWriter *) {
        off_t offset = 0;
        int64_t remain = len;

        while (remain) {
            Size send = (Size)std::min(remain, (int64_t)RG_SIZE_MAX);
            Size sent = sendfile(sock, fd, &offset, (size_t)send);

            if (sent < 0) {
                if (errno == EINTR)
                    continue;

                if (errno != EPIPE) {
                    LogError("Failed to send file: %1", strerror(errno));
                }
                return false;
            }

            remain -= sent;
        }

        return true;
    });
}

http_IO::RequestStatus http_IO::ProcessIncoming(int64_t now)
{
    RG_ASSERT(!ready);

    // Gather request line and headers
    {
        bool complete = false;

        for (;;) {
            incoming.buf.Grow(Mebibytes(1));

            Size available = incoming.buf.Available() - 1;
            Size read = recv(socket->sock, incoming.buf.end(), available, MSG_DONTWAIT);

            incoming.buf.len += std::max(read, (Size)0);
            incoming.buf.ptr[incoming.buf.len] = 0;

            while (incoming.buf.len - incoming.pos >= 4) {
                uint8_t *next = (uint8_t *)memchr(incoming.buf.ptr + incoming.pos, '\r', incoming.buf.len - incoming.pos);
                incoming.pos = next ? next - incoming.buf.ptr : incoming.buf.len;

                if (incoming.pos >= daemon->max_request_size) [[unlikely]] {
                    LogError("Excessive request size");
                    SendError(413);
                    return RequestStatus::Close;
                }

                const char *end = (const char *)incoming.buf.ptr + incoming.pos;

                if (end[0] == '\r' && end[1] == '\n' && end[2] == '\r' && end[3] == '\n') {
                    incoming.intro = incoming.buf.As<char>().Take(0, incoming.pos);
                    incoming.extra = MakeSpan(incoming.buf.ptr + incoming.pos + 4, incoming.buf.len - incoming.pos - 4);

                    complete = true;
                    break;
                } else if (end[0] == '\n' && end[1] == '\n') {
                    incoming.intro = incoming.buf.As<char>().Take(0, incoming.pos);
                    incoming.extra = MakeSpan(incoming.buf.ptr + incoming.pos + 2, incoming.buf.len - incoming.pos - 2);

                    complete = true;
                    break;
                }

                incoming.pos++;
            }
            if (complete)
                break;

            if (read < 0) {
                switch (errno) {
                    // This probably never happens (non-blocking read) but who knows
                    case EINTR: continue;

                    case EAGAIN: {
                        int64_t timeout = GetTimeout(now);

                        if (timeout < 0) [[unlikely]] {
                            if (IsPreparing()) {
                                LogError("Timed out while waiting for HTTP request");
                            }
                            return RequestStatus::Close;
                        }

                        return RequestStatus::Incomplete;
                    } break;

                    case EPIPE:
                    case ECONNRESET: return RequestStatus::Close;

                    default: {
                        LogError("Read failed: %1 (%2) %3", strerror(errno), socket->sock, socket);
                        return RequestStatus::Close;
                    } break;
                }
            } else if (!read) {
                if (incoming.buf.len) {
                    LogError("Client closed connection with unfinished request");
                }
                return RequestStatus::Close;
            }
        }

        RG_ASSERT(complete);
    }

    if (!ParseRequest(incoming.intro))
        return RequestStatus::Close;

    ready = true;
    return RequestStatus::Ready;
}

bool http_IO::WriteDirect(Span<const uint8_t> data)
{
    while (data.len) {
        Size sent = send(socket->sock, data.ptr, (size_t)data.len, MSG_MORE | MSG_NOSIGNAL);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }
            return false;
        }

        data.ptr += sent;
        data.len -= sent;
    }

    return true;
}

bool http_IO::WriteChunked(Span<const uint8_t> data)
{
    while (data.len) {
        LocalArray<uint8_t, 16384> buf;

        Size copy_len = std::min(RG_SIZE(buf.data) - 8, data.len);

        buf.len = 8 + copy_len;
        Fmt(buf.As<char>(), "%1\r\n", FmtHex(copy_len).Pad0(-4));
        MemCpy(buf.data + 6, data.ptr, copy_len);
        buf.data[6 + copy_len + 0] = '\r';
        buf.data[6 + copy_len + 1] = '\n';

        Span<const uint8_t> remain = buf;

        do {
            Size sent = send(socket->sock, remain.ptr, (size_t)remain.len, MSG_MORE | MSG_NOSIGNAL);

            if (sent < 0) {
                if (errno == EINTR)
                    continue;

                if (errno != EPIPE && errno != ECONNRESET) {
                    LogError("Failed to send to client: %1", strerror(errno));
                }
                return false;
            }

            remain.ptr += sent;
            remain.len -= sent;
         } while (remain.len);

        data.ptr += copy_len;
        data.len -= copy_len;
    }

    return true;
}

}

#endif
