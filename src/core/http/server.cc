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
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif
#if defined(__linux__)
    #include <sys/sendfile.h>
    #include <sys/eventfd.h>
#elif defined(__FreeBSD__)
    #include <sys/uio.h>
#endif

namespace RG {

class http_Daemon::RequestHandler {
    http_Daemon *daemon;

#if defined(__linux__)
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
    RequestHandler(http_Daemon *daemon) : daemon(daemon) {}

    bool Run();
    void Wake();

#if defined(_WIN32) || defined(__APPLE__)
    void Stop();
#endif

private:
    http_IO *CreateClient(int fd, int64_t start, struct sockaddr *sa);
    void DestroyClient(http_IO *client);
};

static const int KeepAliveDelay = 5000;
static const int WorkersPerHandler = 4;
static const int PollAfterIdle = 1000;

static RG_CONSTINIT ConstMap<128, int, const char *> ErrorMessages = {
    { 100, "Continue" },
    { 101, "Switching Protocols" },
    { 102, "Processing" },
    { 103, "Early Hints" },
    { 200, "OK" },
    { 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-Authoritative Information" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 207, "Multi-Status" },
    { 208, "Already Reported" },
    { 226, "IM Used" },
    { 300, "Multiple Choices" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },
    { 306, "Switch Proxy" },
    { 307, "Temporary Redirect" },
    { 308, "Permanent Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment Required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Timeout" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Content Too Large" },
    { 414, "URI Too Long" },
    { 415, "Unsupported Media Type" },
    { 416, "Range Not Satisfiable" },
    { 417, "Expectation Failed" },
    { 421, "Misdirected Request" },
    { 422, "Unprocessable Content" },
    { 423, "Locked" },
    { 424, "Failed Dependency" },
    { 425, "Too Early" },
    { 426, "Upgrade Required" },
    { 428, "Precondition Required" },
    { 429, "Too Many Requests" },
    { 431, "Request Header Fields Too Large" },
    { 449, "Reply With" },
    { 450, "Blocked by Windows Parental Controls" },
    { 451, "Unavailable For Legal Reasons" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" },
    { 504, "Gateway Timeout" },
    { 505, "HTTP Version Not Supported" },
    { 506, "Variant Also Negotiates" },
    { 507, "Insufficient Storage" },
    { 508, "Loop Detected" },
    { 509, "Bandwidth Limit Exceeded" },
    { 510, "Not Extended" },
    { 511, "Network Authentication Required" }
};

static void SetSocketNonBlock(int fd, bool enable)
{
#if defined(_WIN32)
    unsigned long mode = enable;
    ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= ApplyMask(flags, O_NONBLOCK, enable);
    fcntl(fd, F_SETFL, flags);
#endif
}

void SetSocketCork(int fd, bool cork)
{
#if defined(TCP_CORK)
    int flag = cork;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));
#elif defined(TCP_NOPUSH)
    int flag = cork;
    setsockopt(fd, IPPROTO_TCP, TCP_NOPUSH, &flag, sizeof(flag));
#elif defined(_WIN32)
    int flag = !cork;
    setsockopt((SOCKET)fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    send((SOCKET)fd, nullptr, 0, 0);
#else
    int flag = !cork;
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

#elif !defined(__linux__)

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

bool http_Config::SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory)
{
    if (key == "SocketType" || key == "IPStack") {
        if (!OptionToEnumI(SocketTypeNames, value, &sock_type)) {
            LogError("Unknown socket type '%1'", value);
            return false;
        }

        return true;
    } else if (key == "UnixPath") {
        unix_path = NormalizePath(value, root_directory, &str_alloc).ptr;
        return true;
    } else if (key == "Port") {
        return ParseInt(value, &port);
    } else if (key == "ClientAddress") {
        if (!OptionToEnumI(http_ClientAddressModeNames, value, &addr_mode)) {
            LogError("Unknown client address mode '%1'", value);
            return false;
        }

        return true;
    }

    LogError("Unknown HTTP property '%1'", key);
    return false;
}

bool http_Config::SetPortOrPath(Span<const char> str)
{
    if (std::all_of(str.begin(), str.end(), IsAsciiDigit)) {
        int new_port;
        if (!ParseInt(str, &new_port))
            return false;

        if (new_port <= 0 || port > UINT16_MAX) {
            LogError("HTTP port %1 is invalid (range: 1 - %2)", port, UINT16_MAX);
            return false;
        }

        if (sock_type != SocketType::IPv4 && sock_type != SocketType::IPv6 && sock_type != SocketType::Dual) {
            sock_type = SocketType::Dual;
        }
        port = new_port;
    } else {
        sock_type = SocketType::Unix;
        unix_path = NormalizePath(str, &str_alloc).ptr;
    }

    return true;
}

bool http_Config::Validate() const
{
    bool valid = true;

    if (sock_type == SocketType::Unix) {
        struct sockaddr_un addr;

        if (!unix_path) {
            LogError("Unix socket path must be set");
            valid = false;
        } else if (strlen(unix_path) >= sizeof(addr.sun_path)) {
            LogError("Socket path '%1' is too long (max length = %2)", unix_path, sizeof(addr.sun_path) - 1);
            valid = false;
        }
    } else if (port < 1 || port > UINT16_MAX) {
        LogError("HTTP port %1 is invalid (range: 1 - %2)", port, UINT16_MAX);
        valid = false;
    }

    return valid;
}

bool http_Daemon::Bind(const http_Config &config)
{
    RG_ASSERT(listen_fd < 0);

    // Validate configuration
    if (!config.Validate())
        return false;

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: { listen_fd = OpenIPSocket(config.sock_type, config.port); } break;
        case SocketType::Unix: { listen_fd = OpenUnixSocket(config.unix_path); } break;
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

    // Run request handlers
    for (Size i = 0; i < async.GetWorkerCount(); i++) {
        RequestHandler *handler = new RequestHandler(this);
        handlers.Append(handler);

        async.Run([=] { return handler->Run(); });
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
    for (RequestHandler *handler: handlers) {
        handler->Stop();
    }
#endif

    async.Sync();

    for (RequestHandler *handler: handlers) {
        delete handler;
    }
    handlers.Clear();

    CloseSocket(listen_fd);
    listen_fd = -1;

    handle_func = {};
}

bool http_Daemon::RequestHandler::Run()
{
#if defined(__linux__)
    RG_ASSERT(event_fd < 0);
#else
    RG_ASSERT(pair_fd[0] < 0);
#endif

    int listen_fd = daemon->listen_fd;
    http_ClientAddressMode addr_mode = daemon->addr_mode;

    Async async(1 + WorkersPerHandler);

#if defined(__linux__)
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
#if defined(__linux__)
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
                int fd = accept4(listen_fd, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC | SOCK_NONBLOCK);
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
#if defined(__linux__)
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
                        client->SendText(400, "Malformed HTTP request");
                        client->Close();

                        break;
                    }

                    client->timeout = KeepAliveDelay - (now - client->start);
                    client->request.keepalive &= (client->timeout >= 0);

                    int worker_idx = 1 + next_worker;
                    next_worker = (next_worker + 1) % WorkersPerHandler;

                    if (client->request.keepalive) {
                        async.Run(worker_idx, [=, this] {
                            daemon->handle_func(client->request, client);

                            client->Reset();
                            Wake();

                            return true;
                        });
                    } else {
                        async.Run(worker_idx, [=, this] {
                            daemon->handle_func(client->request, client);
                            client->Close();

                            return true;
                        });
                    }
                } break;

                case http_IO::PrepareStatus::Busy: {} break;

                case http_IO::PrepareStatus::Error: {
                    client->request.keepalive = false;
                    client->SendText(400, "Malformed request");
                    client->Close();
                } break;

                case http_IO::PrepareStatus::Closed: {
                    DestroyClient(client);
                    keep--;
                } break;
            }
        }
        clients.len = keep;

