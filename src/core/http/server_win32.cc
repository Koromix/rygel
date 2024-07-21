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

#if defined(_WIN32)

#include "src/core/base/base.hh"
#include "server.hh"
#include "misc.hh"

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

#if !defined(UNIX_PATH_MAX)
    #define UNIX_PATH_MAX 108
#endif
typedef struct sockaddr_un {
    ADDRESS_FAMILY sun_family;
    char sun_path[UNIX_PATH_MAX];
} SOCKADDR_UN, *PSOCKADDR_UN;

namespace RG {

static const int WorkersPerDispatcher = 4;

class http_Daemon::Dispatcher {
    http_Daemon *daemon;

    int pair_fd[2] = { -1, -1 };
    std::shared_mutex wake_mutex;
    bool wake_up = false;
    bool wake_interrupt = false;

    std::atomic_bool run { true };

    HeapArray<http_IO *> clients;
    LocalArray<http_IO *, 256> pool;

public:
    Dispatcher(http_Daemon *daemon) : daemon(daemon) {}

    bool Run();
    void Wake();

    void Stop();

private:
    http_IO *CreateClient(int fd, int64_t start, struct sockaddr *sa);
    void DestroyClient(http_IO *client);
};

static void SetSocketNonBlock(int fd, bool enable)
{
    unsigned long mode = enable;
    ioctlsocket(fd, FIONBIO, &mode);
}

void SetSocketPush(int fd, bool push)
{
    int flag = push;
    setsockopt((SOCKET)fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    send((SOCKET)fd, nullptr, 0, 0);
}

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

bool http_Daemon::Bind(const http_Config &config, bool log_addr)
{
    RG_ASSERT(listen_fd < 0);

    if (!InitConfig(config))
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
        errno = TranslateWinSockError();

        LogError("Failed to listen on socket: %1", strerror(errno));
        return false;
    }

    SetSocketNonBlock(listen_fd, true);

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

void http_Daemon::Start(std::function<void(const http_RequestInfo &request, http_IO *io)> func)
{
    RG_ASSERT(listen_fd >= 0);
    RG_ASSERT(!handle_func);
    RG_ASSERT(func);

    handle_func = func;

    // Run request dispatchers
    for (Size i = 0; i < async.GetWorkerCount(); i++) {
        Dispatcher *dispatcher = new Dispatcher(this);
        dispatchers.Append(dispatcher);

        async.Run([=] { return dispatcher->Run(); });
    }
}

void http_Daemon::Stop()
{
    // Shut everything down
    shutdown(listen_fd, SD_RECEIVE);

    // On Windows, the shutdown() does not wake up poll()
    for (Dispatcher *dispatcher: dispatchers) {
        dispatcher->Stop();
    }

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
    RG_ASSERT(pair_fd[0] < 0);

    Async async(1 + WorkersPerDispatcher);

    if (!CreateSocketPair(pair_fd))
        return false;
    RG_DEFER {
        CloseSocket(pair_fd[0]);
        CloseSocket(pair_fd[1]);

        pair_fd[0] = -1;
        pair_fd[1] = -1;
    };

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
        { (SOCKET)daemon->listen_fd, POLLIN, 0 },
        { (SOCKET)pair_fd[0], POLLIN, 0 }
    };

    int next_worker = 0;

    for (;;) {
        int64_t now = GetMonotonicTime();

        pfds.len = 2;

        if (pfds[0].revents & POLLHUP)
            return true;
        if (!run)
            return true;

        if (pfds[0].revents & POLLIN) {
            sockaddr_storage ss;
            socklen_t ss_len = RG_SIZE(ss);

            // Accept queued clients
            for (Size i = 0; i < 64; i++) {
                SOCKET sock = accept((SOCKET)daemon->listen_fd, (sockaddr *)&ss, &ss_len);
                int fd = (sock != INVALID_SOCKET) ? (int)sock : -1;

                if (fd >= 0) {
                    SetSocketNonBlock(fd, true);
                }

                if (fd < 0) {
                    errno = TranslateWinSockError();

                    if (errno == EINVAL)
                        return true;
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    LogError("Failed to accept client: %1 %2", strerror(errno), errno);
                    return false;
                }

                http_IO *client = CreateClient(fd, now, (sockaddr *)&ss);
                clients.Append(client);
            }
        }

        // Clear eventfd
        if (pfds[1].revents & POLLIN) {
            char buf[4096];
            Size ret = recv(pair_fd[0], buf, RG_SIZE(buf), 0);
            (void)ret;
        }

        Size keep = 0;
        unsigned int timeout = UINT_MAX;

        // Process clients
        for (Size i = 0; i < clients.len; i++, keep++) {
            clients[keep] = clients[i];

            http_IO *client = clients[i];
            Size pfd_idx = client->pfd_idx;

            http_IO::PrepareStatus status = http_IO::PrepareStatus::Incoming;

            if (pfd_idx >= 0) {
                const struct pollfd &pfd = pfds.ptr[pfd_idx];

                if (pfd.revents) {
                    status = client->Prepare(now);
                }

                client->pfd_idx = -1;
            } else {
                status = client->Prepare(now);
            }

            switch (status) {
                case http_IO::PrepareStatus::Incoming: {
                    client->pfd_idx = pfds.len;

                    struct pollfd pfd = { (decltype(pollfd::fd))client->Descriptor(), POLLIN, 0 };
                    pfds.Append(pfd);

                    int64_t delay = std::max((int64_t)0, client->GetTimeout(now));
                    timeout = std::min(timeout, (unsigned int)delay);
                } break;

                case http_IO::PrepareStatus::Ready: {
                    if (!client->InitAddress()) {
                        client->request.keepalive = false;
                        client->SendError(400);
                        client->Close();

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
                            client->Close();

                            return true;
                        });
                    }
                } break;

                case http_IO::PrepareStatus::Busy: {} break;

                case http_IO::PrepareStatus::Close: {
                    client->Close();
                } [[fallthrough]];

                case http_IO::PrepareStatus::Unused: {
                    DestroyClient(client);
                    keep--;
                } break;
            }
        }
        clients.len = keep;

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
        int ready = WSAPoll(pfds.ptr, (unsigned long)pfds.len, (int)timeout);

