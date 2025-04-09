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

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)

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
#include <sys/event.h>
#include <sys/uio.h>

namespace RG {

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

    int kqueue_fd = -1;
    int pair_fd[2] = { -1, -1 };

    HeapArray<http_Socket *> sockets;
    LocalArray<http_Socket *, 64> free_sockets;

    HeapArray<struct kevent> next_changes;

public:
    http_Dispatcher(http_Daemon *daemon, http_Dispatcher *next, int listener)
        : daemon(daemon), next(next), listener(listener) {}

    bool Run();
    void Wake(http_Socket *socket);

private:
    http_Socket *InitSocket(int sock, int64_t start, struct sockaddr *sa);
    void ParkSocket(http_Socket *socket);

    void AddEventChange(short filter, int fd, uint16_t flags, void *ptr);

    friend class http_Daemon;
};

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

#if defined(__APPLE__)
    // On macOS, the shutdown() does not wake up poll() so use the pipe to wake it up
    // and signal the ongoing shutdown.
    for (http_Dispatcher *it = dispatcher; it; it = it->next) {
        it->Wake(nullptr);
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
void http_Daemon::StartRead(http_Socket *socket)
{
    (void)socket;

#if !defined(MSG_DONTWAIT)
    SetDescriptorNonBlock(socket->sock, false);
#endif
}

void http_Daemon::StartWrite(http_Socket *socket)
{
    (void)socket;

#if !defined(MSG_DONTWAIT)
    SetDescriptorNonBlock(socket->sock, false);
#endif
#if !defined(MSG_MORE)
    SetDescriptorRetain(socket->sock, true);
#endif
}

void http_Daemon::EndWrite(http_Socket *socket)
{
    (void)socket;

#if !defined(MSG_MORE)
    SetDescriptorRetain(socket->sock, false);
#endif
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
    int flags = MSG_NOSIGNAL;

#if defined(MSG_MORE)
    flags |= MSG_MORE;
#endif

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
    int flags = MSG_NOSIGNAL;

#if defined(MSG_MORE)
    flags |= MSG_MORE;
#endif

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

#if !defined(MSG_DONTWAIT)
    SetDescriptorNonBlock(socket->sock, false);
#endif

    if (len < 0) {
        struct stat sb;
        if (fstat(fd, &sb) < 0) {
            LogError("Cannot get file size: %1", strerror(errno));

            request.keepalive = false;
            return;
        }

        len = (int64_t)sb.st_size;
    }

#if defined(__FreeBSD__) || defined(__APPLE__)
    Span<const char> intro = PrepareResponse(status, CompressionType::None, len);
    bool cork = (len >= MaxSend);

    if (cork) {
        SetDescriptorRetain(socket->sock, true);
    }
    RG_DEFER {
        if (cork) {
            SetDescriptorRetain(socket->sock, false);
        }
    };

    struct iovec header = { (void *)intro.ptr, (size_t)intro.len };
    struct sf_hdtr hdtr = { &header, 1, nullptr, 0 };

    off_t offset = 0;
    int64_t remain = len;

    // Send intro and file in one go
    do {
        Size send = (Size)std::min(remain, (int64_t)MaxSend);

#if defined(__FreeBSD__)
        off_t sent = 0;
        int ret = sendfile(fd, socket->sock, offset, (size_t)send, &hdtr, &sent, 0);
#else
        off_t sent = (off_t)send;
        int ret = sendfile(fd, socket->sock, offset, &sent, &hdtr, 0);
#endif

        if (ret < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send file: %1", strerror(errno));
            }

            request.keepalive = false;
            return;
        }

        if (!ret && !sent) [[unlikely]] {
            LogError("Truncated file sent");

            request.keepalive = false;
            return;
        }

        socket->client.timeout_at = GetMonotonicTime() + daemon->send_timeout;

        if (sent < (Size)header.iov_len) [[unlikely]] {
            header.iov_base = (uint8_t *)header.iov_base + sent;
            header.iov_len -= sent;

            continue;
        }
        sent -= (Size)header.iov_len;

        offset += sent;
        remain -= sent;

        hdtr.hdr_cnt = 0;
        header.iov_len = 0;
    } while (remain);
#else
    Send(status, len, [&](StreamWriter *writer) {
        StreamReader reader(fd, "<file>");

        if (!SpliceStream(&reader, len, writer)) {
            request.keepalive = false;
            return false;
        }
        if (writer->IsValid() && writer->GetRawWritten() < len) {
            LogError("File was truncated while sending");

            request.keepalive = false;
            return false;
        }

        return true;
    });
#endif
}

