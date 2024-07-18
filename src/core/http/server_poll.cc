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

#include "src/core/base/base.hh"
#include "server.hh"
#include "misc.hh"

#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <ws2tcpip.h>
    #include <mswsock.h>
    #include <io.h>

    #if !defined(UNIX_PATH_MAX)
        #define UNIX_PATH_MAX 108
    #endif
    typedef struct sockaddr_un {
        ADDRESS_FAMILY sun_family;
        char sun_path[UNIX_PATH_MAX];
    } SOCKADDR_UN, *PSOCKADDR_UN;
#else
    #include <poll.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif
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

class http_Daemon::Dispatcher {
    http_Daemon *daemon;

#if defined(__linux__) || defined(__FreeBSD__)
    int event_fd = -1;
#else
    int pair_fd[2] = { -1, -1 };
#endif
    std::shared_mutex wake_mutex;
    bool wake_needed = false;

#if defined(_WIN32) || defined(__APPLE__)
    std::atomic_bool run { true };
#endif

    HeapArray<http_IO *> clients;
    LocalArray<http_IO *, 256> pool;

public:
    Dispatcher(http_Daemon *daemon) : daemon(daemon) {}

    bool Run();
    void Wake();

#if defined(_WIN32) || defined(__APPLE__)
    void Stop();
#endif

private:
    http_IO *CreateClient(int fd, int64_t start, struct sockaddr *sa);
    void DestroyClient(http_IO *client);
};

static void SetSocketNonBlock(int fd, bool enable)
{
#if defined(_WIN32)
    unsigned long mode = enable;
    ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    flags = ApplyMask(flags, O_NONBLOCK, enable);
    fcntl(fd, F_SETFL, flags);
#endif
}

void SetSocketPush(int fd, bool push)
{
#if defined(TCP_CORK)
    int flag = !push;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));
#elif defined(TCP_NOPUSH)
    int flag = !push;
    setsockopt(fd, IPPROTO_TCP, TCP_NOPUSH, &flag, sizeof(flag));

#if defined(__APPLE__)
    if (push) {
        send(fd, nullptr, 0, 0);
    }
#endif
#elif defined(_WIN32)
    int flag = push;
    setsockopt((SOCKET)fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    send((SOCKET)fd, nullptr, 0, 0);
#else
    int flag = push;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    send(fd, nullptr, 0, 0);
#endif
}

#if defined(_WIN32)

int TranslateWinSockError()
{
    int error = WSAGetLastError();

    switch (error) {
        case WSAEACCES: return EADDRINUSE;
        case WSAEADDRINUSE: return EADDRINUSE;
        case WSAEADDRNOTAVAIL: return EADDRNOTAVAIL;
        case WSAEALREADY: return EALREADY;
        case WSAEBADF: return EBADF;
        case WSAECONNABORTED: return ECONNABORTED;
        case WSAECONNREFUSED: return ECONNREFUSED;
        case WSAECONNRESET: return ECONNRESET;
        case WSAEDESTADDRREQ: return EDESTADDRREQ;
        // case WSAEDQUOT: return EDQUOT;
        case WSAEFAULT: return EFAULT;
        case WSAEHOSTDOWN: return ETIMEDOUT;
        case WSAEHOSTUNREACH: return EHOSTUNREACH;
        case WSAEINPROGRESS: return EINPROGRESS;
        case WSAEINTR: return EINTR;
        case WSAEINVAL: return EINVAL;
        case WSAEISCONN: return EISCONN;
        case WSAELOOP: return ELOOP;
        case WSAEMFILE: return EMFILE;
        case WSAEMSGSIZE: return EMSGSIZE;
        case WSAENAMETOOLONG: return ENAMETOOLONG;
        case WSAENETDOWN: return ENETDOWN;
        case WSAENETRESET: return ENETRESET;
        case WSAENETUNREACH: return ENETUNREACH;
        case WSAENOBUFS: return ENOBUFS;
        case WSAENOPROTOOPT: return ENOPROTOOPT;
        case WSAENOTCONN: return ENOTCONN;
        case WSAENOTEMPTY: return ENOTEMPTY;
        case WSAENOTSOCK: return ENOTSOCK;
        case WSAEOPNOTSUPP: return EOPNOTSUPP;
        // case WSAEPFNOSUPPORT: return EPFNOSUPPORT;
        // case WSAEPROCLIM: return EPROCLIM;
        case WSAEPROTONOSUPPORT: return EPROTONOSUPPORT;
        case WSAEPROTOTYPE: return EPROTOTYPE;
        case WSAEREMOTE: return EINVAL;
        case WSAESHUTDOWN: return EPIPE;
        // case WSAESOCKTNOSUPPORT: return ESOCKTNOSUPPORT;
        case WSAESTALE: return EINVAL;
        case WSAETIMEDOUT: return ETIMEDOUT;
        // case WSAETOOMANYREFS: return ETOOMANYREFS;
        // case WSAEUSERS: return EUSERS;
        case WSAEWOULDBLOCK: return EAGAIN;
    }

    return error;
}