        if (now - busy > PollAfterIdle) {
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
            if (poll(pfds.ptr, pfds.len, -1) < 0) {
                LogError("Failed to poll descriptors: %1", strerror(errno));
                return false;
            }
#endif

            busy = GetMonotonicTime();
        }
    }

    RG_UNREACHABLE();
}

void http_Daemon::RequestHandler::Wake()
{
    std::shared_lock<std::shared_mutex> lock_shr(wake_mutex);

    if (wake_needed) {
        lock_shr.unlock();

#if defined(__linux__)
        uint64_t one = 1;
        Size ret = write(event_fd, &one, RG_SIZE(one));
        (void)ret;
#else
        char x = 'x';
        Size ret = send(pair_fd[1], &x, RG_SIZE(x), 0);
        (void)ret;
#endif

        wake_needed = false;
    }
}

#if defined(_WIN32) || defined(__APPLE__)

void http_Daemon::RequestHandler::Stop()
{
    run = false;
    Wake();
}

#endif

http_IO *http_Daemon::RequestHandler::CreateClient(int fd, int64_t start, struct sockaddr *sa)
{
    http_IO *client = nullptr;

    if (pool.len) {
        int idx = GetRandomInt(0, pool.len);

        client = pool[idx];

        std::swap(pool[idx], pool[pool.len - 1]);
        pool.len--;
    } else {
        client = new http_IO(this);
    }

    if (!client->Init(fd, start, sa)) [[unlikely]] {
        delete client;
        return nullptr;
    }

    return client;
}