        if (ready < 0) [[unlikely]] {
            errno = TranslateWinSockError();

            LogError("Failed to poll descriptors: %1", strerror(errno));
            return false;
        }

        if (!ready) {
            // Process everyone after a timeout
            for (http_IO *client: clients) {
                client->pfd_idx = -1;
            }
        }
    }

    RG_UNREACHABLE();
}

void http_Daemon::Dispatcher::Wake()
{
    std::shared_lock<std::shared_mutex> lock_shr(wake_mutex);

    wake_up = true;

    // Fast path (prevent poll)
    if (!wake_interrupt)
        return;

    lock_shr.unlock();

    char x = 'x';
    Size ret = RG_RESTART_EINTR(send(pair_fd[1], &x, RG_SIZE(x), 0), < 0);
    (void)ret;
}

void http_Daemon::Dispatcher::Stop()
{
    run = false;
    Wake();
}

http_IO *http_Daemon::Dispatcher::CreateClient(int fd, int64_t start, struct sockaddr *sa)
{
    http_IO *client = nullptr;

    if (pool.len) {
        int idx = GetRandomInt(0, pool.len);

        client = pool[idx];

        std::swap(pool[idx], pool[pool.len - 1]);
        pool.len--;
    } else {
        client = new http_IO(daemon);
    }

    if (!client->Init(fd, start, sa)) [[unlikely]] {
        delete client;
        return nullptr;
    }
    client->pfd_idx = -1;

    return client;
}

void http_Daemon::Dispatcher::DestroyClient(http_IO *client)
{
    if (pool.Available()) {
        pool.Append(client);
        client->Rearm(0);
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

    SetSocketNonBlock(fd, false);
    SetSocketPush(fd, false);

    RG_DEFER {
        response.sent = true;

        SetSocketNonBlock(fd, true);
        SetSocketPush(fd, true);
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

    return true;
}

http_IO::PrepareStatus http_IO::Prepare(int64_t now)
{
    if (ready)
        return PrepareStatus::Busy;
    if (fd < 0)
        return PrepareStatus::Unused;

    // Gather request line and headers
    {
        bool complete = false;

        incoming.buf.Grow(Mebibytes(1));

        for (;;) {
            Size available = incoming.buf.Available() - 1;

            Size read = recv((SOCKET)fd, (char *)incoming.buf.end(), (int)available, 0);

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
                errno = TranslateWinSockError();

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
        int len = (int)std::min(data.len, (Size)INT_MAX);
        Size sent = send((SOCKET)fd, (const char *)data.ptr, len, 0);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            errno = TranslateWinSockError();

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
            int len = (int)std::min(remain.len, (Size)INT_MAX);
            Size sent = send((SOCKET)fd, (const char *)remain.ptr, len, 0);

            if (sent < 0) {
                if (errno == EINTR)
                    continue;

                errno = TranslateWinSockError();

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