static bool CreateSocketPair(int out_sockets[2])
{
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        LogError("Failed to create TCP socket: %1", strerror(TranslateWinSockError()));
        return false;
    }
    RG_DEFER { closesocket(listen_sock); };

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    // Bind socket to random port
    {
        int reuse = 1;
        socklen_t addr_len = RG_SIZE(addr);

        if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, RG_SIZE(reuse)) == SOCKET_ERROR) {
            LogError("setsockopt() failed: %1", strerror(TranslateWinSockError()));
            return false;
        }
        if (bind(listen_sock, (struct sockaddr *)&addr, addr_len) == SOCKET_ERROR) {
            LogError("Failed to bind TCP socket: %1", strerror(TranslateWinSockError()));
            return false;
        }
        if (getsockname(listen_sock, (struct sockaddr *)&addr, &addr_len) == SOCKET_ERROR) {
            LogError("Failed to get socket name: %1", strerror(TranslateWinSockError()));
            return false;
        }

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    if (listen(listen_sock, 1) == SOCKET_ERROR) {
        LogError("Failed to listen on socket: %1", strerror(TranslateWinSockError()));
        return false;
    }

    SOCKET socks[2] = { INVALID_SOCKET, INVALID_SOCKET };

    RG_DEFER_N(err_guard) {
        closesocket(socks[0]);
        closesocket(socks[1]);
    };

    socks[0] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socks[0] == INVALID_SOCKET) {
        LogError("Failed to create TCP socket: %1", strerror(TranslateWinSockError()));
        return false;
    }
    if (connect(socks[0], (struct sockaddr *)&addr, RG_SIZE(addr)) == SOCKET_ERROR) {
        LogError("Failed to connect TCP socket pair: %1", strerror(TranslateWinSockError()));
        return false;
    }

    socks[1] = accept(listen_sock, nullptr, nullptr);
    if (socks[1] == INVALID_SOCKET) {
        LogError("Failed to accept TCP socket pair: %1", strerror(TranslateWinSockError()));
        return false;
    }

    SetSocketNonBlock(socks[0], true);
    SetSocketNonBlock(socks[1], true);

    err_guard.Disable();
    out_sockets[0] = (int)socks[0];
    out_sockets[1] = (int)socks[1];
    return true;
}

#elif !defined(__linux__) && !defined(__FreeBSD__)

static bool CreateSocketPair(int out_sockets[2])
{
    int socks[2] = { -1, -1 };

    RG_DEFER_N(err_guard) {
        CloseDescriptor(socks[0]);
        CloseDescriptor(socks[1]);
    };

#if defined(SOCK_CLOEXEC)
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, socks) < 0) {
        LogError("Failed to create socket pair: %1", strerror(errno));
        return false;
    }