void http_Daemon::RequestHandler::DestroyClient(http_IO *client)
{
    if (pool.Available()) {
        pool.Append(client);
        client->Reset();
    } else {
        delete client;
    }
}

const char *http_RequestInfo::FindHeader(const char *key) const
{
    for (const http_KeyValue &header: headers) {
        if (TestStrI(header.key, key))
            return header.value;
    }

    return nullptr;
}

const char *http_RequestInfo::FindGetValue(const char *) const
{
    LogDebug("Not implemented");
    return nullptr;
}

const char *http_RequestInfo::FindCookie(const char *) const
{
    LogDebug("Not implemented");
    return nullptr;
}

void http_IO::AddHeader(Span<const char> key, Span<const char> value)
{
    http_KeyValue header = {};

    header.key = DuplicateString(key, &allocator).ptr;
    header.value = DuplicateString(value, &allocator).ptr;

    response.headers.Append(header);
}

void http_IO::AddEncodingHeader(CompressionType encoding)
{
    switch (encoding) {
        case CompressionType::None: {} break;
        case CompressionType::Zlib: { AddHeader("Content-Encoding", "deflate"); } break;
        case CompressionType::Gzip: { AddHeader("Content-Encoding", "gzip"); } break;
        case CompressionType::Brotli: { AddHeader("Content-Encoding", "br"); } break;
        case CompressionType::LZ4: { RG_UNREACHABLE(); } break;
        case CompressionType::Zstd: { AddHeader("Content-Encoding", "zstd"); } break;
    }
}

void http_IO::AddCookieHeader(const char *path, const char *name, const char *value, bool http_only)
{
    LocalArray<char, 1024> buf;

    if (value) {
        buf.len = Fmt(buf.data, "%1=%2; Path=%3;", name, value, path).len;
    } else {
        buf.len = Fmt(buf.data, "%1=; Path=%2; Max-Age=0;", name, path).len;
    }

    RG_ASSERT(buf.Available() >= 64);
    buf.len += Fmt(buf.TakeAvailable(), " SameSite=Strict;%1", http_only ? " HttpOnly;" : "").len;

    AddHeader("Set-Cookie", buf.data);
}

void http_IO::AddCachingHeaders(int64_t max_age, const char *etag)
{
    RG_ASSERT(max_age >= 0);

#if defined(RG_DEBUG)
    max_age = 0;
#endif

    if (max_age || etag) {
        char buf[128];

        AddHeader("Cache-Control", max_age ? Fmt(buf, "max-age=%1", max_age / 1000).ptr : "no-store");
        if (etag) {
            AddHeader("ETag", etag);
        }
    } else {
        AddHeader("Cache-Control", "no-store");
    }
}

bool http_IO::NegociateEncoding(CompressionType preferred, CompressionType *out_encoding)
{
    const char *accept_str = request.FindHeader("Accept-Encoding");
    uint32_t acceptable_encodings = http_ParseAcceptableEncodings(accept_str);

    if (acceptable_encodings & (1 << (int)preferred)) {
        *out_encoding = preferred;
        return true;
    } else if (acceptable_encodings) {
        int clz = 31 - CountLeadingZeros(acceptable_encodings);
        *out_encoding = (CompressionType)clz;

        return true;
    } else {
        SendError(406);
        return false;
    }
}

