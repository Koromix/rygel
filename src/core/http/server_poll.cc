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

#if !defined(_WIN32)

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
#if defined(__linux__)
    #include <sys/eventfd.h>
    #include <sys/sendfile.h>
#elif defined(__FreeBSD__)
    #include <sys/eventfd.h>
    #include <sys/uio.h>
#endif

namespace RG {

// Make things work on macOS
#if defined(MSG_DONTWAIT) && !defined(SOCK_CLOEXEC)
    #undef MSG_DONTWAIT
#endif

static const int WorkersPerDispatcher = 4;

struct SocketData {
    int sock = -1;
    int pfd_idx = -1;
    http_IO *client = nullptr;
};

class http_Dispatcher {
    http_Daemon *daemon;

#if defined(__linux__) || defined(__FreeBSD__)
    int event_fd = -1;
#else
    int pair_fd[2] = { -1, -1 };
#endif
    std::shared_mutex wake_mutex;
    bool wake_up = false;
    bool wake_interrupt = false;

#if defined(__APPLE__)
    std::atomic_bool run { true };
#endif

    HeapArray<SocketData> sockets;
    LocalArray<http_IO *, 256> free_clients;

public:
    http_Dispatcher(http_Daemon *daemon) : daemon(daemon) {}

    bool Run();
    void Wake();

#if defined(__APPLE__)
    void Stop();
#endif

private:
    http_IO *InitClient(int sock, int64_t start, struct sockaddr *sa);
    void ParkClient(http_IO *client);
};

static void SetSocketNonBlock(int sock, bool enable)
{
    int flags = fcntl(sock, F_GETFL, 0);
    flags = ApplyMask(flags, O_NONBLOCK, enable);
    fcntl(sock, F_SETFL, flags);
}

void SetSocketPush(int sock, bool push)
{
#if defined(TCP_CORK)
    int flag = !push;
    setsockopt(sock, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));
#elif defined(TCP_NOPUSH)
    int flag = !push;
    setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &flag, sizeof(flag));

#if defined(__APPLE__)
    if (push) {
        send(sock, nullptr, 0, 0);
    }
#endif
#else
    int flag = push;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    send(sock, nullptr, 0, 0);
#endif
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
        http_Dispatcher *dispatcher = new http_Dispatcher(this);
        dispatchers.Append(dispatcher);

        async->Run([=] { return dispatcher->Run(); });
    }

    return true;
}

void http_Daemon::Stop()
{
    // Shut everything down
    shutdown(listener, SHUT_RD);

#if defined(__APPLE__)
    // On macOS, the shutdown() does not wake up poll()
    for (http_Dispatcher *dispatcher: dispatchers) {
        dispatcher->Stop();
    }
#endif

    if (async) {
        async->Sync();

        delete async;
        async = nullptr;
    }

    for (http_Dispatcher *dispatcher: dispatchers) {
        delete dispatcher;
    }
    dispatchers.Clear();

    CloseSocket(listener);
    listener = -1;

    handle_func = {};
}