#else
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0) {
        LogError("Failed to create socket pair: %1", strerror(errno));
        return false;
    }

    fcntl(socks[0], F_SETFD, FD_CLOEXEC);
    fcntl(socks[1], F_SETFD, FD_CLOEXEC);
    SetSocketNonBlock(socks[0], true);
    SetSocketNonBlock(socks[1], true);
#endif

    err_guard.Disable();
    out_sockets[0] = (int)socks[0];
    out_sockets[1] = (int)socks[1];
    return true;
}

#endif

bool http_Daemon::Bind(const http_Config &config)
{
    RG_ASSERT(listen_fd < 0);

    // Validate configuration
    if (!config.Validate())
        return false;

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: { listen_fd = OpenIPSocket(config.sock_type, config.port, SOCK_STREAM); } break;
        case SocketType::Unix: { listen_fd = OpenUnixSocket(config.unix_path, SOCK_STREAM); } break;
    }
    if (listen_fd < 0)
        return false;

    if (listen(listen_fd, 1024) < 0) {
#if defined(_WIN32)
        errno = TranslateWinSockError();
#endif

        LogError("Failed to listen on socket: %1", strerror(errno));
        return false;
    }

    SetSocketNonBlock(listen_fd, true);

    addr_mode = config.addr_mode;

    return true;
}

bool http_Daemon::Start(const http_Config &config,
                        std::function<void(const http_RequestInfo &request, http_IO *io)> func,
                        bool log_socket)
{
    RG_ASSERT(!handle_func);
    RG_ASSERT(func);

    // Validate configuration
    if (!config.Validate())
        return false;

    if (config.addr_mode == http_ClientAddressMode::Socket) {
        LogInfo("You may want to %!.._set HTTP.ClientAddress%!0 to X-Forwarded-For or X-Real-IP "
                "if you run this behind a reverse proxy that sets one of these headers.");
    }

    // Prepare socket (if not done yet)
    if (listen_fd < 0 && !Bind(config))
        return false;

    handle_func = func;

    // Run request dispatchers
    for (Size i = 0; i < async.GetWorkerCount(); i++) {
        Dispatcher *dispatcher = new Dispatcher(this);
        dispatchers.Append(dispatcher);

        async.Run([=] { return dispatcher->Run(); });
    }

    if (log_socket) {
        if (config.sock_type == SocketType::Unix) {
            LogInfo("Listening on socket '%!..+%1%!0' (Unix stack)", config.unix_path);
        } else {
            LogInfo("Listening on %!..+http://localhost:%1/%!0 (%2 stack)",
                    config.port, SocketTypeNames[(int)config.sock_type]);
        }
    }

    return true;
}

void http_Daemon::Stop()
{
    // Shut everything down
#if defined(_WIN32)
    shutdown(listen_fd, SD_RECEIVE);
#else
    shutdown(listen_fd, SHUT_RD);
#endif

#if defined(_WIN32) || defined(__APPLE__)
    // On Windows and macOS, the shutdown() does not wake up poll()
    for (Dispatcher *dispatcher: dispatchers) {
        dispatcher->Stop();
    }
#endif

    async.Sync();

    for (Dispatcher *dispatcher: dispatchers) {
        delete dispatcher;
    }
    dispatchers.Clear();

    CloseSocket(listen_fd);
    listen_fd = -1;

    handle_func = {};
}