bool http_IO::NegociateEncoding(CompressionType preferred1, CompressionType preferred2, CompressionType *out_encoding)
{
    const char *accept_str = request.FindHeader("Accept-Encoding");
    uint32_t acceptable_encodings = http_ParseAcceptableEncodings(accept_str);

    if (acceptable_encodings & (1 << (int)preferred1)) {
        *out_encoding = preferred1;
        return true;
    } else if (acceptable_encodings & (1 << (int)preferred2)) {
        *out_encoding = preferred2;
        return true;
    } else if (acceptable_encodings) {
        int clz = 31 - CountLeadingZeros(acceptable_encodings);
        *out_encoding = (CompressionType)clz;

        return true;
    } else {
        SendError(406);
        return false;
    }
}

void http_IO::Send(int status, int64_t len, FunctionRef<bool(int, StreamWriter *)> func)
{
    RG_ASSERT(!response.sent);

    if (request.headers_only) {
        func = [](int, StreamWriter *) { return true; };
    }

    SetSocketNonBlock(fd, false);
#if !defined(__linux__)
    // On Linux with use MSG_MORE in send() calls to skip one syscall
    SetSocketCork(fd, true);
#endif

    RG_DEFER { 
        response.sent = true;
        SetSocketCork(fd, false);
    };

    const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };
    StreamWriter writer(write, "<http>");

    LocalArray<char, 32768> intro;

    const char *protocol = (request.version == 11) ? "HTTP/1.1" : "HTTP/1.0";
    const char *details = ErrorMessages.FindValue(status, "Unknown");

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
        StreamWriter chunker(chunk, "<http>");

        if (func(-1, &chunker)) {
            writer.Write("0\r\n\r\n");
        } else {
            request.keepalive = false;
        }
    }

    request.keepalive &= writer.IsValid();
}

void http_IO::SendText(int status, Span<const char> text, const char *mimetype)
{
    RG_ASSERT(mimetype);
    AddHeader("Content-Type", mimetype);

    Send(status, text.len, [&](int, StreamWriter *writer) { return writer->Write(text); });
}

void http_IO::SendBinary(int status, Span<const uint8_t> data, const char *mimetype)
{
    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }

    Send(status, data.len, [&](int, StreamWriter *writer) { return writer->Write(data); });
}

