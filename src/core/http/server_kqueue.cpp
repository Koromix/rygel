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

#if !defined(__linux__) && !defined(_WIN32)

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
#include <sys/event.h>
#include <sys/uio.h>

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

    int listener;

    int kqueue_fd = -1;
    int pair_fd[2] = { -1, -1 };

#if defined(__APPLE__)
    std::atomic_bool run { true };
#endif

    HeapArray<http_Socket *> sockets;
    LocalArray<http_Socket *, 256> free_sockets;
    HashSet<void *> busy_sockets;

    HeapArray<struct kevent> next_changes;

public:
    http_Dispatcher(http_Daemon *daemon, http_Dispatcher *next, int listener)
        : daemon(daemon), next(next), listener(listener) {}

    bool Run();
    void Wake(http_Socket *socket);

#if defined(__APPLE__)
    void Stop();
#endif

private:
    http_Socket *InitSocket(int sock, int64_t start, struct sockaddr *sa);
    void ParkSocket(http_Socket *socket);
    void IgnoreSocket(http_Socket *socket);

    void AddEventChange(short filter, int fd, uint16_t flags, void *ptr);

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
    setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &flag, sizeof(flag));

#if defined(__APPLE__)
    if (push) {
        send(sock, nullptr, 0, MSG_NOSIGNAL);
    }
#endif
}

static int CreateListenSocket(const http_Config &config)
{
    int sock = CreateSocket(config.sock_type, SOCK_STREAM);
    if (sock < 0)
        return -1;
    RG_DEFER_N(err_guard) { close(sock); };

#if defined(SO_REUSEPORT_LB)
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT_LB, &reuse, sizeof(reuse));
#else
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
 #endif

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

    SetSocketNonBlock(sock, true);

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

bool http_Daemon::Start(std::function<void(const http_RequestInfo &request, http_IO *io)> func)
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
        shutdown(listener, SHUT_RD);
    }

#if defined(__APPLE__)
    // On macOS, the shutdown() does not wake up poll()
    for (http_Dispatcher *it = dispatcher; it; it = it->next) {
        it->Stop();
    }
#endif

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

