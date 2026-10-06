// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)

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
#include <sys/event.h>
#include <sys/uio.h>

#if defined(__APPLE__)
    // The flag may exist, but it does not seem to work with recv!
    #undef MSG_DONTWAIT
#endif

namespace K {

struct http_Socket {
    int sock = -1;

    bool pollable = false;
    bool process = false;
    Size linger = 0;

    http_IO client;

    http_Socket(http_Daemon *daemon) : client(daemon) {}
    ~http_Socket() { CloseDescriptor(sock); }
};

static const Size MaxSend = Mebibytes(2);

class http_Dispatcher {
    http_Daemon *daemon;
    int listener;

    int kqueue_fd = -1;
    int pair_fd[2] = { -1, -1 };

    HeapArray<http_Socket *> sockets;
    LocalArray<http_Socket *, 64> free_sockets;

    HeapArray<struct kevent> next_changes;

public:
    http_Dispatcher(http_Daemon *daemon, int listener)
        : daemon(daemon), listener(listener) {}
    ~http_Dispatcher();

    bool Init();
    void Run();

    void Wake(http_Socket *socket);

private:
    http_Socket *InitSocket(int sock, int64_t start, struct sockaddr *sa);
    void ParkSocket(http_Socket *socket, bool hard);

    void AddEventChange(short filter, int fd, uint16_t flags, void *ptr);

    void StopWS();