bool http_Dispatcher::Run()
{
    RG_ASSERT(kqueue_fd < 0);

    Async async(1 + WorkersPerDispatcher);

#if defined(__FreeBSD__)
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
        if (!async.Wait(100)) {
            LogInfo("Waiting up to %1 sec before shutting down clients", (double)daemon->stop_timeout / 1000);

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

        next_changes.Clear();
    };

    AddEventChange(EVFILT_READ, listener, EV_ADD, nullptr);
    AddEventChange(EVFILT_READ, pair_fd[0], EV_ADD, nullptr);

    HeapArray<struct kevent> changes;
    HeapArray<struct kevent> events;
    int next_worker = 0;

    for (;;) {
        int64_t now = GetMonotonicTime();
        bool accepts = false;

        for (const struct kevent &ev: events) {
            if (ev.ident == (uintptr_t)listener) {
                if (ev.flags & EV_EOF) [[unlikely]]
                    return true;

                accepts = true;
            } else if (ev.ident == (uintptr_t)pair_fd[0]) {
                uintptr_t addr = 0;
                Size ret = RG_RESTART_EINTR(read(pair_fd[0], &addr, RG_SIZE(addr)), < 0);

                if (ret <= 0)
                    break;
                RG_ASSERT(ret == RG_SIZE(void *));

                http_Socket *socket = (http_Socket *)addr;

#if defined(__APPLE__)
                if (!socket) [[unlikely]]
                    return true;
#endif

#if !defined(MSG_DONTWAIT)
                SetDescriptorNonBlock(socket->sock, true);
#endif
                AddEventChange(EVFILT_READ, socket->sock, EV_ENABLE | EV_CLEAR, socket);
            } else {
                http_Socket *socket = (http_Socket *)ev.udata;
                socket->process = true;
            }
        }

        // Process new connections
        if (accepts) {
            for (int i = 0; i < 8; i++) {
                sockaddr_storage ss;
                socklen_t ss_len = RG_SIZE(ss);

#if defined(SOCK_CLOEXEC)
                int sock = accept4(listener, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);
#else
                int sock = accept(listener, (sockaddr *)&ss, &ss_len);
#endif

                if (sock < 0) {
                    static_assert(EAGAIN == EWOULDBLOCK);

                    if (errno == EAGAIN)
                        break;
                    if (errno == EINVAL)
                        return true;

                    LogError("Failed to accept client: %1", strerror(errno));
                    return false;
                }

#if !defined(SOCK_CLOEXEC)
                fcntl(sock, F_SETFD, FD_CLOEXEC);
#endif
#if !defined(MSG_DONTWAIT)
                SetDescriptorNonBlock(sock, true);
#endif

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

#if defined(MSG_DONTWAIT)
                Size available = client->incoming.buf.Available() - 1;
                Size bytes = recv(socket->sock, client->incoming.buf.ptr, (size_t)available, MSG_DONTWAIT);
#else
                Size available = client->incoming.buf.Available() - 1;
                Size bytes = recv(socket->sock, client->incoming.buf.ptr, (size_t)available, 0);
#endif

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

                    AddEventChange(EVFILT_READ, socket->sock, EV_DISABLE, socket);

                    async.Run(worker_idx, [=, this] {
                        do {
                            daemon->RunHandler(client, now);

                            if (!client->Rearm(GetMonotonicTime())) {
                                shutdown(socket->sock, SHUT_RD);
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
                shutdown(socket->sock, SHUT_RDWR);
                continue;
            }

            timeout = std::min(timeout, (unsigned int)delay);
        }
        sockets.len = keep;

        events.RemoveFrom(0);
        events.AppendDefault(2 + sockets.len);

        // We need to be able to add events while kqueue is running, hence the dance
        changes.RemoveFrom(0);
        std::swap(next_changes, changes);

        struct timespec ts = { timeout / 1000, (timeout % 1000) * 1000000 };
        int ready = kevent(kqueue_fd, changes.ptr, (int)changes.len, events.ptr, (int)events.len, &ts);

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

void http_Dispatcher::Wake(http_Socket *socket)
{
    uintptr_t addr = (uintptr_t)socket;
    Size ret = RG_RESTART_EINTR(write(pair_fd[1], &addr, RG_SIZE(addr)), < 0);
    (void)ret;
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

void http_Dispatcher::AddEventChange(short filter, int fd, uint16_t flags, void *ptr)
{
    struct kevent ev;
    EV_SET(&ev, fd, filter, flags, 0, 0, ptr);

    next_changes.Append(ev);
}

}

#endif