void http_IO::SendError(int status, const char *details)
{
    const char *last_err = nullptr; // XXX

    if (!details) {
        details = (status < 500 && last_err) ? last_err : "";
    }

    Span<char> text = Fmt(&allocator, "Error %1: %2\n%3", status,
                          ErrorMessages.FindValue(status, "Unknown"), details);
    return SendText(status, text);
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

#if defined(__linux__) || defined(__FreeBSD__)
    Send(status, len, [&](int sock, StreamWriter *) {
        off_t offset = 0;
        int64_t remain = len;

        while (remain) {
            Size send = (Size)std::min(remain, (int64_t)RG_SIZE_MAX);

#if defined(__linux__)
            Size sent = sendfile(sock, fd, &offset, (size_t)send);
#else
            Size sent = sendfile(fd, sock, offset, (size_t)send, nullptr, &offset, 0);
#endif

            if (sent < 0) {
                if (errno != EPIPE) {
                    LogError("Failed to send file: %1", strerror(errno));
                }
                return false;
            }

            remain -= sent;
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

void http_IO::SendEmpty(int status)
{
    Send(status, 0, [](int, StreamWriter *) { return true; });
}

void http_IO::AddFinalizer(const std::function<void()> &func)
{
    RG_ASSERT(!response.sent);
    response.finalizers.Append(func);
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

        incoming.buf.Grow(Mebibytes(2));

        for (;;) {
#if defined(_WIN32)
            Size read = recv((SOCKET)fd, (char *)incoming.buf.end(), (int)incoming.buf.Available(), 0);
            errno = TranslateWinSockError();
#else
            Size read = recv(fd, incoming.buf.end(), incoming.buf.Available(), 0);
#endif

            // We'll need it later
            int error = errno;

            incoming.buf.len += std::max(read, (Size)0);

            for (;;) {
                uint8_t *next = (uint8_t *)memchr(incoming.buf.ptr + incoming.pos, '\r', incoming.buf.len - incoming.pos);
                incoming.pos = next ? next - incoming.buf.ptr : incoming.buf.len;

                if (incoming.buf.len - incoming.pos < 4)
                    break;

                if (incoming.buf[incoming.pos] == '\r' && incoming.buf[incoming.pos + 1] == '\n' &&
                        incoming.buf[incoming.pos + 2] == '\r' && incoming.buf[incoming.pos + 3] == '\n') {
                    incoming.pos += 4;
                    complete = true;

                    break;
                }

                incoming.pos++;
            }
            if (complete)
                break;

            if (read < 0) {
                if (error == EAGAIN || error == EWOULDBLOCK)
                    return PrepareStatus::Waiting;
                if (error == ECONNRESET)
                    return PrepareStatus::Error;

                LogError("Read failed: %1", strerror(error));
                return PrepareStatus::Error;
            } else if (!read) {
                if (!incoming.buf.len) {
                    Close();
                    return PrepareStatus::Closed;
                }

                LogError("Malformed HTTP request");
                return PrepareStatus::Error;
            }
        }

        RG_ASSERT(complete);

        incoming.intro = incoming.buf.As<char>().Take(0, incoming.pos - 4);
        incoming.extra = MakeSpan(incoming.buf.ptr + incoming.pos, incoming.buf.len - incoming.pos);
    }

    Span<char> remain = incoming.intro;

    // Parse request line
    {
        Span<char> line = SplitStrLine(remain, &remain);

        Span<char> method = SplitStr(line, ' ', &line);
        Span<char> url = SplitStr(line, ' ', &line);
        Span<char> protocol = SplitStr(line, ' ', &line);

        for (char &c: method) {
            c = UpperAscii(c);
        }

        if (!method.len) {
            SendError(400, "Empty HTTP method");
            return PrepareStatus::Error;
        }
        if (!StartsWith(url, "/")) {
            SendError(400, "Invalid request URL");
            return PrepareStatus::Error;
        }
        if (TestStrI(protocol, "HTTP/1.0")) {
            request.version = 10;
            request.keepalive = false;
        } else if (TestStrI(protocol, "HTTP/1.1")) {
            request.version = 11;
            request.keepalive = true;
        } else {
            SendError(400, "Invalid HTTP version");
            return PrepareStatus::Error;
        }
        if (line.len) {
            SendError(400, "Unexpected data after request line");
            return PrepareStatus::Error;
        }

        if (TestStr(method, "HEAD")) {
            request.method = http_RequestMethod::Get;
            request.headers_only = true;
        } else if (OptionToEnumI(http_RequestMethodNames, method, &request.method)) {
            request.headers_only = false;
        } else {
            SendError(405);
            return PrepareStatus::Error;
        }
        request.client_addr = addr;

        Span<char> query;
        url = SplitStr(url, '?', &query);

        url.ptr[url.len] = 0;
        request.url = url.ptr;
    }

    // Parse headers
    while (remain.len) {
        http_KeyValue header = {};

        Span<char> line = SplitStrLine(remain, &remain);

        Span<char> key = TrimStrRight(SplitStr(line, ':', &line));
        if (line.ptr == key.end()) {
            LogError("Malformed header line");
            return PrepareStatus::Error;
        }
        Span<char> value = TrimStr(line);

        bool upper = true;
        for (char &c: key.As<char>()) {
            c = upper ? UpperAscii(c) : LowerAscii(c);
            upper = (c == '-');
        }

        key.ptr[key.len] = 0;
        value.ptr[value.len] = 0;

        header.key = key.ptr;
        header.value = value.ptr;

        request.headers.Append(header);

        if (TestStrI(key, "Connection")) {
            request.keepalive = !TestStrI(value, "close");
        }
    }

    ready = true;
    return PrepareStatus::Ready;
}

bool http_IO::Init(int fd, int64_t start, struct sockaddr *sa)
{
    this->fd = fd;

    int family = sa->sa_family;
    void *ptr = nullptr;

    switch (family) {
        case AF_INET: { ptr = &((sockaddr_in *)sa)->sin_addr; } break;
        case AF_INET6: { ptr = &((sockaddr_in6 *)sa)->sin6_addr; } break;
        case AF_UNIX: {
            CopyString("unix", addr);
            return true;
        } break;

        default: { RG_UNREACHABLE(); } break;
    }

    if (!inet_ntop(family, ptr, addr, RG_SIZE(addr))) {
        LogError("Cannot convert network address to text");
        return false;
    }

    this->start = start;
    this->timeout = KeepAliveDelay;

    return true;
}

bool http_IO::InitAddress(http_ClientAddressMode addr_mode)
{
    switch (addr_mode) {
        case http_ClientAddressMode::Socket: { /* Keep socket address */ } break;

        case http_ClientAddressMode::XForwardedFor: {
            const char *str = request.FindHeader("X-Forwarded-For");
            if (!str) {
                LogError("X-Forwarded-For header is missing but is required by the configuration");
                return false;
            }

            Span<const char> trimmed = TrimStr(SplitStr(str, ','));

            if (!trimmed.len) [[unlikely]] {
                LogError("Empty client address in X-Forwarded-For header");
                return false;
            }
            if (!CopyString(trimmed, addr)) [[unlikely]] {
                LogError("Excessively long client address in X-Forwarded-For header");
                return false;
            }
        } break;

        case http_ClientAddressMode::XRealIP: {
            const char *str = request.FindHeader("X-Real-IP");
            if (!str) {
                LogError("X-Real-IP header is missing but is required by the configuration");
                return false;
            }

            Span<const char> trimmed = TrimStr(str);

            if (!trimmed.len) [[unlikely]] {
                LogError("Empty client address in X-Forwarded-For header");
                return false;
            }
            if (!CopyString(trimmed, addr)) [[unlikely]] {
                LogError("Excessively long client address in X-Forwarded-For header");
                return false;
            }
        } break;
    }

    return true;
}

bool http_IO::WriteDirect(Span<const uint8_t> data)
{
    while (data.len) {
#if defined(__linux__)
        Size sent = send(fd, data.ptr, (size_t)data.len, MSG_MORE | MSG_NOSIGNAL);
#elif defined(_WIN32)
        int len = (int)std::min(data.len, (Size)INT_MAX);
        Size sent = send((SOCKET)fd, (const char *)data.ptr, len, 0);
#else
        Size sent = send(fd, data.ptr, (size_t)data.len, MSG_NOSIGNAL);
#endif

        if (sent < 0) {
#if defined(_WIN32)
            errno = TranslateWinSockError();
#endif

            if (errno != EPIPE) {
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
#if defined(__linux__)
            Size sent = send(fd, remain.ptr, (size_t)remain.len, MSG_MORE | MSG_NOSIGNAL);
#elif defined(_WIN32)
            int len = (int)std::min(remain.len, (Size)INT_MAX);
            Size sent = send((SOCKET)fd, (const char *)remain.ptr, len, 0);
#else
            Size sent = send(fd, remain.ptr, (size_t)remain.len, MSG_NOSIGNAL);
#endif

            if (sent < 0) {
#if defined(_WIN32)
                errno = TranslateWinSockError();
#endif

                if (errno != EPIPE) {
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

void http_IO::Reset()
{
    for (const auto &finalize: response.finalizers) {
        finalize();
    }

    MemMove(incoming.buf.ptr, incoming.extra.ptr, incoming.extra.len);
    incoming.buf.RemoveFrom(incoming.extra.len);
    incoming.pos = 0;
    incoming.intro = {};
    incoming.extra = {};

    allocator.Reset();

    request.headers.RemoveFrom(0);
    response.headers.RemoveFrom(0);
    response.finalizers.RemoveFrom(0);
    response.sent = false;

    ready = false;
}

void http_IO::Close()
{
    for (const auto &finalize: response.finalizers) {
        finalize();
    }

    CloseSocket(fd);
    fd = -1;

    ready = false;
}

}