bool http_Dispatcher::Run()
{
    RG_ASSERT(kqueue_fd < 0);

    Async async(1 + WorkersPerDispatcher);

#if defined(__FreeBSD__)
    kqueue_fd = kqueuex(KQUEUE_CLOEXEC);
#elif defined(__OpenBSD__)
    kqueue_fd = kqueue1(O_CLOEXEC);
#else
    kqueue_fd = kqueue();

    if (kqueue_fd >= 0) {
        fcntl(kqueue_fd, F_SETFD, FD_CLOEXEC);
    }
#endif
    if (kqueue_fd < 0) {
        LogError("Failed to initialize kqueue: %1", strerror(errno));
        return false;
    }
    RG_DEFER {
        CloseDescriptor(kqueue_fd);
        kqueue_fd = -1;
    };

    if (!CreatePipe(pair_fd))
        return false;
    RG_DEFER {
        CloseDescriptor(pair_fd[0]);
        CloseDescriptor(pair_fd[1]);

        pair_fd[0] = -1;
        pair_fd[1] = -1;
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

        next_changes.Clear();
    };

    AddEventChange(EVFILT_READ, listener, EV_ADD, nullptr);
    AddEventChange(EVFILT_READ, pair_fd[0], EV_ADD, nullptr);

    HeapArray<struct kevent> changes;
    HeapArray<struct kevent> events;
    int next_worker = 0;

    for (;;) {
        int64_t now = GetMonotonicTime();

        for (const struct kevent &ev: events) {
            if (ev.ident == (uintptr_t)listener) {
                if (ev.flags & EV_EOF) [[unlikely]]
                    return true;

                for (int i = 0; i < 8; i++) {
                    sockaddr_storage ss;
                    socklen_t ss_len = RG_SIZE(ss);

#if defined(SOCK_CLOEXEC)
                    int sock = accept4(listener, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);

#if defined(TCP_NOPUSH)
                    if (sock >= 0) {
                        // Disable Nagle algorithm on platforms with better options
                        int flag = 1;
                        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                    }
#endif
#else
                    int sock = accept(daemon->listener, (sockaddr *)&ss, &ss_len);

                    if (sock >= 0) {
                        fcntl(sock, F_SETFD, FD_CLOEXEC);
                        SetSocketNonBlock(sock, true);
                    }
#endif

                    if (sock < 0) {
                        if (errno == EINVAL)
                            return true;
                        if (errno == EAGAIN)
                            break;

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
            } else if (ev.ident == (uintptr_t)pair_fd[0]) {
                for (;;) {
                    uintptr_t addr = 0;
                    Size ret = RG_RESTART_EINTR(read(pair_fd[0], &addr, RG_SIZE(addr)), < 0);

                    if (ret < 0)
                        break;

                    RG_ASSERT(ret == RG_SIZE(void *));
                    http_Socket *socket = (http_Socket *)addr;

                    if (socket) [[likely]] {
                        void **ptr = busy_sockets.Find(socket);

                        if (ptr) {
                            sockets.Append(socket);
                            busy_sockets.Remove(socket);
                        }

                        AddEventChange(EVFILT_READ, socket->sock, EV_ENABLE | EV_CLEAR, socket);
                    }
                }
            } else {
                http_Socket *socket = (http_Socket *)ev.udata;
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

            http_IO::PrepareStatus status = socket->process ? client->ProcessIncoming(now)
                                                            : http_IO::PrepareStatus::Incomplete;
            socket->process = false;

            switch (status) {
                case http_IO::PrepareStatus::Incomplete: {
                    int64_t delay = std::max((int64_t)0, client->GetTimeout(now));
                    timeout = std::min(timeout, (unsigned int)delay);
                } break;

                case http_IO::PrepareStatus::Ready: {
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

                            Wake(socket);

                            return true;
                        });
                    } else {
                        async.Run(worker_idx, [=, this] {
                            daemon->RunHandler(client);
                            client->Rearm(-1);

                            shutdown(socket->sock, SHUT_RD);
                            Wake(socket);

                            return true;
                        });
                    }
                } break;

                case http_IO::PrepareStatus::Close: { disconnect(); } break;
            }
        }
        sockets.len = keep;

        events.RemoveFrom(0);
        events.AppendDefault(2 + sockets.len);

        // We need to be able to add events while kqueue is running, hence the dance
        changes.RemoveFrom(0);
        std::swap(next_changes, changes);

        struct timespec ts = { timeout / 1000, (timeout % 1000) * 1000000 };
        int ready = kevent(kqueue_fd, changes.ptr, (int)changes.len, events.ptr, (int)events.len, &ts);

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

        events.len = ready;
    }

    RG_UNREACHABLE();
}

void http_Dispatcher::Wake(http_Socket *socket)
{
    uintptr_t addr = (uintptr_t)socket;
    Size ret = RG_RESTART_EINTR(write(pair_fd[1], &addr, RG_SIZE(addr)), < 0);
    (void)ret;
}

#if defined(__APPLE__)

void http_Dispatcher::Stop()
{
    run = false;
    Wake(nullptr);
}

#endif

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
    AddEventChange(EVFILT_READ, sock, EV_ADD | EV_CLEAR, socket);

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

void http_Dispatcher::IgnoreSocket(http_Socket *socket)
{
    AddEventChange(EVFILT_READ, socket->sock, EV_DISABLE, socket);
    busy_sockets.Set(socket);
}

void http_Dispatcher::AddEventChange(short filter, int fd, uint16_t flags, void *ptr)
{
    struct kevent ev;
    EV_SET(&ev, fd, filter, flags, 0, 0, ptr);

    next_changes.Append(ev);
}