bool http_Dispatcher::Run()
{
#if defined(__linux__) || defined(__FreeBSD__)
    RG_ASSERT(event_fd < 0);
#else
    RG_ASSERT(pair_fd[0] < 0);
#endif

    Async async(1 + WorkersPerDispatcher);

#if defined(__linux__) || defined(__FreeBSD__)
    event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (event_fd < 0) {
        LogError("Failed to create eventfd: %1", strerror(errno));
        return false;
    }
    RG_DEFER {
        CloseDescriptor(event_fd);
        event_fd = -1;
    };
#else
    if (!CreatePipe(pair_fd))
        return false;
    RG_DEFER {
        CloseDescriptor(pair_fd[0]);
        CloseDescriptor(pair_fd[1]);

        pair_fd[0] = -1;
        pair_fd[1] = -1;
    };
#endif

    // Delete remaining clients when function exits
    RG_DEFER {
        async.Sync();

        for (SocketData &socket: sockets) {
            close(socket.sock);
            delete socket.client;
        }
        for (http_IO *client: free_clients) {
            delete client;
        }

        sockets.Clear();
        free_clients.Clear();
    };

    HeapArray<struct pollfd> pfds = {
#if defined(__linux__) || defined(__FreeBSD__)
        { daemon->listener, POLLIN, 0 },
        { event_fd, POLLIN, 0 }
#else
        { daemon->listener, POLLIN, 0 },
        { pair_fd[0], POLLIN, 0 }
#endif
    };

    int next_worker = 0;

    for (;;) {
        int64_t now = GetMonotonicTime();

        pfds.len = 2;

        if (pfds[0].revents & POLLHUP)
            return true;
#if defined(__APPLE__)
        if (!run)
            return true;
#endif

        if (pfds[0].revents & POLLIN) {
            sockaddr_storage ss;
            socklen_t ss_len = RG_SIZE(ss);

            // Accept queued clients
            for (Size i = 0; i < 64; i++) {
#if defined(SOCK_CLOEXEC)
                int sock = accept4(daemon->listener, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);

#if defined(TCP_NOPUSH) && !defined(TCP_CORK)
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
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    LogError("Failed to accept client: %1 %2", strerror(errno), errno);
                    return false;
                }

                SocketData *socket = sockets.AppendDefault();

                socket->sock = sock;
                socket->client = InitClient(sock, now, (sockaddr *)&ss);

                if (!socket->client) {
                    sockets.RemoveLast(1);
                    continue;
                }
            }
        }

        // Clear eventfd
        if (pfds[1].revents & POLLIN) {
#if defined(__linux__) || defined(__FreeBSD__)
            uint64_t dummy = 0;
            Size ret = read(event_fd, &dummy, RG_SIZE(dummy));
            (void)ret;
#else
            char buf[4096];
            Size ret = read(pair_fd[0], buf, RG_SIZE(buf));
            (void)ret;
#endif
        }

        Size keep = 0;
        unsigned int timeout = UINT_MAX;

        // Process clients
        for (Size i = 0; i < sockets.len; i++, keep++) {
            sockets[keep] = sockets[i];

            SocketData *socket = &sockets[keep];
            http_IO *client = socket->client;

            const auto disconnect = [&]() {
                close(socket->sock);

                ParkClient(socket->client);
                socket->client = nullptr;

                keep--;
            };

            http_IO::PrepareStatus status = http_IO::PrepareStatus::Incoming;

            if (socket->pfd_idx >= 0) {
                const struct pollfd &pfd = pfds.ptr[socket->pfd_idx];

                if (pfd.revents) {
                    status = client->Prepare(now);
                }

                socket->pfd_idx = -1;
            } else {
                status = client->Prepare(now);
            }

            switch (status) {
                case http_IO::PrepareStatus::Incoming: {
                    socket->pfd_idx = pfds.len;

                    struct pollfd pfd = { socket->sock, POLLIN, 0 };
                    pfds.Append(pfd);

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

                    if (client->request.keepalive) {
                        async.Run(worker_idx, [=, this] {
                            daemon->RunHandler(client);

                            client->Rearm(now);
                            Wake();

                            return true;
                        });
                    } else {
                        async.Run(worker_idx, [=, this] {
                            daemon->RunHandler(client);

                            client->ready = false;
                            shutdown(client->sock, SHUT_RD);

                            return true;
                        });
                    }
                } break;

                case http_IO::PrepareStatus::Busy: {} break;

                case http_IO::PrepareStatus::Close: { disconnect(); } break;
            }
        }
        sockets.len = keep;

        // Wake me up from the kernel if needed
        {
            std::lock_guard<std::shared_mutex> lock_excl(wake_mutex);

            if (wake_up) {
                wake_up = false;
                continue;
            }

            wake_interrupt = true;
        }

        // The timeout is unsigned to make it easier to use with std::min() without dealing
        // with the default value -1. If it stays at UINT_MAX, the (int) cast results in -1.
        int ready = poll(pfds.ptr, pfds.len, (int)timeout);

        if (ready < 0 && errno != EINTR) [[unlikely]] {
            LogError("Failed to poll descriptors: %1", strerror(errno));
            return false;
        }

        if (!ready) {
            // Process everyone after a timeout
            for (SocketData &socket: sockets) {
                socket.pfd_idx = -1;
            }
        }
    }

    RG_UNREACHABLE();
}