bool http_Daemon::Dispatcher::Run()
{
#if defined(__linux__) || defined(__FreeBSD__)
    RG_ASSERT(event_fd < 0);
#else
    RG_ASSERT(pair_fd[0] < 0);
#endif

    int listen_fd = daemon->listen_fd;
    http_ClientAddressMode addr_mode = daemon->addr_mode;

    Async async(1 + http_WorkersPerHandler);

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
    if (!CreateSocketPair(pair_fd))
        return false;
    RG_DEFER {
        CloseSocket(pair_fd[0]);
        CloseSocket(pair_fd[1]);

        pair_fd[0] = -1;
        pair_fd[1] = -1;
    };
#endif

    // Delete remaining clients when function exits
    RG_DEFER {
        async.Sync();

        for (const http_IO *client: clients) {
            delete client;
        };
        for (const http_IO *client: pool) {
            delete client;
        }

        clients.Clear();
        pool.Clear();
    };

    HeapArray<struct pollfd> pfds = {
#if defined(__linux__) || defined(__FreeBSD__)
        { listen_fd, POLLIN, 0 },
        { event_fd, POLLIN, 0 }
#elif defined(_WIN32)
        { (SOCKET)listen_fd, POLLIN, 0 },
        { (SOCKET)pair_fd[0], POLLIN, 0 }
#else
        { listen_fd, POLLIN, 0 },
        { pair_fd[0], POLLIN, 0 }
#endif
    };

    int next_worker = 0;
    int64_t busy = 0;

    for (;;) {
        int64_t now = GetMonotonicTime();

        pfds.RemoveFrom(2);

        if (pfds[0].revents & POLLHUP)
            return true;
#if defined(_WIN32) || defined(__APPLE__)
        if (!run)
            return true;
#endif

        if (pfds[0].revents & POLLIN) {
            sockaddr_storage ss;
            socklen_t ss_len = RG_SIZE(ss);

            // Accept queued clients
            for (Size i = 0; i < 64; i++) {
#if defined(_WIN32)
                SOCKET sock = accept((SOCKET)listen_fd, (sockaddr *)&ss, &ss_len);
                int fd = (sock != INVALID_SOCKET) ? (int)sock : -1;

                if (fd >= 0) {
                    SetSocketNonBlock(fd, true);
                }
#elif defined(SOCK_CLOEXEC)
                int fd = accept4(listen_fd, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);

#if defined(TCP_NOPUSH) && !defined(TCP_CORK)
                if (fd >= 0) {
                    // Disable Nagle algorithm on platforms with better options
                    int flag = 1;
                    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                }
#endif
#else
                int fd = accept(listen_fd, (sockaddr *)&ss, &ss_len);

                if (fd >= 0) {
                    fcntl(fd, F_SETFD, FD_CLOEXEC);
                    SetSocketNonBlock(fd, true);
                }
#endif

                if (fd < 0) {
#if defined(_WIN32)
                    errno = TranslateWinSockError();
#endif

                    if (errno == EINVAL)
                        return true;
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    LogError("Failed to accept client: %1 %2", strerror(errno), errno);
                    return false;
                }

                http_IO *client = CreateClient(fd, now, (sockaddr *)&ss);
                clients.Append(client);

                busy = now;
            }
        }

        // Clear eventfd
        if (pfds[1].revents & POLLIN) {
#if defined(__linux__) || defined(__FreeBSD__)
            uint64_t dummy = 0;
            Size ret = read(event_fd, &dummy, RG_SIZE(dummy));
            (void)ret;
#else
            char buf[1024];
            Size ret = recv(pair_fd[0], buf, RG_SIZE(buf), 0);
            (void)ret;
#endif
        }

        Size keep = 0;
        for (Size i = 0; i < clients.len; i++, keep++) {
            clients[keep] = clients[i];

            http_IO *client = clients[i];
            http_IO::PrepareStatus ret = client->Prepare();

            switch (ret) {
                case http_IO::PrepareStatus::Waiting: {
#if defined(_WIN32)
                    struct pollfd pfd = { (SOCKET)client->Descriptor(), POLLIN, 0 };
                    pfds.Append(pfd);
#else
                    struct pollfd pfd = { client->Descriptor(), POLLIN, 0 };
                    pfds.Append(pfd);
#endif
                } break;

                case http_IO::PrepareStatus::Ready: {
                    if (!client->InitAddress(addr_mode)) {
                        client->request.keepalive = false;
                        client->SendError(400);
                        client->Close();

                        break;
                    }

                    client->timeout = http_KeepAliveDelay - (now - client->start);
                    client->request.keepalive &= (client->timeout >= 0);

                    int worker_idx = 1 + next_worker;
                    next_worker = (next_worker + 1) % http_WorkersPerHandler;

                    if (client->request.keepalive) {
                        async.Run(worker_idx, [=, this] {
                            daemon->RunHandler(client);

                            client->Reset();
                            Wake();

                            return true;
                        });
                    } else {
                        async.Run(worker_idx, [=, this] {
                            daemon->RunHandler(client);
                            client->Close();

                            return true;
                        });
                    }
                } break;

                case http_IO::PrepareStatus::Busy: {} break;

                case http_IO::PrepareStatus::Error: {
                    if (!client->response.sent) {
                        client->SendError(400);
                    }
                    client->Close();
                } break;

                case http_IO::PrepareStatus::Closed: {
                    DestroyClient(client);
                    keep--;
                } break;
            }
        }
        clients.len = keep;

        if (now - busy > http_PollAfterIdle) {
            // Wake me up from the kernel if needed
            {
                std::lock_guard<std::shared_mutex> lock_excl(wake_mutex);
                wake_needed = true;
            }

#if defined(_WIN32)
            if (WSAPoll(pfds.ptr, (unsigned long)pfds.len, -1) < 0) {
                errno = TranslateWinSockError();

                LogError("Failed to poll descriptors: %1", strerror(errno));
                return false;
            }
#else
            if (RG_RESTART_EINTR(poll(pfds.ptr, pfds.len, -1), < 0) < 0) {
                LogError("Failed to poll descriptors: %1", strerror(errno));
                return false;
            }
#endif

            busy = GetMonotonicTime();
        }
    }

    RG_UNREACHABLE();
}