void http_IO::Send(int status, CompressionType encoding, int64_t len, FunctionRef<bool(int, StreamWriter *)> func)
{
    RG_ASSERT(!response.sent);

    if (request.headers_only) {
        func = [](int, StreamWriter *) { return true; };
    }

#if !defined(MSG_DONTWAIT)
    SetSocketNonBlock(socket->sock, false);
    RG_DEFER { SetSocketNonBlock(socket->sock, true); };
#endif
    SetSocketPush(socket->sock, false);

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

#if defined(__FreeBSD__) || defined(__APPLE__)
#if !defined(MSG_DONTWAIT)
    SetSocketNonBlock(socket->sock, false);
    RG_DEFER { SetSocketNonBlock(socket->sock, true); };
#endif

    SetSocketPush(socket->sock, false);

    RG_DEFER {
        response.sent = true;
        SetSocketPush(socket->sock, true);
    };

    Span<const char> intro = PrepareResponse(status, CompressionType::None, len);

    struct iovec header = {};
    struct sf_hdtr hdtr = { &header, 1, nullptr, 0 };

    off_t offset = 0;
    int64_t remain = intro.len + len;

    // Send intro and file in one go
    while (remain) {
        if (offset < intro.len) {
            header.iov_base = (void *)(intro.ptr + offset);
            header.iov_len = (size_t)(intro.len - offset);
        } else {
            hdtr.headers = nullptr;
            hdtr.hdr_cnt = 0;
        }

        Size send = (Size)std::min(remain, (int64_t)RG_SIZE_MAX);

#if defined(__FreeBSD__)
        off_t sent = 0;
        int ret = sendfile(fd, socket->sock, offset, (size_t)send, &hdtr, &sent, 0);
#else
        off_t sent = (off_t)send;
        int ret = sendfile(fd, socket->sock, offset, &sent, &hdtr, 0);
#endif

        if (ret < 0 && errno != EINTR) {
            if (errno != EPIPE) {
                LogError("Failed to send file: %1", strerror(errno));

            }
            return;
        }

        if (!ret && !send) [[unlikely]] {
            LogError("Truncated file sent");
            return;
        }

        offset += sent;
        remain -= (int64_t)sent;
    }
#else
    Send(status, len, [&](int, StreamWriter *writer) {
        StreamReader reader(fd, filename);
        return SpliceStream(&reader, -1, writer);
    });
#endif
}

http_IO::PrepareStatus http_IO::ProcessIncoming(int64_t now)
{
    RG_ASSERT(!ready);

    // Gather request line and headers
    {
        bool complete = false;

        for (;;) {
            incoming.buf.Grow(Mebibytes(1));

            Size available = incoming.buf.Available() - 1;
#if defined(MSG_DONTWAIT)
            Size read = recv(socket->sock, incoming.buf.end(), available, MSG_DONTWAIT);
#else
            Size read = recv(socket->sock, incoming.buf.end(), available, 0);
#endif

            incoming.buf.len += std::max(read, (Size)0);
            incoming.buf.ptr[incoming.buf.len] = 0;

            while (incoming.buf.len - incoming.pos >= 4) {
                uint8_t *next = (uint8_t *)memchr(incoming.buf.ptr + incoming.pos, '\r', incoming.buf.len - incoming.pos);
                incoming.pos = next ? next - incoming.buf.ptr : incoming.buf.len;

                if (incoming.pos >= daemon->max_request_size) [[unlikely]] {
                    LogError("Excessive request size");
                    SendError(413);
                    return PrepareStatus::Close;
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
                            return PrepareStatus::Close;
                        }

                        return PrepareStatus::Incomplete;
                    } break;

                    case EPIPE:
                    case ECONNRESET: return PrepareStatus::Close;

                    default: {
                        LogError("Read failed: %1 (%2) %3", strerror(errno), socket->sock, socket);
                        return PrepareStatus::Close;
                    } break;
                }
            } else if (!read) {
                if (incoming.buf.len) {
                    LogError("Client closed connection with unfinished request");
                }
                return PrepareStatus::Close;
            }
        }

        RG_ASSERT(complete);
    }

    if (!ParseRequest(incoming.intro))
        return PrepareStatus::Close;

    ready = true;
    return PrepareStatus::Ready;
}

bool http_IO::WriteDirect(Span<const uint8_t> data)
{
    while (data.len) {
        Size sent = send(socket->sock, data.ptr, (size_t)data.len, MSG_NOSIGNAL);

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
            Size sent = send(socket->sock, remain.ptr, (size_t)remain.len, MSG_NOSIGNAL);

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