void http_Dispatcher::Wake()
{
    std::shared_lock<std::shared_mutex> lock_shr(wake_mutex);

    wake_up = true;

    // Fast path (prevent poll)
    if (!wake_interrupt)
        return;

    lock_shr.unlock();

#if defined(__linux__) || defined(__FreeBSD__)
    uint64_t one = 1;
    Size ret = RG_RESTART_EINTR(write(event_fd, &one, RG_SIZE(one)), < 0);
    (void)ret;
#else
    char x = 'x';
    Size ret = RG_RESTART_EINTR(send(pair_fd[1], &x, RG_SIZE(x), 0), < 0);
    (void)ret;
#endif
}

#if defined(__APPLE__)

void http_Dispatcher::Stop()
{
    run = false;
    Wake();
}

#endif

http_IO *http_Dispatcher::InitClient(int sock, int64_t start, struct sockaddr *sa)
{
    http_IO *client = nullptr;

    if (free_clients.len) {
        int idx = GetRandomInt(0, (int)free_clients.len);

        client = free_clients[idx];

        std::swap(free_clients[idx], free_clients[free_clients.len - 1]);
        free_clients.len--;
    } else {
        client = new http_IO(daemon);
    }

    if (!client->Init(sock, start, sa)) [[unlikely]] {
        delete client;
        return nullptr;
    }

    return client;
}

void http_Dispatcher::ParkClient(http_IO *client)
{
    if (free_clients.Available()) {
        client->Rearm(0);
        free_clients.Append(client);
    } else {
        delete client;
    }
}