void http_Daemon::Dispatcher::Wake()
{
    std::shared_lock<std::shared_mutex> lock_shr(wake_mutex);

    if (wake_needed) {
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

        wake_needed = false;
    }
}

#if defined(_WIN32) || defined(__APPLE__)

void http_Daemon::Dispatcher::Stop()
{
    run = false;
    Wake();
}

#endif

http_IO *http_Daemon::Dispatcher::CreateClient(int fd, int64_t start, struct sockaddr *sa)
{
    http_IO *client = nullptr;

    if (pool.len) {
        int idx = GetRandomInt(0, pool.len);

        client = pool[idx];

        std::swap(pool[idx], pool[pool.len - 1]);
        pool.len--;
    } else {
        client = new http_IO();
    }

    if (!client->Init(fd, start, sa)) [[unlikely]] {
        delete client;
        return nullptr;
    }

    return client;
}

void http_Daemon::Dispatcher::DestroyClient(http_IO *client)
{
    if (pool.Available()) {
        pool.Append(client);
        client->Reset();
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
    SetSocketNonBlock(fd, false);
    RG_DEFER { SetSocketNonBlock(fd, true); };
#endif
#if !defined(__linux__)
    // On Linux, use MSG_MORE in send() calls to set TCP_CORK without additional syscall
    SetSocketPush(fd, false);
#endif

    RG_DEFER { 
        response.sent = true;
        SetSocketPush(fd, true);
    };

    const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };
    StreamWriter writer(write, "<http>");

    LocalArray<char, 32768> intro;

    const char *protocol = (request.version == 11) ? "HTTP/1.1" : "HTTP/1.0";
    const char *details = http_ErrorMessages.FindValue(status, "Unknown");

    if (request.keepalive) {
        intro.len += Fmt(intro.TakeAvailable(), "%1 %2 %3\r\n"
                                                "Connection: keep-alive\r\n"
                                                "Keep-Alive: timeout=%4\r\n",
                         protocol, status, details, timeout / 1000).len;
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

        request.keepalive &= func(fd, &writer);
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
#elif defined(_WIN32)
    Send(status, len, [&](int sock, StreamWriter *) {
        HANDLE h = (HANDLE)_get_osfhandle(fd);

        if (len) {
            for (;;) {
                DWORD send = (DWORD)std::min(len, (Size)UINT32_MAX);
                BOOL success = TransmitFile((SOCKET)sock, h, send, 0, nullptr, nullptr, 0);

                if (!success) {
                    LogError("Failed to send file: %1", strerror(TranslateWinSockError()));
                    return false;
                }

                len -= (Size)send;
                if (!len)
                    break;

                if (!SetFilePointerEx(h, { .QuadPart = send }, nullptr, FILE_CURRENT)) {
                    LogError("Failed to send file: %1", GetWin32ErrorString());
                    return false;
                }
            }
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

http_IO::PrepareStatus http_IO::Prepare()
{
    if (ready)
        return PrepareStatus::Busy;
    if (fd < 0)
        return PrepareStatus::Closed;

    // Gather request line and headers
    {
        bool complete = false;

        incoming.buf.Grow(Mebibytes(1));

        for (;;) {
            Size available = incoming.buf.Available() - 1;

#if defined(_WIN32)
            Size read = recv((SOCKET)fd, (char *)incoming.buf.end(), (int)available, 0);
            errno = TranslateWinSockError();
#elif defined(MSG_DONTWAIT)
            Size read = recv(fd, incoming.buf.end(), available, MSG_DONTWAIT);
#else
            Size read = recv(fd, incoming.buf.end(), available, 0);
#endif

            incoming.buf.len += std::max(read, (Size)0);
            incoming.buf.ptr[incoming.buf.len] = 0;

            while (incoming.buf.len - incoming.pos >= 4) {
                uint8_t *next = (uint8_t *)memchr(incoming.buf.ptr + incoming.pos, '\r', incoming.buf.len - incoming.pos);
                incoming.pos = next ? next - incoming.buf.ptr : incoming.buf.len;

                if (incoming.pos >= http_MaxRequestSize) [[unlikely]] {
                    LogError("Excessive request size");
                    SendError(413);
                    return PrepareStatus::Error;
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
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return PrepareStatus::Waiting;

                // This probably never happens (non-blocking read) but who knows
                if (errno == EINTR)
                    continue;

                LogError("Read failed: %1", strerror(errno));
                return PrepareStatus::Error;
            } else if (!read) {
                if (!incoming.buf.len) {
                    Close();
                    return PrepareStatus::Closed;
                }

                LogError("Malformed or truncated HTTP request");
                return PrepareStatus::Error;
            }
        }

        RG_ASSERT(complete);
    }

    if (!ParseRequest(incoming.intro))
        return PrepareStatus::Error;

    ready = true;
    return PrepareStatus::Ready;
}

bool http_IO::WriteDirect(Span<const uint8_t> data)
{
    while (data.len) {
#if defined(_WIN32)
        int len = (int)std::min(data.len, (Size)INT_MAX);
        Size sent = send((SOCKET)fd, (const char *)data.ptr, len, 0);
#elif defined(__linux__)
        Size sent = send(fd, data.ptr, (size_t)data.len, MSG_MORE | MSG_NOSIGNAL);
#else
        Size sent = send(fd, data.ptr, (size_t)data.len, MSG_NOSIGNAL);
#endif

        if (sent < 0) {
            if (errno == EINTR)
                continue;

#if defined(_WIN32)
            errno = TranslateWinSockError();
#endif

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
#if defined(_WIN32)
            int len = (int)std::min(remain.len, (Size)INT_MAX);
            Size sent = send((SOCKET)fd, (const char *)remain.ptr, len, 0);
#elif defined(__linux__)
            Size sent = send(fd, remain.ptr, (size_t)remain.len, MSG_MORE | MSG_NOSIGNAL);
#else
            Size sent = send(fd, remain.ptr, (size_t)remain.len, MSG_NOSIGNAL);
#endif

            if (sent < 0) {
                if (errno == EINTR)
                    continue;

#if defined(_WIN32)
                errno = TranslateWinSockError();
#endif

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