    friend class http_Daemon;
};

http_Dispatcher *http_Daemon::CreateDispatcher(int listener)
{
    http_Dispatcher *dispatcher = new http_Dispatcher(this, listener);

    if (!dispatcher->Init()) {
        delete dispatcher;
        return nullptr;
    }

    return dispatcher;
}

void http_Daemon::RunDispatcher(http_Dispatcher *dispatcher)
{
    dispatcher->Run();
}

void http_Daemon::StopDispatcher(http_Dispatcher *dispatcher)
{
    // On macOS (and maybe others), the shutdown() does not wake up poll() so use the
    // pipe to wake it up and signal the ongoing shutdown.

    dispatcher->Wake(nullptr);
}

void http_Daemon::DestroyDispatcher(http_Dispatcher *dispatcher)
{
    delete dispatcher;
}

void http_Daemon::StartRead(http_Socket *socket)
{
    (void)socket;

#if !defined(MSG_DONTWAIT)
    SetSocketNonBlock(socket->sock, false);
#endif
}

void http_Daemon::StartWrite(http_Socket *socket, bool blocking)
{
#if !defined(MSG_DONTWAIT)
    SetSocketNonBlock(socket->sock, !blocking);
#else
    (void)blocking;
#endif

#if !defined(MSG_MORE)
    SetSocketRetain(socket->sock, true);
#endif
}

void http_Daemon::EndWrite(http_Socket *socket)
{
    SetSocketRetain(socket->sock, false);
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

    socket->client.ExtendTimeout(bytes / 4, idle_timeout);

    return bytes;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<const uint8_t> buf, bool nowait)
{
    int flags = MSG_NOSIGNAL;

#if defined(MSG_MORE)
    flags |= MSG_MORE;
#endif
#if defined(MSG_DONTWAIT)
    flags |= nowait ? MSG_DONTWAIT : 0;
#else
    (void)nowait;
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

        socket->client.ExtendTimeout(bytes / 4, send_timeout);

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

        socket->client.ExtendTimeout(sent / 4, send_timeout);

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
    if (len < 0) {
        struct stat sb;
        if (fstat(fd, &sb) < 0) {
            LogError("Cannot get file size: %1", strerror(errno));

            request.keepalive = false;
            return;
        }
        if (!S_ISREG(sb.st_mode)) {
            LogError("Cannot send non-regular file");

            request.keepalive = false;
            return;
        }

        len = (int64_t)sb.st_size;
    }

    if (!StartResponse()) [[unlikely]]
        return;

#if !defined(MSG_MORE)
    K_DEFER { daemon->EndWrite(socket); };
#endif

    // In theory we can use the hdtr argument of sendfile, but the documentation is confusing.
    // Among others, it's not clear what the "sent" value means regarding the headers. Let's not risk it.
    {
        Span<const char> intro = PrepareResponse(status, CompressionType::None, len, false);

        if (!daemon->WriteSocket(socket, intro.As<uint8_t>())) {
            request.keepalive = false;
            return;
        }
    }

    if (request.head || !len)
        return;

#if defined(__FreeBSD__) || defined(__APPLE__)
    off_t offset = 0;
    int64_t remain = len;

    // Send intro and file in one go
    do {
        Size send = (Size)std::min(remain, (int64_t)MaxSend);

#if defined(__FreeBSD__)
        off_t sent = 0;
        int ret = sendfile(fd, socket->sock, offset, (size_t)send, nullptr, &sent, 0);
#else
        off_t sent = (off_t)send;
        int ret = sendfile(fd, socket->sock, offset, &sent, nullptr, 0);
#endif

        if (ret < 0 && errno != EINTR) {
            if (errno != EPIPE && errno != ECONNRESET) {
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

        ExtendTimeout(sent / 4, daemon->send_timeout);

        offset += sent;
        remain -= sent;
    } while (remain);
#else
    StreamReader reader(fd, "<file>");
    StreamWriter writer([&](Span<const uint8_t> buf) { return WriteDirect(buf); }, "<http>");

    if (!SpliceStream(&reader, len, &writer)) {
        request.keepalive = false;
        return;
    }
    if (writer.IsValid() && writer.GetRawWritten() < len) {
        LogError("File was truncated while sending");

        request.keepalive = false;
        return;
    }
#endif
}

http_Dispatcher::~http_Dispatcher()
{
    CloseDescriptor(kqueue_fd);
    CloseDescriptor(pair_fd[0]);
    CloseDescriptor(pair_fd[1]);
}

bool http_Dispatcher::Init()
{
    K_ASSERT(kqueue_fd < 0);

#if defined(__FreeBSD__)
    kqueue_fd = kqueue1(O_CLOEXEC);
#else
    kqueue_fd = kqueue();
    fcntl(kqueue_fd, F_SETFD, FD_CLOEXEC);
#endif
    if (kqueue_fd < 0) {
        LogError("Failed to initialize kqueue: %1", strerror(errno));
        return false;
    }

    if (!CreatePipe(false, pair_fd))
        return false;

    return true;
}

void http_Dispatcher::Run()
{
    K_ASSERT(kqueue_fd >= 0);

    SetThreadName("HTTP dispatcher");

    Async async(daemon->async);

    // Delete remaining clients when function exits
    K_DEFER {
        StopWS();

        if (!async.Wait(100)) {
            LogInfo("Waiting up to %1 sec before shutting down clients...", (double)daemon->stop_timeout / 1000);

            int64_t start = GetMonotonicClock();

            do {
                StopWS();

                if (async.Wait(100))
                    break;
            } while (GetMonotonicClock() - start < daemon->stop_timeout);

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

        next_changes.Clear();
    };

    AddEventChange(EVFILT_READ, listener, EV_ADD | EV_CLEAR, nullptr);
    AddEventChange(EVFILT_READ, pair_fd[0], EV_ADD, nullptr);

    HeapArray<struct kevent> changes;
    HeapArray<struct kevent> events;
    bool accepts = false;

    for (;;) {
        int64_t clock = GetMonotonicClock();

        for (const struct kevent &ev: events) {
            if (ev.ident == (uintptr_t)listener) {
                if (ev.flags & EV_EOF) [[unlikely]]
                    return;

                accepts = true;
            } else if (ev.ident == (uintptr_t)pair_fd[0]) {
                uintptr_t addr = 0;
                Size ret = read(pair_fd[0], &addr, K_SIZE(addr));

                if (ret < 0) [[unlikely]] {
                    if (errno == EINTR)
                        continue;

                    LogError("Unexpected error during pipe read: %1", strerror(errno));
                    return;
                } else if (ret != K_SIZE(addr)) [[unlikely]] {
                    LogError("Unexpected empty or partial read during pipe read");
                    return;
                }

                http_Socket *socket = (http_Socket *)addr;

                if (!socket) [[unlikely]]
                    return;

#if !defined(MSG_DONTWAIT)
                SetSocketNonBlock(socket->sock, true);
#endif
                AddEventChange(EVFILT_READ, socket->sock, EV_ENABLE, socket);
            } else {
                http_Socket *socket = (http_Socket *)ev.udata;
                socket->process = true;
            }
        }

        // Process new connections
        if (accepts) {
            for (int i = 0; i < 32; i++) {
                sockaddr_storage ss;
                socklen_t ss_len = K_SIZE(ss);

#if defined(SOCK_CLOEXEC)
                int sock = accept4(listener, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);
#else
                int sock = accept(listener, (sockaddr *)&ss, &ss_len);
#endif

                if (sock < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        accepts = false;
                        break;
                    }
                    if (errno == EINVAL)
                        return;

                    // Assume transient error (such as too many open files)
                    LogError("Failed to accept client: %1", strerror(errno));
                    WaitDelay(20);

                    break;
                }

#if !defined(SOCK_CLOEXEC)
                fcntl(sock, F_SETFD, FD_CLOEXEC);
#endif
#if !defined(MSG_DONTWAIT)
                SetSocketNonBlock(sock, true);
#endif

                http_Socket *socket = InitSocket(sock, clock, (sockaddr *)&ss);

                if (!socket) [[unlikely]] {
                    close(sock);
                    continue;
                }

                sockets.Append(socket);
            }
        }

        Size keep = 0;
        unsigned int timeout = accepts ? 0 : UINT_MAX;

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
                Size bytes = recv(socket->sock, client->incoming.buf.end(), (size_t)available, MSG_DONTWAIT);
#else
                Size available = client->incoming.buf.Available() - 1;
                Size bytes = recv(socket->sock, client->incoming.buf.end(), (size_t)available, 0);
#endif

                if (bytes > 0) {
                    client->incoming.buf.len += bytes;
                    client->incoming.buf.ptr[client->incoming.buf.len] = 0;

                    if (socket->linger) {
                        if (client->incoming.buf.len > socket->linger) {
                            ParkSocket(socket, true);
                            keep--;

                            continue;
                        }
                    } else {
                        status = client->ParseRequest();
                    }
                } else if (!bytes || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                    if (!socket->linger && client->IsBusy()) {
                        if (bytes) {
                            LogError("Connection failed: %1", strerror(errno));
                        } else {
                            LogError("Connection closed unexpectedly");
                        }
                    }

                    ParkSocket(socket, bytes < 0);
                    keep--;

                    continue;
                }

                if (!socket->pollable) {
                    // We can't do this in InitSocket because the socket might get closed shortly after,
                    // after the first recv() call. This would either trigger EV_ERROR or watch an
                    // unrelated socket descriptor, and also read from a freed or reused socket object.
                    AddEventChange(EVFILT_READ, socket->sock, EV_ADD, socket);
                    socket->pollable = true;
                }
            }

            switch (status) {
                case http_RequestStatus::Busy: { /* Do nothing */ } break;

                case http_RequestStatus::Ready: {
                    AddEventChange(EVFILT_READ, socket->sock, EV_DISABLE, socket);

                    async.Run([socket, this] {
                        http_IO *client = &socket->client;
                        http_RequestStatus status;

                        do {
                            daemon->RunHandler(client);

                            if (!client->Rearm(GetMonotonicClock())) {
                                status = http_RequestStatus::Shutdown;
                                break;
                            }

                            status = client->ParseRequest();
                        } while (status == http_RequestStatus::Ready);

                        if (status == http_RequestStatus::Shutdown) {
                            client->SetTimeout(GetMonotonicClock() + daemon->linger_timeout);
                            socket->linger = Mebibytes(1);
                            shutdown(socket->sock, SHUT_WR);
                        }
                        Wake(socket);

                        return true;
                    });
                } break;

                case http_RequestStatus::Shutdown: {
                    client->SetTimeout(GetMonotonicClock() + daemon->linger_timeout);
                    socket->linger = Mebibytes(1);
                    shutdown(socket->sock, SHUT_WR);
                } break;
            }

            int delay = (int)(client->timeout_at.load(std::memory_order_relaxed) - clock);

            if (delay <= 0) {
                shutdown(socket->sock, SHUT_RDWR);
                socket->linger = Mebibytes(1); // Possible race with worker (who also sets true), harmless

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

        // The timeout is unsigned to make it easier to use with std::min() without dealing
        // with the default value -1. If it stays at UINT_MAX, the (int) cast results in -1.
        struct timespec ts = { timeout / 1000, (timeout % 1000) * 1000000 };
        struct timespec *t = ((int)timeout >= 0) ? &ts : nullptr;
        int ready = kevent(kqueue_fd, changes.ptr, (int)changes.len, events.ptr, (int)events.len, t);

        if (ready < 0) {
            if (errno != EINTR) {
                LogError("Failed to poll descriptors: %1", strerror(errno));
                return;
            }

            ready = 0;
        }

        events.len = ready;
    }

    K_UNREACHABLE();
}

void http_Dispatcher::Wake(http_Socket *socket)
{
    uintptr_t addr = (uintptr_t)socket;
    Size ret = K_RESTART_EINTR(write(pair_fd[1], &addr, K_SIZE(addr)), < 0);
    (void)ret;
}

http_Socket *http_Dispatcher::InitSocket(int sock, int64_t start, struct sockaddr *sa)
{
    http_Socket *socket = free_sockets.len ? free_sockets.data[--free_sockets.len] : new http_Socket(daemon);
    K_DEFER_N(err_guard) { delete socket; };

    if (!socket->client.Init(socket, start, sa)) [[unlikely]]
        return nullptr;

    socket->sock = sock;
    socket->pollable = false;
    socket->process = true;
    socket->linger = 0;

    err_guard.Disable();
    return socket;
}

void http_Dispatcher::ParkSocket(http_Socket *socket, bool hard)
{
    if (hard) {
        struct linger sl = { 1, 0 };
        setsockopt(socket->sock, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl));
    }

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