void http_IO::Send(int status, CompressionType encoding, int64_t len, FunctionRef<bool(int, StreamWriter *)> func)
{
    RG_ASSERT(!response.sent);

    if (request.headers_only) {
        func = [](int, StreamWriter *) { return true; };
    }

#if !defined(MSG_DONTWAIT)
    SetSocketNonBlock(sock, false);
    RG_DEFER { SetSocketNonBlock(sock, true); };
#endif
#if !defined(MSG_MORE)
    // On Linux, use MSG_MORE in send() calls to set TCP_CORK without additional syscall
    SetSocketPush(sock, false);
#endif

    RG_DEFER { 
        response.sent = true;
        SetSocketPush(sock, true);
    };

    const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };
    StreamWriter writer(write, "<http>");

    LocalArray<char, 32768> intro;

    const char *protocol = (request.version == 11) ? "HTTP/1.1" : "HTTP/1.0";
    const char *details = http_ErrorMessages.FindValue(status, "Unknown");

    if (request.keepalive) {
        intro.len += Fmt(intro.TakeAvailable(), "%1 %2 %3\r\n"
                                                "Connection: keep-alive\r\n",
                         protocol, status, details).len;
    } else {
        intro.len += Fmt(intro.TakeAvailable(), "%1 %2 %3\r\n"
                                                "Connection: close\r\n",
                         protocol, status, details).len;
    }

    switch (encoding) {
        case CompressionType::None: {} break;
        case CompressionType::Zlib: { intro.len += Fmt(intro.TakeAvailable(), "Content-Encoding: deflate\r\n").len; } break;
        case CompressionType::Gzip: { intro.len += Fmt(intro.TakeAvailable(), "Content-Encoding: gzip\r\n").len; } break;
        case CompressionType::Brotli: { intro.len += Fmt(intro.TakeAvailable(), "Content-Encoding: br\r\n").len; } break;
        case CompressionType::LZ4: { RG_UNREACHABLE(); } break;
        case CompressionType::Zstd: { intro.len += Fmt(intro.TakeAvailable(), "Content-Encoding: zstd\r\n").len; } break;
    }

    for (const http_KeyValue &header: response.headers) {
        intro.len += Fmt(intro.TakeAvailable(), "%1: %2\r\n", header.key, header.value).len;
    }

    if (len >= 0) {
        intro.len += Fmt(intro.TakeAvailable(), "Content-Length: %1\r\n\r\n", len).len;

        if (!intro.Available()) [[unlikely]] {
            LogError("Excessive length for response headers");

            request.keepalive = false;
            return;
        }

        writer.Write(intro);

        if (encoding != CompressionType::None) {
            writer.Close();
            writer.Open(write, "<http>", encoding);
        }

        request.keepalive &= func(sock, &writer);
    } else {
        intro.len += Fmt(intro.TakeAvailable(), "Transfer-Encoding: chunked\r\n\r\n").len;

        if (!intro.Available()) [[unlikely]] {
            LogError("Excessive length for response headers");

            request.keepalive = false;
            return;
        }

        writer.Write(intro);

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

bool http_IO::SendFile(int status, const char *filename, const char *mimetype)
{
    int fd = OpenFile(filename, (int)OpenFlag::Read);
    if (fd < 0)
        return false;
    RG_DEFER { CloseDescriptor(fd); };

    FileInfo file_info;
    if (StatFile(fd, filename, &file_info) != StatResult::Success)
        return false;
    if (file_info.type != FileType::File) {
        LogError("Cannot serve non-regular file '%1'", filename);
        return false;
    }
    int64_t len = file_info.size;

    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }

#if defined(__linux__)
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
#elif defined(__FreeBSD__)
    Send(status, len, [&](int sock, StreamWriter *) {
        off_t offset = 0;
        int64_t remain = len;

        while (remain) {
            Size send = (Size)std::min(remain, (int64_t)RG_SIZE_MAX);

            off_t sent = 0;
            int ret = sendfile(fd, sock, offset, (size_t)send, nullptr, &sent, 0);

            if (ret < 0) {
                if (errno == EINTR)
                    continue;

                if (errno != EPIPE) {
                    LogError("Failed to send file: %1", strerror(errno));
                }
                return false;
            }

            offset += sent;
            remain -= (Size)sent;
        }

        return true;
    });
#else
    Send(status, len, [&](int, StreamWriter *writer) {
        StreamReader reader(fd, filename);
        return SpliceStream(&reader, -1, writer);
    });
#endif

    return true;
}

http_IO::PrepareStatus http_IO::Prepare(int64_t now)
{
    if (ready)
        return PrepareStatus::Busy;

    // Gather request line and headers
    {
        bool complete = false;

        for (;;) {
            incoming.buf.Grow(Mebibytes(1));

            Size available = incoming.buf.Available() - 1;
#if defined(MSG_DONTWAIT)
            Size read = recv(sock, incoming.buf.end(), available, MSG_DONTWAIT);
#else
            Size read = recv(sock, incoming.buf.end(), available, 0);
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

#if EAGAIN != EWOULDBLOCK
                    case EAGAIN:
#endif
                    case EWOULDBLOCK: {
                        int64_t timeout = GetTimeout(now);

                        if (timeout < 0) [[unlikely]] {
                            if (IsPreparing()) {
                                LogError("Timed out while waiting for HTTP request");
                            }
                            return PrepareStatus::Close;
                        }

                        return PrepareStatus::Incoming;
                    } break;

                    case ECONNRESET: return PrepareStatus::Close;

                    default: {
                        LogError("Read failed: %1", strerror(errno));
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
#if defined(MSG_MORE)
        Size sent = send(sock, data.ptr, (size_t)data.len, MSG_MORE | MSG_NOSIGNAL);
#else
        Size sent = send(sock, data.ptr, (size_t)data.len, MSG_NOSIGNAL);
#endif

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
#if defined(MSG_MORE)
            Size sent = send(sock, remain.ptr, (size_t)remain.len, MSG_MORE | MSG_NOSIGNAL);
#else
            Size sent = send(sock, remain.ptr, (size_t)remain.len, MSG_NOSIGNAL);
#endif

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
